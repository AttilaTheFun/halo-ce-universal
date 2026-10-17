#import "cloud_save.h"
#import "save_snapshot.h"
#import <CommonCrypto/CommonDigest.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static NSError *cloudFailure(NSString *message) {
    return [NSError errorWithDomain:@"HaloCloudSave" code:1 userInfo:@{NSLocalizedDescriptionKey:message}];
}
static NSData *readAsset(NSURL *url, NSError **error) {
    if (!url.isFileURL) { *error = cloudFailure(@"iCloud returned no saved-game file."); return nil; }
    int fd = open(url.fileSystemRepresentation, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    struct stat st;
    if (fd < 0 || fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_size <= 0 || st.st_size > 128 * 1024 * 1024) {
        if (fd >= 0) close(fd);
        *error = cloudFailure(@"The cloud save is unavailable or exceeds the size limit."); return nil;
    }
    NSMutableData *data = [NSMutableData dataWithLength:(NSUInteger)st.st_size];
    NSUInteger done = 0;
    while (done < data.length) {
        ssize_t n = read(fd, (char *)data.mutableBytes + done, data.length - done);
        if (n <= 0) break;
        done += n;
    }
    close(fd);
    if (done != data.length) { *error = cloudFailure(@"The cloud save download is incomplete."); return nil; }
    return data;
}
@interface HaloCloudSaveTransport ()
@property(nonatomic) id<HaloCloudDatabase> database;
@property(nonatomic) NSURL *stagingDirectory;
@end
@implementation HaloCloudSaveTransport
- (instancetype)initWithDatabase:(id<HaloCloudDatabase>)database stagingDirectory:(NSURL *)directory {
    if ((self = [super init])) { _database = database; _stagingDirectory = directory; }
    return self;
}
+ (CKRecordID *)headIDForCompatibility:(NSString *)compatibility {
    NSData *bytes = [compatibility dataUsingEncoding:NSUTF8StringEncoding];
    unsigned char hash[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(bytes.bytes, (CC_LONG)bytes.length, hash);
    NSMutableString *name = [NSMutableString stringWithString:@"progress-v1-"];
    for (NSUInteger i = 0; i < sizeof(hash); i++) [name appendFormat:@"%02x", hash[i]];
    return [[CKRecordID alloc] initWithRecordName:name];
}
- (void)fetchCompatibility:(NSString *)compatibility completion:(void (^)(CKRecord *, NSData *, NSError *))completion {
    if (!compatibility.length) { completion(nil, nil, cloudFailure(@"Missing game-data identity.")); return; }
    CKRecordID *recordID = [HaloCloudSaveTransport headIDForCompatibility:compatibility];
    [self.database fetchRecordWithID:recordID completionHandler:^(CKRecord *record, NSError *error) {
        if (error) {
            if ([error.domain isEqual:CKErrorDomain] && error.code == CKErrorUnknownItem) completion(nil, nil, nil);
            else completion(nil, nil, error);
            return;
        }
        if (![record.recordID isEqual:recordID] || ![record.recordType isEqual:@"HaloProgress"] ||
            ![record[@"compatibility"] isEqual:compatibility] || ![record[@"schema"] isEqual:@1] ||
            ![record[@"snapshot"] isKindOfClass:NSString.class] || ![record[@"payload"] isKindOfClass:CKAsset.class]) {
            completion(nil, nil, cloudFailure(@"The cloud save record has an unsupported format.")); return;
        }
        // CKAsset's temporary URL belongs to CloudKit. Read and validate it
        // during this callback rather than retaining a URL that may disappear.
        NSError *failure = nil;
        NSData *bytes = readAsset(((CKAsset *)record[@"payload"]).fileURL, &failure);
        NSDictionary *snapshot = bytes ? halo_save_snapshot_decode(bytes, compatibility, &failure) : nil;
        if (!snapshot || ![snapshot[@"snapshot"] isEqual:record[@"snapshot"]]) {
            completion(nil, nil, failure ?: cloudFailure(@"The cloud save metadata does not match its file.")); return;
        }
        completion(record, bytes, nil);
    }];
}
- (void)uploadSnapshot:(NSData *)snapshot compatibility:(NSString *)compatibility expectedHead:(CKRecord *)expected
           completion:(void (^)(CKRecord *, NSError *))completion {
    NSError *error = nil;
    NSDictionary *decoded = halo_save_snapshot_decode(snapshot, compatibility, &error);
    if (!decoded) { completion(nil, error); return; }
    CKRecordID *recordID = [HaloCloudSaveTransport headIDForCompatibility:compatibility];
    if (expected && (![expected.recordID isEqual:recordID] || ![expected.recordType isEqual:@"HaloProgress"])) {
        completion(nil, cloudFailure(@"The cloud save belongs to a different game-data version.")); return;
    }
    CKRecord *record = expected ? [expected copy] : [[CKRecord alloc] initWithRecordType:@"HaloProgress" recordID:recordID];
    NSFileManager *fm = NSFileManager.defaultManager;
    if (![fm createDirectoryAtURL:self.stagingDirectory withIntermediateDirectories:YES attributes:@{NSFilePosixPermissions:@0700} error:&error]) {
        completion(nil, error); return;
    }
    NSURL *directory = [self.stagingDirectory URLByAppendingPathComponent:NSUUID.UUID.UUIDString];
    if (![fm createDirectoryAtURL:directory withIntermediateDirectories:NO attributes:@{NSFilePosixPermissions:@0700} error:&error]) {
        completion(nil, error); return;
    }
    [directory setResourceValue:@YES forKey:NSURLIsExcludedFromBackupKey error:nil];
    NSURL *file = [directory URLByAppendingPathComponent:@"progress.plist"];
    if (![snapshot writeToURL:file options:NSDataWritingAtomic error:&error]) {
        [fm removeItemAtURL:directory error:nil]; completion(nil, error); return;
    }
    record[@"schema"] = @1;
    record[@"compatibility"] = compatibility;
    record[@"snapshot"] = decoded[@"snapshot"];
    record[@"payload"] = [[CKAsset alloc] initWithFileURL:file];
    CKModifyRecordsOperation *operation = [[CKModifyRecordsOperation alloc] initWithRecordsToSave:@[record] recordIDsToDelete:nil];
    operation.savePolicy = CKRecordSaveIfServerRecordUnchanged;
    operation.configuration.timeoutIntervalForRequest = 30;
    operation.configuration.timeoutIntervalForResource = 120;
    // Do not retry conflicts here: the user must be able to keep either save.
    operation.modifyRecordsCompletionBlock = ^(NSArray<CKRecord *> *saved, NSArray<CKRecordID *> *deleted, NSError *failure) {
        (void)deleted;
        [fm removeItemAtURL:directory error:nil];
        CKRecord *result = saved.firstObject;
        if (!failure && (![result.recordID isEqual:recordID] || !result.recordChangeTag.length))
            failure = cloudFailure(@"iCloud did not acknowledge the saved progress.");
        completion(failure ? nil : result, failure);
    };
    [self.database addOperation:operation];
}
@end
