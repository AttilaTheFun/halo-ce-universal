#import "../cloud_save.h"
#import "../save_snapshot.h"
#include <assert.h>

@interface AcknowledgedRecord : CKRecord
@end
@implementation AcknowledgedRecord
- (NSString *)recordChangeTag { return @"test-server-revision"; }
@end
@interface FakeDatabase : NSObject <HaloCloudDatabase>
@property CKRecord *head;
@property NSError *fetchError;
@property NSError *saveError;
@property NSURL *remoteFile;
@property NSUInteger writes;
@property CKRecordSavePolicy policy;
@property BOOL assetReadableDuringUpload;
@end
@implementation FakeDatabase
- (void)fetchRecordWithID:(CKRecordID *)recordID completionHandler:(void (^)(CKRecord *, NSError *))completion {
    (void)recordID;
    completion(self.head, self.fetchError ?: (self.head ? nil : [NSError errorWithDomain:CKErrorDomain code:CKErrorUnknownItem userInfo:nil]));
}
- (void)addOperation:(CKDatabaseOperation *)operation {
    CKModifyRecordsOperation *op = (CKModifyRecordsOperation *)operation;
    self.writes++; self.policy = op.savePolicy;
    CKRecord *input = op.recordsToSave.firstObject;
    CKAsset *asset = input[@"payload"];
    NSData *bytes = [NSData dataWithContentsOfURL:asset.fileURL];
    self.assetReadableDuringUpload = bytes.length > 0;
    if (self.saveError) { op.modifyRecordsCompletionBlock(nil, nil, self.saveError); return; }
    assert([bytes writeToURL:self.remoteFile atomically:YES]);
    CKRecord *saved = [[AcknowledgedRecord alloc] initWithRecordType:input.recordType recordID:input.recordID];
    for (NSString *key in input.allKeys) saved[key] = input[key];
    saved[@"payload"] = [[CKAsset alloc] initWithFileURL:self.remoteFile];
    self.head = saved;
    op.modifyRecordsCompletionBlock(@[saved], @[], nil);
}
@end
int main(void) { @autoreleasepool {
    NSFileManager *fm = NSFileManager.defaultManager;
    NSURL *root = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString]];
    NSURL *profile = [root URLByAppendingPathComponent:@"z/saved/profiles/default.sav"];
    assert([fm createDirectoryAtURL:profile.URLByDeletingLastPathComponent withIntermediateDirectories:YES attributes:nil error:nil]);
    assert([@"progress" writeToURL:profile atomically:YES encoding:NSUTF8StringEncoding error:nil]);
    NSError *error = nil;
    NSData *snapshot = halo_save_snapshot_create(root, @"game-1", &error);
    assert(snapshot);
    FakeDatabase *db = [FakeDatabase new]; db.remoteFile = [root URLByAppendingPathComponent:@"server-copy"];
    NSURL *staging = [root URLByAppendingPathComponent:@"uploads"];
    HaloCloudSaveTransport *transport = [[HaloCloudSaveTransport alloc] initWithDatabase:db stagingDirectory:staging];
    __block NSUInteger callbacks = 0;
    [transport fetchCompatibility:@"game-1" completion:^(CKRecord *head, NSData *data, NSError *failure) {
        assert(!head && !data && !failure); callbacks++;
    }];
    db.fetchError = [NSError errorWithDomain:CKErrorDomain code:CKErrorNetworkUnavailable userInfo:nil];
    [transport fetchCompatibility:@"game-1" completion:^(CKRecord *head, NSData *data, NSError *failure) {
        assert(!head && !data && failure.code == CKErrorNetworkUnavailable); callbacks++;
    }];
    db.fetchError = nil;
    [transport uploadSnapshot:snapshot compatibility:@"game-1" expectedHead:nil completion:^(CKRecord *head, NSError *failure) {
        assert(head.recordChangeTag && !failure); callbacks++;
    }];
    assert(db.writes == 1 && db.policy == CKRecordSaveIfServerRecordUnchanged && db.assetReadableDuringUpload);
    assert([fm contentsOfDirectoryAtPath:staging.path error:nil].count == 0);
    [transport fetchCompatibility:@"game-1" completion:^(CKRecord *head, NSData *data, NSError *failure) {
        assert(head && [data isEqual:snapshot] && !failure); callbacks++;
    }];
    db.saveError = [NSError errorWithDomain:CKErrorDomain code:CKErrorServerRecordChanged userInfo:nil];
    [transport uploadSnapshot:snapshot compatibility:@"game-1" expectedHead:db.head completion:^(CKRecord *head, NSError *failure) {
        assert(!head && failure.code == CKErrorServerRecordChanged); callbacks++;
    }];
    assert(db.writes == 2); // no silent retry with a fresh head
    assert([fm contentsOfDirectoryAtPath:staging.path error:nil].count == 0);
    [transport uploadSnapshot:snapshot compatibility:@"other-game" expectedHead:db.head completion:^(CKRecord *head, NSError *failure) {
        assert(!head && failure); callbacks++;
    }];
    assert(db.writes == 2);
    db.head[@"snapshot"] = NSUUID.UUID.UUIDString;
    [transport fetchCompatibility:@"game-1" completion:^(CKRecord *head, NSData *data, NSError *failure) {
        assert(!head && !data && failure); callbacks++;
    }];
    assert(callbacks == 7);
    assert([fm removeItemAtURL:root error:nil]);
    puts("Cloud transport contracts passed: missing/offline, upload lifetime, validated download, conflict without retry, compatibility and metadata mismatch");
} }
