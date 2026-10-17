#import "../cloud_save.h"
#import "../save_snapshot.h"
#import "../cloud_sync.h"
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
@property NSUInteger reads;
@property CKRecordSavePolicy policy;
@property BOOL assetReadableDuringUpload;
@end
@implementation FakeDatabase
- (void)fetchRecordWithID:(CKRecordID *)recordID completionHandler:(void (^)(CKRecord *, NSError *))completion {
    (void)recordID; self.reads++;
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
static void waitSync(HaloCloudSync *sync) {
    NSDate *limit = [NSDate dateWithTimeIntervalSinceNow:3];
    while (sync.busy && limit.timeIntervalSinceNow > 0)
        [NSRunLoop.mainRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
    assert(!sync.busy);
}
static void putCandidate(NSURL *root, NSData *data) {
    NSURL *directory = [root URLByAppendingPathComponent:@"Cloud Saves"];
    assert([NSFileManager.defaultManager createDirectoryAtURL:directory withIntermediateDirectories:YES attributes:nil error:nil]);
    assert([data writeToURL:[directory URLByAppendingPathComponent:@"local-checkpoint.plist"] atomically:YES]);
}
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
    // Coordinator reconciliation against the real transport with a fake server.
    db.saveError = nil;
    db.head[@"snapshot"] = halo_save_snapshot_decode(snapshot, @"game-1", nil)[@"snapshot"];
    NSURL *device = [root URLByAppendingPathComponent:@"device"];
    putCandidate(device, snapshot);
    HaloCloudSync *sync = [[HaloCloudSync alloc] initWithRoot:device compatibility:@"game-1" account:@"user-A" transport:transport];
    sync.guestStopped = YES;
    [sync synchronize]; waitSync(sync); assert([sync.status isEqual:@"synced"]);
    assert([@"second" writeToURL:profile atomically:YES encoding:NSUTF8StringEncoding error:nil]);
    NSData *second = halo_save_snapshot_create(root, @"game-1", nil);
    putCandidate(device, second);
    NSUInteger writes = db.writes;
    [sync synchronize]; waitSync(sync);
    assert([sync.status isEqual:@"synced"] && db.writes == writes + 1);
    assert([@"third" writeToURL:profile atomically:YES encoding:NSUTF8StringEncoding error:nil]);
    NSData *third = halo_save_snapshot_create(root, @"game-1", nil);
    [transport uploadSnapshot:third compatibility:@"game-1" expectedHead:db.head completion:^(CKRecord *head, NSError *failure) { assert(head && !failure); }];
    sync.guestStopped = NO;
    [sync synchronize]; waitSync(sync);
    assert([sync.status isEqual:@"cloudReadyForRelaunch"]);
    assert(![fm fileExistsAtPath:[device URLByAppendingPathComponent:@"z/saved/profiles/default.sav"].path]);
    sync.guestStopped = YES; [sync useCloud];
    assert([sync.status isEqual:@"synced"]);
    assert([[NSString stringWithContentsOfURL:[device URLByAppendingPathComponent:@"z/saved/profiles/default.sav"] encoding:NSUTF8StringEncoding error:nil] isEqual:@"third"]);
    assert([@"local fourth" writeToURL:profile atomically:YES encoding:NSUTF8StringEncoding error:nil]);
    NSData *fourth = halo_save_snapshot_create(root, @"game-1", nil);
    putCandidate(device, fourth);
    assert([@"remote fifth" writeToURL:profile atomically:YES encoding:NSUTF8StringEncoding error:nil]);
    NSData *fifth = halo_save_snapshot_create(root, @"game-1", nil);
    [transport uploadSnapshot:fifth compatibility:@"game-1" expectedHead:db.head completion:^(CKRecord *head, NSError *failure) { assert(head && !failure); }];
    writes = db.writes;
    [sync synchronize]; waitSync(sync);
    assert([sync.status isEqual:@"conflict"] && db.writes == writes);
    db.saveError = [NSError errorWithDomain:CKErrorDomain code:CKErrorServerRecordChanged userInfo:nil];
    [sync keepLocal]; waitSync(sync);
    assert([sync.status isEqual:@"syncError"]);
    NSURL *remoteBackup = [[device URLByAppendingPathComponent:@"Cloud Saves/Conflicts"]
        URLByAppendingPathComponent:[halo_save_snapshot_decode(fifth, @"game-1", nil)[@"snapshot"] stringByAppendingPathExtension:@"plist"]];
    assert([[NSData dataWithContentsOfURL:remoteBackup] isEqual:fifth]);
    // If the backup cannot be persisted, Keep This Device must not overwrite
    // the remote revision, even when the server would accept the upload.
    NSURL *conflictDirectory = remoteBackup.URLByDeletingLastPathComponent;
    NSURL *heldBackups = [device URLByAppendingPathComponent:@"held-backups"];
    assert([fm moveItemAtURL:conflictDirectory toURL:heldBackups error:nil]);
    assert([@"not a directory" writeToURL:conflictDirectory atomically:YES encoding:NSUTF8StringEncoding error:nil]);
    writes = db.writes;
    [sync keepLocal]; waitSync(sync);
    assert([sync.status isEqual:@"localError"] && db.writes == writes);
    assert([fm removeItemAtURL:conflictDirectory error:nil]);
    assert([fm moveItemAtURL:heldBackups toURL:conflictDirectory error:nil]);
    db.saveError = nil;
    [sync synchronize]; waitSync(sync); assert([sync.status isEqual:@"conflict"]);
    [sync useCloud]; assert([sync.status isEqual:@"synced"]);
    NSURL *conflicts = [device URLByAppendingPathComponent:@"Cloud Saves/Conflicts"];
    BOOL foundLocal = NO;
    for (NSURL *file in [fm contentsOfDirectoryAtURL:conflicts includingPropertiesForKeys:nil options:0 error:nil])
        foundLocal |= [[NSData dataWithContentsOfURL:file] isEqual:fourth];
    assert(foundLocal);
    HaloCloudSync *changed = [[HaloCloudSync alloc] initWithRoot:device compatibility:@"game-1" account:@"user-B" transport:transport];
    NSUInteger reads = db.reads;
    [changed synchronize]; assert([changed.status isEqual:@"accountChanged"] && db.reads == reads);
    [sync synchronize]; [sync invalidate];
    [NSRunLoop.mainRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.03]];
    assert([sync.status isEqual:@"accountUnavailable"]);
    assert([fm removeItemAtURL:root error:nil]);
    puts("Cloud contracts passed: transport validation, offline errors, conditional upload, reconciliation, stopped-guest restore, conflict preservation, account isolation and invalidation");
} }
