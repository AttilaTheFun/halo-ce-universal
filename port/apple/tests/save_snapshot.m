#import "../save_snapshot.h"
#import "../save_restore.h"
#include <assert.h>

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
    NSError *error = nil;
    NSData *data = halo_save_snapshot_create(root, @"retail-2276-runtime-1", &error);
    assert(data && !error);
    NSDictionary *snapshot = halo_save_snapshot_decode(data, @"retail-2276-runtime-1", &error);
    assert(snapshot && [snapshot[@"files"] count] == 2);
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
    assert([fm createSymbolicLinkAtURL:[root URLByAppendingPathComponent:@"u/UDATA/link"] withDestinationURL:[root URLByAppendingPathComponent:@"game-private/disc.iso"] error:nil]);
    assert(!halo_save_snapshot_create(root, @"retail-2276-runtime-1", &error));
    NSURL *archive = [root URLByAppendingPathComponent:@"snapshot.plist"];
    assert([data writeToURL:archive atomically:YES]);
    for (int cut = 0; cut <= 6; cut++) {
        NSURL *live = [root URLByAppendingPathComponent:[NSString stringWithFormat:@"case-%d", cut]];
        fixture_write(live, @"u/UDATA/profile/savegame.bin", @"old checkpoint");
        fixture_write(live, @"z/saved/player_profiles/default/00.sav", @"old profile");
        fixture_write(live, @"z/cache000.map", @"untouched cache");
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
        BOOL committed = cut == 0 || cut == 6;
        NSString *checkpoint = [NSString stringWithContentsOfURL:[live URLByAppendingPathComponent:@"u/UDATA/profile/savegame.bin"] encoding:NSUTF8StringEncoding error:nil];
        NSString *profile = [NSString stringWithContentsOfURL:[live URLByAppendingPathComponent:@"z/saved/player_profiles/default/00.sav"] encoding:NSUTF8StringEncoding error:nil];
        assert([checkpoint isEqual:committed ? @"checkpoint" : @"old checkpoint"]);
        assert([profile isEqual:committed ? @"profile" : @"old profile"]);
        assert([[NSString stringWithContentsOfURL:[live URLByAppendingPathComponent:@"z/cache000.map"] encoding:NSUTF8StringEncoding error:nil] isEqual:@"untouched cache"]);
    }
    assert([fm removeItemAtURL:root error:nil]);
    puts("Snapshot and restore tests passed: six process-death recovery points, round-trip, cache/ISO exclusion, compatibility, corruption, traversal, collisions, and symlinks");
} }
