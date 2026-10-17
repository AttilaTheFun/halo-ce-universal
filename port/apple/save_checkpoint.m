#import <Foundation/Foundation.h>
#import "save_snapshot.h"
#include "save_runtime.h"
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
void halo_save_checkpoint_committed(void) {
    @autoreleasepool {
        if (!checkpointRoot || !checkpointCompatibility) return;
        NSError *error = nil;
        NSURL *root = [NSURL fileURLWithPath:checkpointRoot isDirectory:YES];
        NSData *snapshot = halo_save_snapshot_create(root, checkpointCompatibility, &error);
        NSURL *directory = [root URLByAppendingPathComponent:@"Cloud Saves" isDirectory:YES];
        NSURL *file = [directory URLByAppendingPathComponent:@"local-checkpoint.plist"];
        BOOL ok = snapshot && [NSFileManager.defaultManager createDirectoryAtURL:directory withIntermediateDirectories:YES
            attributes:@{NSFilePosixPermissions:@0700} error:&error] && [snapshot writeToURL:file options:NSDataWritingAtomic error:&error];
        if (ok) {
            int fd = open(file.fileSystemRepresentation, O_RDONLY | O_NOFOLLOW);
            ok = fd >= 0 && !fsync(fd);
            if (fd >= 0) close(fd);
        }
        if (!ok) fprintf(stderr, "Could not prepare cloud checkpoint: %s\n", error.localizedDescription.UTF8String ?: "storage unavailable");
        // This is a local candidate, not an acknowledged cloud revision. The
        // account-aware coordinator must compare it with the remote head.
    }
}
