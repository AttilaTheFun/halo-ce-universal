#import "save_snapshot.h"
#import <CommonCrypto/CommonDigest.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static const NSUInteger maximumBytes = 128 * 1024 * 1024;
static const NSUInteger maximumFiles = 4096;
static BOOL fail(NSError **error, NSString *message) {
    if (error) *error = [NSError errorWithDomain:@"HaloSaveSnapshot" code:1
        userInfo:@{NSLocalizedDescriptionKey:message}];
    return NO;
}
static NSString *digest(NSData *data) {
    unsigned char bytes[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(data.bytes, (CC_LONG)data.length, bytes);
    NSMutableString *result = [NSMutableString new];
    for (NSUInteger i = 0; i < sizeof(bytes); i++) [result appendFormat:@"%02x", bytes[i]];
    return result;
}
static BOOL allowedPath(NSString *path) {
    if (![path isKindOfClass:NSString.class] || path.length > 1024 ||
        [path containsString:@"\\"] || [path rangeOfString:[NSString stringWithFormat:@"%C", (unichar)0]].location != NSNotFound)
        return NO;
    if ([path isEqual:@"z/lastprof.txt"] || [path isEqual:@"z/last_solo.txt"]) return YES;
    NSArray *parts = [path componentsSeparatedByString:@"/"];
    if (parts.count < 3) return NO;
    for (NSString *part in parts)
        if (!part.length || [part isEqual:@"."] || [part isEqual:@".."]) return NO;
    NSString *prefix = [[parts subarrayWithRange:NSMakeRange(0, 2)] componentsJoinedByString:@"/"];
    return [prefix isEqual:@"u/UDATA"] || [prefix isEqual:@"z/saved"];
}
/* Bound allocation before reading; reject symlinks and detect modification
   during a read. The guest-quiescence requirement also covers multiple files. */
static NSData *readFile(NSURL *url, NSUInteger remaining, NSError **error) {
    int fd = open(url.fileSystemRepresentation, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    struct stat before, after;
    if (fd < 0 || fstat(fd, &before) || !S_ISREG(before.st_mode) || before.st_size < 0 ||
        (uint64_t)before.st_size > remaining) {
        if (fd >= 0) close(fd);
        fail(error, @"A save file is unavailable, linked, or too large to sync."); return nil;
    }
    NSMutableData *data = [NSMutableData dataWithLength:(NSUInteger)before.st_size];
    NSUInteger done = 0;
    while (done < data.length) {
        ssize_t n = read(fd, (char *)data.mutableBytes + done, data.length - done);
        if (n <= 0) break;
        done += n;
    }
    BOOL stable = !fstat(fd, &after) && done == data.length && before.st_size == after.st_size &&
        before.st_mtimespec.tv_sec == after.st_mtimespec.tv_sec &&
        before.st_mtimespec.tv_nsec == after.st_mtimespec.tv_nsec &&
        before.st_ctimespec.tv_sec == after.st_ctimespec.tv_sec &&
        before.st_ctimespec.tv_nsec == after.st_ctimespec.tv_nsec;
    close(fd);
    if (!stable) { fail(error, @"A save changed while preparing sync. Try again after saving finishes."); return nil; }
    return data;
}
NSData *halo_save_snapshot_create(NSURL *root, NSString *compatibility, NSError **error) {
    if (!compatibility.length) { fail(error, @"The game-data compatibility ID is missing."); return nil; }
    NSMutableDictionary *files = [NSMutableDictionary new];
    NSUInteger bytes = 0;
    NSFileManager *fm = NSFileManager.defaultManager;
    for (NSString *prefix in @[@"u/UDATA", @"z/saved"]) {
        // Check each ancestor without following links, not just the leaf file.
        NSURL *directory = root;
        BOOL exists = YES;
        for (NSString *part in [prefix componentsSeparatedByString:@"/"]) {
            directory = [directory URLByAppendingPathComponent:part];
            struct stat st;
            if (lstat(directory.fileSystemRepresentation, &st)) {
                if (errno == ENOENT) { exists = NO; break; }
                fail(error, @"Could not inspect the save directory."); return nil;
            }
            if (!S_ISDIR(st.st_mode)) { fail(error, @"Save directories must not be symbolic links."); return nil; }
        }
        if (!exists) continue;
        directory = directory.URLByResolvingSymlinksInPath.URLByStandardizingPath;
        __block NSError *enumerationError = nil;
        NSDirectoryEnumerator *enumerator = [fm enumeratorAtURL:directory includingPropertiesForKeys:nil options:0
            errorHandler:^BOOL(NSURL *url, NSError *failure) {
                (void)url; enumerationError = failure; return NO;
            }];
        for (NSURL *url in enumerator) {
            struct stat st;
            if (lstat(url.fileSystemRepresentation, &st)) { fail(error, @"A save disappeared during sync."); return nil; }
            if (S_ISDIR(st.st_mode)) continue;
            NSString *basePath = [directory.path.stringByStandardizingPath stringByAppendingString:@"/"];
            NSString *filePath = url.path.stringByStandardizingPath;
            if (![filePath hasPrefix:basePath]) { fail(error, @"A save path escaped its directory."); return nil; }
            NSString *relative = [filePath substringFromIndex:basePath.length];
            NSString *path = [prefix stringByAppendingFormat:@"/%@", relative];
            if (!allowedPath(path) || files.count >= maximumFiles) { fail(error, @"The save contains invalid paths or too many files."); return nil; }
            NSData *data = readFile(url, maximumBytes - bytes, error);
            if (!data) return nil;
            bytes += data.length;
            files[path] = @{@"data":data, @"sha256":digest(data)};
        }
        if (enumerationError) { if (error) *error = enumerationError; return nil; }
    }
    for (NSString *path in @[@"z/lastprof.txt", @"z/last_solo.txt"]) {
        NSURL *parent = [root URLByAppendingPathComponent:@"z"];
        struct stat st;
        if (lstat(parent.fileSystemRepresentation, &st)) {
            if (errno == ENOENT) continue;
            fail(error, @"Could not inspect save selection metadata."); return nil;
        }
        if (!S_ISDIR(st.st_mode)) { fail(error, @"Save metadata directories must not be links."); return nil; }
        NSURL *file = [root URLByAppendingPathComponent:path];
        if (lstat(file.fileSystemRepresentation, &st)) {
            if (errno == ENOENT) continue;
            fail(error, @"Could not inspect save selection metadata."); return nil;
        }
        if (files.count >= maximumFiles) { fail(error, @"Too many save files."); return nil; }
        NSData *data = readFile(file, MIN((NSUInteger)4096, maximumBytes - bytes), error);
        if (!data) return nil;
        bytes += data.length;
        files[path] = @{@"data":data, @"sha256":digest(data)};
    }
    NSDictionary *record = @{@"version":@1, @"compatibility":compatibility,
        @"snapshot":NSUUID.UUID.UUIDString, @"created":NSDate.date, @"files":files};
    NSData *result = [NSPropertyListSerialization dataWithPropertyList:record format:NSPropertyListBinaryFormat_v1_0 options:0 error:error];
    if (result.length > maximumBytes) { fail(error, @"The save snapshot exceeds the sync size limit."); return nil; }
    return result;
}
NSDictionary *halo_save_snapshot_decode(NSData *data, NSString *compatibility, NSError **error) {
    if (!data || data.length > maximumBytes) { fail(error, @"Invalid save snapshot size."); return nil; }
    id record = [NSPropertyListSerialization propertyListWithData:data options:NSPropertyListImmutable format:nil error:error];
    if (![record isKindOfClass:NSDictionary.class] || ![record[@"version"] isEqual:@1] ||
        ![record[@"compatibility"] isEqual:compatibility] || ![record[@"files"] isKindOfClass:NSDictionary.class] ||
        ![record[@"snapshot"] isKindOfClass:NSString.class] || ![[NSUUID alloc] initWithUUIDString:record[@"snapshot"]] ||
        ![record[@"created"] isKindOfClass:NSDate.class]) {
        fail(error, @"This save uses a different format or game-data version."); return nil;
    }
    NSDictionary *files = record[@"files"];
    NSMutableSet *paths = [NSMutableSet new];
    NSUInteger bytes = 0;
    if (files.count > maximumFiles) { fail(error, @"Too many files in the save snapshot."); return nil; }
    for (NSString *path in files) {
        id file = files[path];
        NSString *folded = [path isKindOfClass:NSString.class] ? path.precomposedStringWithCanonicalMapping.lowercaseString : nil;
        if (!allowedPath(path) || [paths containsObject:folded] || ![file isKindOfClass:NSDictionary.class] ||
            ![file[@"data"] isKindOfClass:NSData.class] || ![file[@"sha256"] isEqual:digest(file[@"data"])]) {
            fail(error, @"The save snapshot has invalid paths or damaged data."); return nil;
        }
        if (([path isEqual:@"z/lastprof.txt"] || [path isEqual:@"z/last_solo.txt"]) && [file[@"data"] length] > 4096) {
            fail(error, @"Save selection metadata is too large."); return nil;
        }
        [paths addObject:folded];
        bytes += [file[@"data"] length];
        if (bytes > maximumBytes) { fail(error, @"The save snapshot is too large."); return nil; }
    }
    // Reject file/directory collisions before writing any bytes.
    for (NSString *path in paths) {
        NSString *ancestor = path.stringByDeletingLastPathComponent;
        while (ancestor.length) {
            if ([paths containsObject:ancestor]) { fail(error, @"Conflicting save paths."); return nil; }
            ancestor = ancestor.stringByDeletingLastPathComponent;
        }
    }
    return record;
}
NSURL *halo_save_snapshot_stage(NSData *data, NSString *compatibility, NSURL *parent, NSError **error) {
    NSDictionary *record = halo_save_snapshot_decode(data, compatibility, error);
    if (!record) return nil;
    NSURL *stage = [parent URLByAppendingPathComponent:[@"restore-" stringByAppendingString:NSUUID.UUID.UUIDString]];
    NSFileManager *fm = NSFileManager.defaultManager;
    if (![fm createDirectoryAtURL:stage withIntermediateDirectories:NO attributes:@{NSFilePosixPermissions:@0700} error:error]) return nil;
    for (NSString *path in record[@"files"]) {
        NSURL *file = [stage URLByAppendingPathComponent:path];
        if (![fm createDirectoryAtURL:file.URLByDeletingLastPathComponent withIntermediateDirectories:YES attributes:nil error:error] ||
            ![record[@"files"][path][@"data"] writeToURL:file options:NSDataWritingAtomic error:error]) {
            [fm removeItemAtURL:stage error:nil]; return nil;
        }
    }
    return stage;
}
