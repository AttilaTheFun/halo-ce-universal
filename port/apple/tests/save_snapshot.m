#import "../save_snapshot.h"
#import "../save_restore.h"
#include "../save_runtime.h"
#include <assert.h>

void halo_cloud_checkpoint_ready(void) {} // Network scheduling is tested separately.
static NSData *encode(NSDictionary *record) {
    return [NSPropertyListSerialization dataWithPropertyList:record format:NSPropertyListBinaryFormat_v1_0 options:0 error:nil];
}
static void fixture_write(NSURL *root, NSString *path, NSString *value) {
    NSURL *file = [root URLByAppendingPathComponent:path];
    assert([NSFileManager.defaultManager createDirectoryAtURL:file.URLByDeletingLastPathComponent withIntermediateDirectories:YES attributes:nil error:nil]);
    assert([value writeToURL:file atomically:YES encoding:NSUTF8StringEncoding error:nil]);
}
int main(int argc, char **argv) { @autoreleasepool {
    if (argc == 4) {
        NSURL *root = [NSURL fileURLWithPath:[NSString stringWithUTF8String:argv[1]]];
        NSData *data = [NSData dataWithContentsOfFile:[NSString stringWithUTF8String:argv[2]]];
        NSError *error = nil;
        return halo_save_restore(data, [NSString stringWithUTF8String:argv[3]], root, &error) ? 0 : 1;
    }
    NSFileManager *fm = NSFileManager.defaultManager;
    NSURL *root = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString]];
    assert([fm createDirectoryAtURL:root withIntermediateDirectories:NO attributes:nil error:nil]);
    fixture_write(root, @"u/UDATA/profile/savegame.bin", @"checkpoint");
    fixture_write(root, @"z/saved/player_profiles/default/00.sav", @"profile");
    fixture_write(root, @"z/cache000.map", @"DO NOT SYNC");
    fixture_write(root, @"z/savegame.bin", @"TRANSIENT");
    fixture_write(root, @"game-private/disc.iso", @"RETAIL DATA");
    fixture_write(root, @"halo.log", @"PRIVATE LOG");
    fixture_write(root, @"z/lastprof.txt", @"u:\\UDATA\\profile");
    fixture_write(root, @"z/last_solo.txt", @"levels\\b30\\b30");
    NSError *error = nil;
    NSData *data = halo_save_snapshot_create(root, @"retail-2276-runtime-1", &error);
    assert(data && !error);
    NSDictionary *snapshot = halo_save_snapshot_decode(data, @"retail-2276-runtime-1", &error);
    assert(snapshot && [snapshot[@"files"] count] == 4);
    assert(!halo_save_snapshot_decode(data, @"other-disc", &error));
    NSURL *staged = halo_save_snapshot_stage(data, @"retail-2276-runtime-1", root, &error);
    assert(staged);
    assert([[NSString stringWithContentsOfURL:[staged URLByAppendingPathComponent:@"u/UDATA/profile/savegame.bin"] encoding:NSUTF8StringEncoding error:nil] isEqual:@"checkpoint"]);
    assert(![fm fileExistsAtPath:[staged URLByAppendingPathComponent:@"z/cache000.map"].path]);
    assert([fm fileExistsAtPath:[root URLByAppendingPathComponent:@"z/cache000.map"].path]);
    NSDictionary *entry = snapshot[@"files"][@"u/UDATA/profile/savegame.bin"];
    for (NSString *path in @[@"u/UDATA/../../escape", @"/u/UDATA/save", @"z/cache000.map", @"u/UDATA//file", @"u/UDATA/a\\b"]) {
        NSMutableDictionary *bad = [snapshot mutableCopy]; bad[@"files"] = @{path:entry};
        assert(!halo_save_snapshot_stage(encode(bad), @"retail-2276-runtime-1", root, &error));
    }
    NSMutableDictionary *bad = [snapshot mutableCopy];
    bad[@"files"] = @{@"u/UDATA/file":entry, @"u/UDATA/FILE":entry};
    assert(!halo_save_snapshot_decode(encode(bad), @"retail-2276-runtime-1", &error));
    bad[@"files"] = @{@"u/UDATA/file":entry, @"u/UDATA/file/child":entry};
    assert(!halo_save_snapshot_decode(encode(bad), @"retail-2276-runtime-1", &error));
    bad[@"files"] = @{@"u/UDATA/file":@{@"data":[@"corrupt" dataUsingEncoding:NSUTF8StringEncoding], @"sha256":entry[@"sha256"]}};
    assert(!halo_save_snapshot_decode(encode(bad), @"retail-2276-runtime-1", &error));
    bad[@"files"] = @{ @"u/UDATA/file": @"wrong type" };
    assert(!halo_save_snapshot_decode(encode(bad), @"retail-2276-runtime-1", &error));
    NSString *hash = [@"" stringByPaddingToLength:64 withString:@"a" startingAtIndex:0];
    NSURL *game = [root URLByAppendingPathComponent:@"game-private"];
    assert([@{@"sha256":hash} writeToURL:[game URLByAppendingPathComponent:@"integrity.plist"] atomically:YES]);
    halo_save_checkpoint_configure(root.fileSystemRepresentation, game.fileSystemRepresentation);
    halo_save_checkpoint_committed();
    NSURL *candidate = [root URLByAppendingPathComponent:@"Cloud Saves/local-checkpoint.plist"];
    NSData *captured = [NSData dataWithContentsOfURL:candidate];
    assert(halo_save_snapshot_decode(captured, [@"halo-native-save-v1:" stringByAppendingString:hash], &error));
    assert(halo_save_checkpoint_prepare());
    assert([[NSData dataWithContentsOfURL:candidate] isEqual:captured]);
    fixture_write(root, @"z/saved/player_profiles/default/00.sav", @"offline profile change");
    assert(halo_save_checkpoint_prepare());
    NSData *updated = [NSData dataWithContentsOfURL:candidate];
    assert(![updated isEqual:captured]);
    NSDictionary *offline = halo_save_snapshot_decode(updated, [@"halo-native-save-v1:" stringByAppendingString:hash], nil);
    assert([offline[@"files"][@"z/saved/player_profiles/default/00.sav"][@"data"] isEqual:[@"offline profile change" dataUsingEncoding:NSUTF8StringEncoding]]);
    assert([@"broken" writeToURL:candidate atomically:YES encoding:NSUTF8StringEncoding error:nil]);
    assert(!halo_save_checkpoint_prepare());
    assert([[NSString stringWithContentsOfURL:candidate encoding:NSUTF8StringEncoding error:nil] isEqual:@"broken"]);
    assert([fm removeItemAtURL:candidate error:nil]);
    halo_save_checkpoint_configure(root.fileSystemRepresentation, root.fileSystemRepresentation);
    halo_save_checkpoint_committed(); // no trusted game identity: do not enqueue
    assert(![fm fileExistsAtPath:candidate.path]);
    assert([fm createSymbolicLinkAtURL:[root URLByAppendingPathComponent:@"u/UDATA/link"] withDestinationURL:[root URLByAppendingPathComponent:@"game-private/disc.iso"] error:nil]);
    assert(!halo_save_snapshot_create(root, @"retail-2276-runtime-1", &error));
    NSURL *archive = [root URLByAppendingPathComponent:@"snapshot.plist"];
    assert([data writeToURL:archive atomically:YES]);
    for (int cut = 0; cut <= 10; cut++) {
        NSURL *live = [root URLByAppendingPathComponent:[NSString stringWithFormat:@"case-%d", cut]];
        fixture_write(live, @"u/UDATA/profile/savegame.bin", @"old checkpoint");
        fixture_write(live, @"z/saved/player_profiles/default/00.sav", @"old profile");
        fixture_write(live, @"z/cache000.map", @"untouched cache");
        fixture_write(live, @"z/lastprof.txt", @"old selection");
        fixture_write(live, @"z/last_solo.txt", @"old map");
        NSTask *task = [NSTask new];
        task.executableURL = [NSURL fileURLWithPath:[NSString stringWithUTF8String:argv[0]]];
        task.arguments = @[live.path, archive.path, @"retail-2276-runtime-1"];
        NSMutableDictionary *env = [NSProcessInfo.processInfo.environment mutableCopy];
        env[@"HALO_TEST_RESTORE_CUT"] = [NSString stringWithFormat:@"%d", cut];
        task.environment = env;
        assert([task launchAndReturnError:&error]); [task waitUntilExit];
        assert(task.terminationStatus == (cut ? 86 : 0));
        assert(halo_save_restore_recover(live, &error));
        assert(halo_save_restore_recover(live, &error)); // recovery is idempotent
        BOOL committed = cut == 0 || cut == 10;
        NSString *checkpoint = [NSString stringWithContentsOfURL:[live URLByAppendingPathComponent:@"u/UDATA/profile/savegame.bin"] encoding:NSUTF8StringEncoding error:nil];
        NSString *profile = [NSString stringWithContentsOfURL:[live URLByAppendingPathComponent:@"z/saved/player_profiles/default/00.sav"] encoding:NSUTF8StringEncoding error:nil];
        assert([checkpoint isEqual:committed ? @"checkpoint" : @"old checkpoint"]);
        assert([profile isEqual:committed ? @"profile" : @"old profile"]);
        assert([[NSString stringWithContentsOfURL:[live URLByAppendingPathComponent:@"z/lastprof.txt"] encoding:NSUTF8StringEncoding error:nil] isEqual:committed ? @"u:\\UDATA\\profile" : @"old selection"]);
        assert([[NSString stringWithContentsOfURL:[live URLByAppendingPathComponent:@"z/last_solo.txt"] encoding:NSUTF8StringEncoding error:nil] isEqual:committed ? @"levels\\b30\\b30" : @"old map"]);
        assert([[NSString stringWithContentsOfURL:[live URLByAppendingPathComponent:@"z/cache000.map"] encoding:NSUTF8StringEncoding error:nil] isEqual:@"untouched cache"]);
    }
    // Older two-directory journals must remain recoverable after upgrading.
    NSURL *legacyRoot = [root URLByAppendingPathComponent:@"legacy-journal"];
    fixture_write(legacyRoot, @"u/UDATA/file", @"replacement");
    fixture_write(legacyRoot, @"z/saved/file", @"replacement");
    fixture_write(legacyRoot, @".halo-save-restore/old-0/file", @"legacy original");
    fixture_write(legacyRoot, @".halo-save-restore/old-1/file", @"legacy profile");
    assert(([@{@"version":@1, @"hadOriginal":@[@YES,@YES], @"committed":@NO}
        writeToURL:[legacyRoot URLByAppendingPathComponent:@".halo-save-restore/state.plist"] atomically:YES]));
    assert(halo_save_restore_recover(legacyRoot, &error));
    assert([[NSString stringWithContentsOfURL:[legacyRoot URLByAppendingPathComponent:@"u/UDATA/file"] encoding:NSUTF8StringEncoding error:nil] isEqual:@"legacy original"]);
    NSMutableDictionary *withoutSelection = [snapshot mutableCopy];
    NSMutableDictionary *olderFiles = [snapshot[@"files"] mutableCopy];
    [olderFiles removeObjectsForKeys:@[@"z/lastprof.txt", @"z/last_solo.txt"]];
    withoutSelection[@"files"] = olderFiles;
    fixture_write(legacyRoot, @"z/lastprof.txt", @"wrong device profile");
    fixture_write(legacyRoot, @"z/last_solo.txt", @"wrong device map");
    assert(halo_save_restore(encode(withoutSelection), @"retail-2276-runtime-1", legacyRoot, &error));
    assert(![fm fileExistsAtPath:[legacyRoot URLByAppendingPathComponent:@"z/lastprof.txt"].path]);
    assert(![fm fileExistsAtPath:[legacyRoot URLByAppendingPathComponent:@"z/last_solo.txt"].path]);
    // A folder selected from an old app is copied now, applied only at the
    // next stopped-guest launch, with both source and prior local saves kept.
    NSURL *oldApp = [root URLByAppendingPathComponent:@"old-app"];
    NSURL *newApp = [root URLByAppendingPathComponent:@"new-app"];
    fixture_write(oldApp, @"u/UDATA/player/savegame.bin", @"old app progress");
    fixture_write(oldApp, @"z/cache000.map", @"large disposable map cache");
    fixture_write(newApp, @"u/UDATA/player/savegame.bin", @"new app progress");
    assert(!halo_save_import_queue(newApp, @"game-1", newApp, &error));
    NSURL *alias = [root URLByAppendingPathComponent:@"current-save-alias"];
    assert([fm createSymbolicLinkAtURL:alias withDestinationURL:newApp error:nil]);
    assert(!halo_save_import_queue(alias, @"game-1", newApp, &error));
    assert(halo_save_import_queue(oldApp, @"game-1", newApp, &error));
    assert(!halo_save_import_queue(oldApp, @"game-1", newApp, &error));
    NSURL *liveSave = [newApp URLByAppendingPathComponent:@"u/UDATA/player/savegame.bin"];
    assert([[NSString stringWithContentsOfURL:liveSave encoding:NSUTF8StringEncoding error:nil] isEqual:@"new app progress"]);
    assert(!halo_save_import_apply(@"wrong game", newApp, &error));
    assert(halo_save_import_apply(@"game-1", newApp, &error));
    assert(halo_save_import_apply(@"game-1", newApp, &error));
    assert([[NSString stringWithContentsOfURL:liveSave encoding:NSUTF8StringEncoding error:nil] isEqual:@"old app progress"]);
    assert([fm fileExistsAtPath:[oldApp URLByAppendingPathComponent:@"u/UDATA/player/savegame.bin"].path]);
    assert(![fm fileExistsAtPath:[newApp URLByAppendingPathComponent:@"z/cache000.map"].path]);
    BOOL keptOriginal = NO;
    for (NSURL *backup in [fm contentsOfDirectoryAtURL:[newApp URLByAppendingPathComponent:@"Save Imports"] includingPropertiesForKeys:nil options:0 error:nil]) {
        NSDictionary *saved = halo_save_snapshot_decode([NSData dataWithContentsOfURL:backup], @"game-1", nil);
        NSData *value = saved[@"files"][@"u/UDATA/player/savegame.bin"][@"data"];
        keptOriginal |= [value isEqual:[@"new app progress" dataUsingEncoding:NSUTF8StringEncoding]];
    }
    assert(keptOriginal);
    assert(halo_save_snapshot_decode([NSData dataWithContentsOfURL:[newApp URLByAppendingPathComponent:@"Cloud Saves/local-checkpoint.plist"]], @"game-1", nil));
    NSURL *empty = [root URLByAppendingPathComponent:@"empty"];
    assert([fm createDirectoryAtURL:empty withIntermediateDirectories:YES attributes:nil error:nil]);
    assert(!halo_save_import_queue(empty, @"game-1", newApp, &error));
    assert([fm removeItemAtURL:root error:nil]);
    puts("Snapshot and restore tests passed: ten process-death recovery points, round-trip, cache/ISO exclusion, compatibility, corruption, traversal, collisions, and symlinks");
} }
