#import "../save_snapshot.h"
#include <assert.h>

static NSData *encode(NSDictionary *record) {
    return [NSPropertyListSerialization dataWithPropertyList:record format:NSPropertyListBinaryFormat_v1_0 options:0 error:nil];
}
static void fixture_write(NSURL *root, NSString *path, NSString *value) {
    NSURL *file = [root URLByAppendingPathComponent:path];
    assert([NSFileManager.defaultManager createDirectoryAtURL:file.URLByDeletingLastPathComponent withIntermediateDirectories:YES attributes:nil error:nil]);
    assert([value writeToURL:file atomically:YES encoding:NSUTF8StringEncoding error:nil]);
}
int main(void) { @autoreleasepool {
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
    assert([fm removeItemAtURL:root error:nil]);
    puts("Snapshot round-trip, cache/ISO exclusion, compatibility, corruption, traversal, collisions, and symlink rejection passed");
} }
