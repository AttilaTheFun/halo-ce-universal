#import "../game_save.h"
#import "../save_snapshot.h"
#import "../cloud_sync.h"
#include <assert.h>
@interface TestState : NSObject
@property GSSyncState state;
@property NSURL *url;
@property NSError *error;
@property NSArray *conflictedVersions;
@end
@implementation TestState
@end
@interface TestVersion : NSObject
@property NSURL *url;
@property NSString *localizedNameOfSavingComputer;
@property NSDate *modifiedDate;
@end
@implementation TestVersion
@end
@interface TestDirectory : NSObject
@property TestState *directoryState;
@property NSUInteger closes, resolutions;
@property BOOL delay;
@property(copy) void (^reply)(void);
@end
@implementation TestDirectory
- (void)close { self.closes++; }
- (void)finishSyncingWithCompletionHandler:(void (^)(void))reply { if (self.delay) self.reply = reply; else reply(); }
- (void)resolveConflictsWithVersion:(id)version { (void)version; self.resolutions++; }
@end
static void drain(void) { [NSRunLoop.mainRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.03]]; }
int main(void) { @autoreleasepool {
    if (@available(macOS 26.0, *)) {
        NSFileManager *fm = NSFileManager.defaultManager;
        NSURL *root = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString]];
        NSURL *profile = [root URLByAppendingPathComponent:@"z/saved/profiles/default.sav"];
        assert([fm createDirectoryAtURL:profile.URLByDeletingLastPathComponent withIntermediateDirectories:YES attributes:nil error:nil]);
        assert([@"checkpoint A" writeToURL:profile atomically:YES encoding:NSUTF8StringEncoding error:nil]);
        NSData *first = halo_save_snapshot_create(root, @"game-1", nil);
        assert([@"checkpoint B" writeToURL:profile atomically:YES encoding:NSUTF8StringEncoding error:nil]);
        NSData *second = halo_save_snapshot_create(root, @"game-1", nil);
        NSURL *cloud = [root URLByAppendingPathComponent:@"remote"];
        assert([fm createDirectoryAtURL:cloud withIntermediateDirectories:YES attributes:nil error:nil]);
        TestDirectory *directory = [TestDirectory new]; directory.directoryState = [TestState new];
        directory.directoryState.state = GSSyncStateReady; directory.directoryState.url = cloud;
        NSURL *backups = [root URLByAppendingPathComponent:@"backups"];
        HaloGameSaveTransport *transport = [[HaloGameSaveTransport alloc] initWithContainer:@"iCloud.test" backups:backups];
        transport.openDirectory = ^GSSyncedDirectory *(NSString *identifier) { (void)identifier; return (id)directory; };
        __block NSUInteger replies = 0;
        __block CKRecord *observed;
        [transport fetchCompatibility:@"game-1" completion:^(CKRecord *head, NSData *bytes, NSError *error) {
            assert(!head && !bytes && !error); replies++;
        }]; drain(); assert(replies == 1);
        [transport uploadSnapshot:first compatibility:@"game-1" expectedHead:nil completion:^(CKRecord *head, NSError *error) {
            assert(head && !error); observed = head; replies++;
        }]; assert(replies == 2 && directory.closes == 1 && transport.uploadsRequireConfirmation);
        [transport fetchCompatibility:@"game-1" completion:^(CKRecord *head, NSData *bytes, NSError *error) {
            assert(head && [bytes isEqual:first] && !error); replies++;
        }]; drain(); assert(replies == 3);
        NSURL *file = [cloud URLByAppendingPathComponent:[[HaloCloudSaveTransport headIDForCompatibility:@"game-1"].recordName stringByAppendingPathExtension:@"plist"]];
        assert([second writeToURL:file atomically:YES]);
        [transport uploadSnapshot:first compatibility:@"game-1" expectedHead:observed completion:^(CKRecord *head, NSError *error) {
            assert(!head && error); replies++;
        }]; assert(replies == 4 && [[NSData dataWithContentsOfURL:file] isEqual:second]);
        for (NSNumber *state in @[@(GSSyncStateOffline), @(GSSyncStateLocal), @(GSSyncStateError)]) {
            directory.directoryState.state = state.integerValue;
            [transport fetchCompatibility:@"game-1" completion:^(CKRecord *head, NSData *bytes, NSError *error) {
                assert(!head && !bytes && error); replies++;
            }]; drain();
        }
        assert(replies == 7);
        // Conflicts cannot be resolved until all versions have safe backups.
        TestVersion *version = [TestVersion new]; version.url = cloud;
        version.localizedNameOfSavingComputer = @"Test phone"; version.modifiedDate = NSDate.date;
        directory.directoryState.state = GSSyncStateConflicted;
        directory.directoryState.conflictedVersions = @[(id)version];
        assert(transport.conflictChoices.count == 1);
        NSError *error = nil;
        assert([transport resolveConflictAtIndex:0 error:&error] && !error && directory.resolutions == 1);
        NSArray *groups = [fm contentsOfDirectoryAtURL:backups includingPropertiesForKeys:nil options:0 error:nil];
        NSArray *copies = [fm contentsOfDirectoryAtURL:groups.firstObject includingPropertiesForKeys:nil options:0 error:nil];
        assert(copies.count == 1 && [[NSData dataWithContentsOfURL:copies.firstObject] isEqual:second]);
        [transport fetchCompatibility:@"game-1" completion:^(CKRecord *head, NSData *bytes, NSError *err) {
            assert(!head && !bytes && err); replies++;
        }]; drain();
        assert([@"broken" writeToURL:file atomically:YES encoding:NSUTF8StringEncoding error:nil]);
        assert(![transport resolveConflictAtIndex:0 error:&error] && directory.resolutions == 1);
        // A stale completion after opt-out must never reach reconciliation.
        directory.delay = YES; directory.directoryState.state = GSSyncStateReady;
        [transport fetchCompatibility:@"game-1" completion:^(CKRecord *head, NSData *bytes, NSError *err) {
            (void)head; (void)bytes; (void)err; assert(0 && "Invalidated callback ran");
        }];
        [transport invalidate]; directory.reply(); drain();
        // Exercise the real coordinator against the file transport: local/cloud
        // conflict, explicit overwrite, queued handoff, confirmation and a
        // remote checkpoint arriving while the game is running.
        directory.delay = NO; directory.directoryState.state = GSSyncStateReady;
        assert([second writeToURL:file atomically:YES]);
        HaloGameSaveTransport *integrated = [[HaloGameSaveTransport alloc] initWithContainer:@"iCloud.test" backups:backups];
        integrated.openDirectory = ^GSSyncedDirectory *(NSString *identifier) { (void)identifier; return (id)directory; };
        NSURL *candidate = [root URLByAppendingPathComponent:@"Cloud Saves/local-checkpoint.plist"];
        assert([fm createDirectoryAtURL:candidate.URLByDeletingLastPathComponent withIntermediateDirectories:YES attributes:nil error:nil]);
        assert([first writeToURL:candidate atomically:YES]);
        HaloCloudSync *sync = [[HaloCloudSync alloc] initWithRoot:root compatibility:@"game-1" account:@"test-account" transport:integrated];
        [sync synchronize]; drain(); drain();
        assert([sync.status isEqual:@"conflict"]);
        [sync keepLocal]; drain();
        assert([sync.status isEqual:@"queued"] && [[NSData dataWithContentsOfURL:file] isEqual:first]);
        [sync synchronize]; drain(); drain();
        assert([sync.status isEqual:@"synced"]);
        assert([second writeToURL:file atomically:YES]);
        NSData *runningSave = [NSData dataWithContentsOfURL:profile];
        sync.guestStopped = NO;
        [sync synchronize]; drain(); drain();
        assert([sync.status isEqual:@"cloudReadyForRelaunch"]);
        assert([[NSData dataWithContentsOfURL:profile] isEqual:runningSave]);
        [sync invalidate]; [integrated invalidate];
        assert([fm removeItemAtURL:root error:nil]);
        puts("GameSave contracts passed: file exchange, revision checks, offline fallback, conflict backups, opt-out invalidation");
    }
} return 0; }
