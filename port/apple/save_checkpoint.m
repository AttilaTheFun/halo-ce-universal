#import <Foundation/Foundation.h>
#import "save_snapshot.h"
#import "save_restore.h"
#include "save_runtime.h"
#include "cloud_runtime.h"
#include <fcntl.h>
#include <unistd.h>

/* Set before guest launch; checkpoint callbacks occur synchronously after the
   persistent save handle closes. Cloud transfers never read the live tree. */
static NSString *checkpointRoot;
static NSString *checkpointCompatibility;
void halo_save_checkpoint_configure(const char *saves, const char *data) {
    @autoreleasepool {
        checkpointRoot = [NSString stringWithUTF8String:saves];
        NSString *game = [NSString stringWithUTF8String:data];
        NSDictionary *manifest = [NSDictionary dictionaryWithContentsOfFile:[game stringByAppendingPathComponent:@"integrity.plist"]];
        NSString *hash = manifest[@"sha256"];
        NSCharacterSet *hex = [NSCharacterSet characterSetWithCharactersInString:@"0123456789abcdef"];
        if ([hash isKindOfClass:NSString.class] && hash.length == 64 &&
            [hash rangeOfCharacterFromSet:hex.invertedSet].location == NSNotFound)
            checkpointCompatibility = [@"halo-native-save-v1:" stringByAppendingString:hash];
        else checkpointCompatibility = nil;
    }
}
static BOOL captureCheckpoint(BOOL preparingLaunch) {
    @autoreleasepool {
        if (!checkpointRoot || !checkpointCompatibility) return NO;
        NSError *error = nil;
        NSURL *root = [NSURL fileURLWithPath:checkpointRoot isDirectory:YES];
        NSData *snapshot = halo_save_snapshot_create(root, checkpointCompatibility, &error);
        NSURL *directory = [root URLByAppendingPathComponent:@"Cloud Saves" isDirectory:YES];
        NSURL *file = [directory URLByAppendingPathComponent:@"local-checkpoint.plist"];
        if (preparingLaunch) {
            if (!snapshot) return NO;
            NSDictionary *current = halo_save_snapshot_decode(snapshot, checkpointCompatibility, &error);
            NSDictionary *attributes = [NSFileManager.defaultManager attributesOfItemAtPath:file.path error:nil];
            if (attributes) {
                if (![attributes[NSFileType] isEqual:NSFileTypeRegular] ||
                    [attributes[NSFileSize] unsignedLongLongValue] > 128 * 1024 * 1024) return NO;
                NSDictionary *previous = halo_save_snapshot_decode([NSData dataWithContentsOfURL:file], checkpointCompatibility, &error);
                if (!previous) return NO;
                // Preserve the snapshot identity for unchanged content; making
                // a fresh UUID on every launch would manufacture conflicts.
                if ([current[@"files"] isEqual:previous[@"files"]]) return YES;
                // A disappeared save tree must not overwrite progress in the
                // cloud with an empty snapshot or silently restore stale data.
                if (![current[@"files"] count]) return NO;
            } else if (![current[@"files"] count]) return YES;
        }
        BOOL ok = snapshot && [NSFileManager.defaultManager createDirectoryAtURL:directory withIntermediateDirectories:YES
            attributes:@{NSFilePosixPermissions:@0700} error:&error] && [snapshot writeToURL:file options:NSDataWritingAtomic error:&error];
        if (ok) {
            int fd = open(file.fileSystemRepresentation, O_RDONLY | O_NOFOLLOW);
            ok = fd >= 0 && !fsync(fd);
            if (fd >= 0) close(fd);
        }
        if (!ok) fprintf(stderr, "Could not prepare cloud checkpoint: %s\n", error.localizedDescription.UTF8String ?: "storage unavailable");
        if (ok) halo_cloud_checkpoint_ready();
        // This is a local candidate, not an acknowledged cloud revision. The
        // account-aware coordinator must compare it with the remote head.
        return ok;
    }
}
void halo_save_checkpoint_committed(void) { (void)captureCheckpoint(NO); }
int halo_save_checkpoint_prepare(void) { return captureCheckpoint(YES); }

int halo_save_import_before_launch(char *message, size_t capacity) {
    @autoreleasepool {
        NSError *error = nil;
        BOOL ok = checkpointRoot && halo_save_import_apply(checkpointCompatibility,
            [NSURL fileURLWithPath:checkpointRoot isDirectory:YES], &error);
        if (!ok && capacity) snprintf(message, capacity, "%s", error.localizedDescription.UTF8String ?: "Could not import saved progress.");
        return ok;
    }
}
