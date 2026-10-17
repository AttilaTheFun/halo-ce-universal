#import "save_restore.h"
#import "save_snapshot.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static BOOL failure(NSError **error, NSString *text) {
    if (error) *error = [NSError errorWithDomain:@"HaloSaveRestore" code:1 userInfo:@{NSLocalizedDescriptionKey:text}];
    return NO;
}
static BOOL directory(NSURL *url) {
    struct stat st;
    return !lstat(url.fileSystemRepresentation, &st) && S_ISDIR(st.st_mode);
}
static BOOL exists(NSURL *url) {
    struct stat st;
    return !lstat(url.fileSystemRepresentation, &st);
}
static BOOL syncURL(NSURL *url, NSError **error) {
    int fd = open(url.fileSystemRepresentation, O_RDONLY | O_NOFOLLOW);
    BOOL ok = fd >= 0 && !fsync(fd);
    if (fd >= 0) close(fd);
    return ok || failure(error, @"Could not durably save the restore transaction.");
}
static BOOL move(NSURL *source, NSURL *target, NSError **error) {
    if (![NSFileManager.defaultManager moveItemAtURL:source toURL:target error:error]) return NO;
    return syncURL(source.URLByDeletingLastPathComponent, error) && syncURL(target.URLByDeletingLastPathComponent, error);
}
static void checkpoint(void) {
#ifdef HALO_SAVE_RESTORE_TEST
    static int step;
    const char *cut = getenv("HALO_TEST_RESTORE_CUT");
    if (cut && ++step == atoi(cut)) _exit(86);
#endif
}
static NSArray *prefixes(void) { return @[@"u/UDATA", @"z/saved"]; }
static NSURL *transaction(NSURL *root) { return [root URLByAppendingPathComponent:@".halo-save-restore"]; }
static BOOL writeState(NSDictionary *state, NSURL *txn, NSError **error) {
    NSURL *url = [txn URLByAppendingPathComponent:@"state.plist"];
    NSData *bytes = [NSPropertyListSerialization dataWithPropertyList:state format:NSPropertyListBinaryFormat_v1_0 options:0 error:error];
    return bytes && [bytes writeToURL:url options:NSDataWritingAtomic error:error] && syncURL(url, error) && syncURL(txn, error);
}
BOOL halo_save_restore_recover(NSURL *root, NSError **error) {
    NSFileManager *fm = NSFileManager.defaultManager;
    if (!directory(root)) return failure(error, @"The save root is unavailable.");
    NSURL *txn = transaction(root);
    if (!exists(txn)) return YES;
    if (!directory(txn)) return failure(error, @"The save restore journal is not a private directory.");
    NSURL *stateURL = [txn URLByAppendingPathComponent:@"state.plist"];
    NSDictionary *state = [NSDictionary dictionaryWithContentsOfURL:stateURL];
    // No journal means preparation was interrupted before any live path moved.
    if (!exists(stateURL)) return [fm removeItemAtURL:txn error:error];
    if (![state[@"version"] isEqual:@1] || ![state[@"hadOriginal"] isKindOfClass:NSArray.class] ||
        [state[@"hadOriginal"] count] != prefixes().count ||
        ![state[@"committed"] isKindOfClass:NSNumber.class])
        return failure(error, @"The save restore journal is invalid; original saves were retained.");
    for (id value in state[@"hadOriginal"])
        if (![value isKindOfClass:NSNumber.class]) return failure(error, @"Invalid save restore journal.");
    if (![state[@"committed"] boolValue]) {
        for (NSUInteger i = 0; i < prefixes().count; i++) {
            NSURL *live = [root URLByAppendingPathComponent:prefixes()[i]];
            NSURL *old = [txn URLByAppendingPathComponent:[NSString stringWithFormat:@"old-%lu", (unsigned long)i]];
            if (!directory(live.URLByDeletingLastPathComponent)) return failure(error, @"The save directory changed during restore.");
            if (exists(old)) {
                if (!directory(old)) return failure(error, @"Invalid saved backup directory.");
                if (exists(live) && ![fm removeItemAtURL:live error:error]) return NO;
                if (!move(old, live, error)) return NO;
            } else if (![state[@"hadOriginal"][i] boolValue] && exists(live)) {
                if (![fm removeItemAtURL:live error:error] || !syncURL(live.URLByDeletingLastPathComponent, error)) return NO;
            }
        }
    }
    return [fm removeItemAtURL:txn error:error] && syncURL(root, error);
}
BOOL halo_save_restore(NSData *snapshot, NSString *compatibility, NSURL *root, NSError **error) {
    NSFileManager *fm = NSFileManager.defaultManager;
    if (!directory(root)) return failure(error, @"The save root is unavailable.");
    if (!halo_save_restore_recover(root, error)) return NO;
    // Validate everything before creating a journal or touching live files.
    if (!halo_save_snapshot_decode(snapshot, compatibility, error)) return NO;
    NSMutableArray *had = [NSMutableArray new];
    for (NSString *prefix in prefixes()) {
        NSURL *live = [root URLByAppendingPathComponent:prefix];
        NSURL *parent = live.URLByDeletingLastPathComponent;
        if (!exists(parent) && ![fm createDirectoryAtURL:parent withIntermediateDirectories:NO attributes:nil error:error]) return NO;
        if (!directory(parent) || (exists(live) && !directory(live)))
            return failure(error, @"Save restore does not follow symbolic links.");
        [had addObject:@(exists(live))];
    }
    NSURL *txn = transaction(root);
    if (![fm createDirectoryAtURL:txn withIntermediateDirectories:NO attributes:@{NSFilePosixPermissions:@0700} error:error]) return NO;
    NSURL *stage = halo_save_snapshot_stage(snapshot, compatibility, txn, error);
    if (!stage) { [fm removeItemAtURL:txn error:nil]; return NO; }
    for (NSString *prefix in prefixes()) {
        NSURL *new = [stage URLByAppendingPathComponent:prefix];
        if (![fm createDirectoryAtURL:new withIntermediateDirectories:YES attributes:nil error:error]) return NO;
    }
    // Sync staged files before publishing the journal that authorizes moves.
    NSDirectoryEnumerator *entries = [fm enumeratorAtURL:stage includingPropertiesForKeys:nil options:0 errorHandler:nil];
    for (NSURL *url in entries) if (!syncURL(url, error)) return NO;
    if (!syncURL(stage, error)) return NO;
    NSMutableDictionary *state = [@{@"version":@1, @"hadOriginal":had, @"committed":@NO} mutableCopy];
    if (!writeState(state, txn, error) || !syncURL(root, error)) return NO;
    checkpoint();
    for (NSUInteger i = 0; i < prefixes().count; i++) {
        NSURL *live = [root URLByAppendingPathComponent:prefixes()[i]];
        NSURL *old = [txn URLByAppendingPathComponent:[NSString stringWithFormat:@"old-%lu", (unsigned long)i]];
        if ([had[i] boolValue] && !move(live, old, error)) return NO;
        checkpoint();
        if (!move([stage URLByAppendingPathComponent:prefixes()[i]], live, error)) return NO;
        checkpoint();
    }
    state[@"committed"] = @YES;
    if (!writeState(state, txn, error)) return NO;
    checkpoint();
    return halo_save_restore_recover(root, error);
}

#include "save_runtime.h"
int halo_save_recover_before_launch(const char *path, char *message, size_t capacity) {
    @autoreleasepool {
        NSError *error = nil;
        BOOL ok = halo_save_restore_recover([NSURL fileURLWithPath:[NSString stringWithUTF8String:path]], &error);
        if (!ok && capacity) snprintf(message, capacity, "%s", error.localizedDescription.UTF8String ?: "Could not recover saved progress.");
        return ok;
    }
}
