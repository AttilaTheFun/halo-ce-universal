#import "game_save.h"
#import "save_snapshot.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

static NSError *failure(NSString *message) {
    return [NSError errorWithDomain:@"HaloGameSave" code:1 userInfo:@{NSLocalizedDescriptionKey:message}];
}
static NSData *readCheckpoint(NSURL *url, BOOL *missing, NSError **error) {
    *missing = NO;
    int fd = open(url.fileSystemRepresentation, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0 && errno == ENOENT) { *missing = YES; return nil; }
    struct stat st;
    if (fd < 0 || fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_size <= 0 || st.st_size > 128 * 1024 * 1024) {
        if (fd >= 0) close(fd);
        *error = failure(@"GameSave returned an invalid or oversized checkpoint."); return nil;
    }
    NSMutableData *bytes = [NSMutableData dataWithLength:(NSUInteger)st.st_size];
    NSUInteger done = 0;
    while (done < bytes.length) {
        ssize_t n = read(fd, (char *)bytes.mutableBytes + done, bytes.length - done);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        done += n;
    }
    close(fd);
    if (done != bytes.length) { *error = failure(@"The GameSave checkpoint is incomplete."); return nil; }
    return bytes;
}
static BOOL writeCheckpoint(NSData *bytes, NSURL *file, NSError **error) {
    if (![bytes writeToURL:file options:NSDataWritingAtomic error:error]) return NO;
    int fd = open(file.fileSystemRepresentation, O_RDONLY | O_NOFOLLOW);
    BOOL ok = fd >= 0 && !fsync(fd);
    if (fd >= 0) close(fd);
    int parent = open(file.URLByDeletingLastPathComponent.fileSystemRepresentation, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    ok = parent >= 0 && !fsync(parent) && ok;
    if (parent >= 0) close(parent);
    if (!ok && error) *error = failure(@"The save could not be committed to storage. Local progress is unchanged.");
    return ok;
}
API_AVAILABLE(ios(26.0), macos(26.0))
@interface HaloGameSaveTransport ()
@property(nonatomic) NSString *identifier;
@property(nonatomic) NSURL *backups;
@property(nonatomic) GSSyncedDirectory *directory;
@property(nonatomic) NSString *compatibility;
@property(nonatomic) BOOL invalid;
@property(nonatomic) NSUInteger generation;
@end
@implementation HaloGameSaveTransport
- (BOOL)uploadsRequireConfirmation { return YES; }
- (instancetype)initWithContainer:(NSString *)identifier backups:(NSURL *)backups {
    if ((self = [super init])) {
        _identifier = identifier; _backups = backups;
        _openDirectory = ^GSSyncedDirectory *(NSString *name) { return [GSSyncedDirectory openDirectoryForContainerIdentifier:name]; };
    }
    return self;
}
- (void)invalidate {
    self.invalid = YES; self.generation++;
    [self.directory close]; self.directory = nil;
}
- (void)dealloc { [_directory close]; }
- (NSURL *)checkpointURL:(NSURL *)root compatibility:(NSString *)compatibility {
    return [root URLByAppendingPathComponent:[[HaloCloudSaveTransport headIDForCompatibility:compatibility].recordName stringByAppendingPathExtension:@"plist"]];
}
- (CKRecord *)head:(NSData *)bytes compatibility:(NSString *)compatibility error:(NSError **)error {
    NSDictionary *decoded = halo_save_snapshot_decode(bytes, compatibility, error);
    if (!decoded) return nil;
    CKRecord *head = [[CKRecord alloc] initWithRecordType:@"HaloProgress" recordID:[HaloCloudSaveTransport headIDForCompatibility:compatibility]];
    head[@"snapshot"] = decoded[@"snapshot"];
    return head;
}
- (void)fetchCompatibility:(NSString *)compatibility completion:(void (^)(CKRecord *, NSData *, NSError *))completion {
    NSAssert(NSThread.isMainThread, @"GameSave runs on main thread");
    if (self.invalid) { completion(nil, nil, failure(@"Sync is disabled.")); return; }
    [self.directory close]; self.directory = nil;
    self.compatibility = compatibility;
    NSUInteger token = ++self.generation;
    @try { self.directory = self.openDirectory(self.identifier); }
    @catch (NSException *exception) { (void)exception; completion(nil, nil, failure(@"This signed app cannot access GameSave. Local saves are unchanged.")); return; }
    if (!self.directory) { completion(nil, nil, failure(@"GameSave is unavailable.")); return; }
    [self.directory finishSyncingWithCompletionHandler:^{
        dispatch_async(dispatch_get_main_queue(), ^{
            if (self.invalid || token != self.generation) return;
            GSSyncedDirectoryState *state = self.directory.directoryState;
            // Do not mistake an offline/local cache for an authoritative remote
            // head. Keep checkpoints local until cloud access is available.
            if (state.state != GSSyncStateReady || !state.url.isFileURL) {
                completion(nil, nil, state.error ?: failure(state.state == GSSyncStateConflicted ? @"Choose an iCloud save version in Settings." : @"GameSave is offline or unavailable. Progress stays local.")); return;
            }
            NSError *error = nil; BOOL missing;
            NSData *bytes = readCheckpoint([self checkpointURL:state.url compatibility:compatibility], &missing, &error);
            CKRecord *head = bytes ? [self head:bytes compatibility:compatibility error:&error] : nil;
            completion(head, head ? bytes : nil, error);
        });
    }];
}
- (void)uploadSnapshot:(NSData *)bytes compatibility:(NSString *)compatibility expectedHead:(CKRecord *)expected completion:(void (^)(CKRecord *, NSError *))completion {
    NSAssert(NSThread.isMainThread, @"GameSave runs on main thread");
    GSSyncedDirectoryState *state = self.directory.directoryState;
    if (self.invalid || !self.directory || state.state != GSSyncStateReady || !state.url.isFileURL || ![compatibility isEqual:self.compatibility]) {
        completion(nil, failure(@"GameSave is not ready; the checkpoint remains local.")); return;
    }
    NSError *error = nil;
    CKRecord *head = [self head:bytes compatibility:compatibility error:&error];
    if (!head) { completion(nil, error); return; }
    NSURL *file = [self checkpointURL:state.url compatibility:compatibility];
    BOOL missing;
    NSData *current = readCheckpoint(file, &missing, &error);
    CKRecord *currentHead = current ? [self head:current compatibility:compatibility error:&error] : nil;
    if (error || (expected && (![expected.recordID isEqual:head.recordID] || ![expected[@"snapshot"] isEqual:currentHead[@"snapshot"]])) || (!expected && !missing)) {
        completion(nil, error ?: failure(@"GameSave changed; sync again before choosing a save.")); return;
    }
    if (!writeCheckpoint(bytes, file, &error)) { completion(nil, error); return; }
    // GameSave owns upload/reconciliation after close. This acknowledges a
    // durable local handoff, NOT a confirmed server upload.
    [self.directory close]; self.directory = nil;
    completion(head, nil);
}
- (NSArray<NSString *> *)conflictChoices {
    if (self.invalid || self.directory.directoryState.state != GSSyncStateConflicted) return @[];
    NSMutableArray *choices = [NSMutableArray new];
    NSDateFormatter *format = [NSDateFormatter new]; format.dateStyle = NSDateFormatterShortStyle; format.timeStyle = NSDateFormatterShortStyle;
    for (GSSyncedDirectoryVersion *version in self.directory.directoryState.conflictedVersions)
        [choices addObject:[NSString stringWithFormat:@"%@ — %@", version.localizedNameOfSavingComputer ?: @"Device", [format stringFromDate:version.modifiedDate] ?: @"Unknown date"]];
    return choices;
}
- (BOOL)resolveConflictAtIndex:(NSUInteger)index error:(NSError **)error {
    if (self.invalid || self.directory.directoryState.state != GSSyncStateConflicted) return NO;
    NSArray<GSSyncedDirectoryVersion *> *versions = self.directory.directoryState.conflictedVersions;
    if (index >= versions.count) return NO;
    // Back up EVERY checkpoint in EVERY version before GameSave can discard
    // a losing directory, including checkpoints from other imported ISO hashes.
    NSURL *backup = [self.backups URLByAppendingPathComponent:NSUUID.UUID.UUIDString];
    NSFileManager *fm = NSFileManager.defaultManager;
    if (![fm createDirectoryAtURL:backup withIntermediateDirectories:YES attributes:@{NSFilePosixPermissions:@0700} error:error]) return NO;
    NSUInteger count = 0, total = 0;
    for (GSSyncedDirectoryVersion *version in versions) {
        NSArray<NSURL *> *files = [fm contentsOfDirectoryAtURL:version.url includingPropertiesForKeys:nil options:0 error:error];
        if (!files) return NO;
        for (NSURL *file in files) {
            if (![file.lastPathComponent hasPrefix:@"progress-v1-"] || ![file.pathExtension isEqual:@"plist"]) continue;
            if (++count > 128) { if (error) *error = failure(@"Too many conflicting saves to back up safely."); return NO; }
            BOOL missing; NSError *readError = nil;
            NSData *bytes = readCheckpoint(file, &missing, &readError);
            NSDictionary *plist = bytes ? [NSPropertyListSerialization propertyListWithData:bytes options:NSPropertyListImmutable format:nil error:&readError] : nil;
            NSString *identity = [plist isKindOfClass:NSDictionary.class] ? plist[@"compatibility"] : nil;
            if (![identity isKindOfClass:NSString.class] || !halo_save_snapshot_decode(bytes, identity, &readError)) {
                if (error) *error = readError ?: failure(@"A conflicting save could not be backed up."); return NO;
            }
            total += bytes.length;
            if (total > 512 * 1024 * 1024) { if (error) *error = failure(@"Conflict backups exceed the safety limit."); return NO; }
            if (!writeCheckpoint(bytes, [backup URLByAppendingPathComponent:[NSString stringWithFormat:@"%lu.plist", (unsigned long)count]], error)) return NO;
        }
    }
    [self.directory resolveConflictsWithVersion:versions[index]];
    [self.directory close]; self.directory = nil;
    return YES;
}
@end
