#import "cloud_sync.h"
#import "save_snapshot.h"
#import "save_restore.h"
#include <fcntl.h>
#include <unistd.h>

@interface HaloCloudSync ()
@property(nonatomic) NSURL *root;
@property(nonatomic) NSURL *stateURL;
@property(nonatomic) NSURL *candidateURL;
@property(nonatomic) NSString *compatibility;
@property(nonatomic) NSString *account;
@property(nonatomic) HaloCloudSaveTransport *transport;
@property(nonatomic) NSMutableDictionary *state;
@property(nonatomic, readwrite) NSString *status;
@property(nonatomic, readwrite) NSError *lastError;
@property(nonatomic, readwrite) BOOL busy;
@property(nonatomic) BOOL invalid;
@property(nonatomic) CKRecord *remoteHead;
@property(nonatomic) NSData *remoteBytes;
@end
@implementation HaloCloudSync
- (instancetype)initWithRoot:(NSURL *)root compatibility:(NSString *)compatibility account:(NSString *)account transport:(HaloCloudSaveTransport *)transport {
    if ((self = [super init])) {
        _root = root; _compatibility = compatibility; _account = account; _transport = transport;
        NSURL *directory = [root URLByAppendingPathComponent:@"Cloud Saves"];
        _stateURL = [directory URLByAppendingPathComponent:@"sync-state.plist"];
        _candidateURL = [directory URLByAppendingPathComponent:@"local-checkpoint.plist"];
        NSDictionary *attributes = [NSFileManager.defaultManager attributesOfItemAtPath:_stateURL.path error:nil];
        BOOL readable = [attributes[NSFileType] isEqual:NSFileTypeRegular] &&
            [attributes[NSFileSize] unsignedLongLongValue] <= 1024 * 1024;
        NSDictionary *stored = readable ? [NSDictionary dictionaryWithContentsOfURL:_stateURL] : nil;
        _state = stored ? [stored mutableCopy] : [NSMutableDictionary new];
        _status = @"idle";
        if ([NSFileManager.defaultManager fileExistsAtPath:_stateURL.path] &&
            (!stored || ![stored[@"version"] isEqual:@1] || ![stored[@"account"] isKindOfClass:NSString.class] ||
             ![stored[@"compatibility"] isKindOfClass:NSString.class] ||
             (stored[@"baseSnapshot"] && (![stored[@"baseSnapshot"] isKindOfClass:NSString.class] ||
                ![[NSUUID alloc] initWithUUIDString:stored[@"baseSnapshot"]])))) _status = @"invalidLocalState";
        else if (stored && ![stored[@"account"] isEqual:account]) _status = @"accountChanged";
        else if (stored && ![stored[@"compatibility"] isEqual:compatibility]) _status = @"gameChanged";
    }
    return self;
}
- (void)announce:(NSString *)status error:(NSError *)error {
    self.status = status; self.lastError = error;
    if (self.stateChanged) self.stateChanged();
}
- (BOOL)writeData:(NSData *)data to:(NSURL *)url error:(NSError **)error {
    NSFileManager *fm = NSFileManager.defaultManager;
    if (![fm createDirectoryAtURL:url.URLByDeletingLastPathComponent withIntermediateDirectories:YES attributes:@{NSFilePosixPermissions:@0700} error:error] ||
        ![data writeToURL:url options:NSDataWritingAtomic error:error]) return NO;
    int fd = open(url.fileSystemRepresentation, O_RDONLY | O_NOFOLLOW);
    BOOL ok = fd >= 0 && !fsync(fd); if (fd >= 0) close(fd);
    if (!ok && error) *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:nil];
    return ok;
}
- (BOOL)persistBase:(NSString *)base {
    NSMutableDictionary *state = [@{@"version":@1, @"account":self.account, @"compatibility":self.compatibility} mutableCopy];
    if (base) state[@"baseSnapshot"] = base;
    NSError *error = nil;
    NSData *bytes = [NSPropertyListSerialization dataWithPropertyList:state format:NSPropertyListBinaryFormat_v1_0 options:0 error:&error];
    if (!bytes || ![self writeData:bytes to:self.stateURL error:&error]) {
        [self announce:@"localError" error:error]; return NO;
    }
    self.state = state; return YES;
}
- (NSData *)candidate {
    // The bounded decoder checks the snapshot; reject oversized input before
    // allocating it from disk, even for damaged local state.
    NSDictionary *attrs = [NSFileManager.defaultManager attributesOfItemAtPath:self.candidateURL.path error:nil];
    if (!attrs) return nil;
    if (![attrs[NSFileType] isEqual:NSFileTypeRegular] || [attrs[NSFileSize] unsignedLongLongValue] > 128 * 1024 * 1024) {
        [self announce:@"invalidLocalSave" error:nil]; return nil;
    }
    NSError *error = nil;
    NSData *bytes = [NSData dataWithContentsOfURL:self.candidateURL options:0 error:&error];
    if (!bytes || !halo_save_snapshot_decode(bytes, self.compatibility, &error)) {
        [self announce:@"invalidLocalSave" error:error]; return nil;
    }
    return bytes;
}
- (NSString *)snapshotID:(NSData *)bytes {
    return bytes ? halo_save_snapshot_decode(bytes, self.compatibility, nil)[@"snapshot"] : nil;
}
- (BOOL)canAct {
    NSAssert(NSThread.isMainThread, @"Cloud save coordinator runs on the main thread");
    return !self.invalid && !self.busy && self.account.length && self.compatibility.length &&
        ![@[@"accountChanged", @"gameChanged", @"invalidLocalState"] containsObject:self.status];
}
- (void)synchronize {
    if (![self canAct]) return;
    // Bind BEFORE networking. Failed/offline attempts remain associated with
    // this account, preventing a later sign-in from inheriting queued saves.
    if (!self.state[@"account"] && ![self persistBase:nil]) return;
    self.busy = YES; [self announce:@"checking" error:nil];
    [self.transport fetchCompatibility:self.compatibility completion:^(CKRecord *head, NSData *bytes, NSError *error) {
        dispatch_async(dispatch_get_main_queue(), ^{
            if (self.invalid) return;
            self.busy = NO;
            if (error) { [self announce:@"offline" error:error]; return; }
            self.remoteHead = head; self.remoteBytes = bytes;
            NSData *local = [self candidate];
            if ([self.status isEqual:@"invalidLocalSave"]) return;
            NSString *localID = [self snapshotID:local], *remoteID = [self snapshotID:bytes], *base = self.state[@"baseSnapshot"];
            if (!localID && !remoteID) { [self announce:@"ready" error:nil]; return; }
            if ([localID isEqual:remoteID]) {
                if ([self persistBase:remoteID]) [self announce:@"synced" error:nil]; return;
            }
            if (!localID || [localID isEqual:base]) {
                if (bytes) [self useCloud]; else [self announce:@"conflict" error:nil];
            } else if ((!remoteID && !base) || [remoteID isEqual:base]) {
                [self upload:local];
            } else [self announce:@"conflict" error:nil];
        });
    }];
}
- (void)upload:(NSData *)local {
    if (![self canAct] || !local) return;
    NSString *sentID = [self snapshotID:local];
    self.busy = YES; [self announce:@"uploading" error:nil];
    [self.transport uploadSnapshot:local compatibility:self.compatibility expectedHead:self.remoteHead completion:^(CKRecord *head, NSError *error) {
        dispatch_async(dispatch_get_main_queue(), ^{
            if (self.invalid) return;
            self.busy = NO;
            if (error) {
                // Includes per-record partial failures: never retry by fetching
                // and overwriting a new revision without reconciliation.
                [self announce:@"syncError" error:error]; return;
            }
            self.remoteHead = head; self.remoteBytes = local;
            if ([self persistBase:sentID]) {
                // A checkpoint written during upload stays pending.
                NSString *latest = [self snapshotID:[self candidate]];
                if (![self.status isEqual:@"invalidLocalSave"])
                    [self announce:[latest isEqual:sentID] ? (self.transport.uploadsRequireConfirmation ? @"queued" : @"synced") : @"pending" error:nil];
            }
        });
    }];
}
- (void)keepLocal {
    if (![self canAct]) return;
    NSData *local = [self candidate];
    if (!local) return;
    // Keeping this device also replaces somebody's progress: retain the
    // observed cloud revision locally before issuing its conditional update.
    // A server-side race still fails through the transport's change-tag check.
    if (self.remoteBytes && ![[self snapshotID:local] isEqual:[self snapshotID:self.remoteBytes]]) {
        NSError *error = nil;
        NSURL *backup = [[self.stateURL.URLByDeletingLastPathComponent URLByAppendingPathComponent:@"Conflicts"]
            URLByAppendingPathComponent:[[self snapshotID:self.remoteBytes] stringByAppendingPathExtension:@"plist"]];
        if (![self writeData:self.remoteBytes to:backup error:&error]) {
            [self announce:@"localError" error:error]; return;
        }
    }
    [self upload:local];
}
- (void)useCloud {
    if (![self canAct] || !self.remoteBytes) return;
    if (!self.guestStopped) { [self announce:@"cloudReadyForRelaunch" error:nil]; return; }
    NSError *error = nil;
    NSData *local = [self candidate];
    if ([self.status isEqual:@"invalidLocalSave"]) return;
    // Preserve the losing side before adopting an explicit conflict choice.
    if (local && ![[self snapshotID:local] isEqual:[self snapshotID:self.remoteBytes]]) {
        NSURL *conflict = [[self.stateURL.URLByDeletingLastPathComponent URLByAppendingPathComponent:@"Conflicts"]
            URLByAppendingPathComponent:[NSUUID.UUID.UUIDString stringByAppendingPathExtension:@"plist"]];
        if (![self writeData:local to:conflict error:&error]) { [self announce:@"localError" error:error]; return; }
    }
    if (!halo_save_restore(self.remoteBytes, self.compatibility, self.root, &error) ||
        ![self writeData:self.remoteBytes to:self.candidateURL error:&error]) {
        [self announce:@"restoreError" error:error]; return;
    }
    if ([self persistBase:[self snapshotID:self.remoteBytes]]) [self announce:@"synced" error:nil];
}
- (void)allowAccountChange {
    NSAssert(NSThread.isMainThread, @"Cloud save coordinator runs on the main thread");
    if (self.invalid || self.busy || ![self.status isEqual:@"accountChanged"]) return;
    // Explicit UI consent is required; do not call automatically on sign-in.
    if ([self persistBase:nil]) { [self announce:@"idle" error:nil]; [self synchronize]; }
}
- (void)invalidate {
    NSAssert(NSThread.isMainThread, @"Cloud save coordinator runs on the main thread");
    self.invalid = YES; self.busy = NO; self.remoteHead = nil; self.remoteBytes = nil;
    [self announce:@"accountUnavailable" error:nil];
}
@end
