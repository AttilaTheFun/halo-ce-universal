#import "HaloUpdates.h"
#include <fcntl.h>
#include <spawn.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

@interface HaloForkUpdater () <NSURLSessionTaskDelegate, NSURLSessionDownloadDelegate>
@property(nonatomic, copy) void (^downloadProgress)(unsigned long long received, unsigned long long total);
@end

extern char **environ;

static NSError *updateFailure(NSString *message) {
    return [NSError errorWithDomain:@"HaloForkUpdate" code:1 userInfo:@{NSLocalizedDescriptionKey: message}];
}

static int lineIsSection(const char *line, const char *end, char *section, size_t size) {
    const char *close;
    while (line < end && (*line == ' ' || *line == '\t')) line++;
    if (line >= end || *line != '[') return 0;
    close = memchr(line, ']', (size_t)(end - line));
    if (!close || (size_t)(close - line - 1) >= size) return 0;
    memcpy(section, line + 1, (size_t)(close - line - 1));
    section[close - line - 1] = 0;
    return 1;
}

static int lineIsKey(const char *line, const char *end, const char *key) {
    size_t length = strlen(key);
    while (line < end && (*line == ' ' || *line == '\t')) line++;
    if ((size_t)(end - line) <= length || strncmp(line, key, length) != 0) return 0;
    line += length;
    while (line < end && (*line == ' ' || *line == '\t')) line++;
    return line < end && *line == '=';
}

static int lineValue(const char *line, const char *end, const char *key, char *value, size_t size) {
    const char *start;
    size_t length;
    if (!lineIsKey(line, end, key) || size == 0) return 0;
    start = line;
    while (start < end && (*start == ' ' || *start == '\t')) start++;
    start += strlen(key);
    while (start < end && (*start == ' ' || *start == '\t')) start++;
    if (start >= end || *start != '=') return 0;
    start++;
    while (start < end && (*start == ' ' || *start == '\t')) start++;
    length = (size_t)(end - start);
    if (length >= size) length = size - 1;
    memcpy(value, start, length);
    value[length] = 0;
    return 1;
}

static int configValue(NSString *text, const char *section, const char *key, char *value, size_t size) {
    const char *file = text.UTF8String ?: "";
    const char *line;
    char current[64] = "";
    int inSection = 0, found = 0;
    for (line = file; *line; ) {
        const char *end = line + strcspn(line, "\n");
        const char *next = *end ? end + 1 : end;
        if (lineIsSection(line, end, current, sizeof(current)))
            inSection = strcmp(current, section) == 0;
        else if (inSection && lineValue(line, end, key, value, size))
            found = 1;
        line = next;
    }
    return found;
}

static NSRegularExpression *tagPattern(void) {
    static NSRegularExpression *pattern;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        pattern = [NSRegularExpression regularExpressionWithPattern:
            @"^test-v[0-9.]+-net([0-9]+)-mac([0-9]+)$" options:0 error:nil];
    });
    return pattern;
}

static NSTextCheckingResult *tagMatch(NSString *tag) {
    if (![tag isKindOfClass:NSString.class] || !tag.length) return nil;
    return [tagPattern() firstMatchInString:tag options:0 range:NSMakeRange(0, tag.length)];
}

static NSString *tagGroup(NSString *tag, NSTextCheckingResult *match, NSUInteger index) {
    NSRange range = [match rangeAtIndex:index];
    if (range.location == NSNotFound) return nil;
    return [tag substringWithRange:range];
}

long HaloForkUpdateBuildFromTag(NSString *tag) {
    NSTextCheckingResult *match = tagMatch(tag);
    if (!match) return 0;
    return tagGroup(tag, match, 2).integerValue;
}

NSString *HaloForkUpdateAssetName(NSString *tag) {
    NSTextCheckingResult *match = tagMatch(tag);
    if (!match) return nil;
    return [NSString stringWithFormat:@"OpenCE-macos-arm64-net%@-mac%@.dmg",
        tagGroup(tag, match, 1), tagGroup(tag, match, 2)];
}

BOOL HaloForkUpdateDownloadIsTrusted(NSString *tag, NSString *url) {
    NSString *asset = HaloForkUpdateAssetName(tag);
    if (!asset || ![url isKindOfClass:NSString.class]) return NO;
    NSString *expected = [NSString stringWithFormat:
        @"https://github.com/innerhat-dev/OpenCE/releases/download/%@/%@", tag, asset];
    return [url isEqualToString:expected];
}

static NSString *macDownloadURL(NSDictionary *release) {
    NSString *tag = release[@"tag_name"];
    NSString *asset = HaloForkUpdateAssetName(tag);
    NSString *found = nil;
    id assets = release[@"assets"];
    if (!asset) return nil;
    for (id item in [assets isKindOfClass:NSArray.class] ? assets : @[]) {
        if (![item isKindOfClass:NSDictionary.class]) continue;
        if ([item[@"name"] isEqual:asset]) found = item[@"browser_download_url"];
    }
    return HaloForkUpdateDownloadIsTrusted(tag, found) ? found : nil;
}

long HaloForkNewestMacRelease(NSArray *releases, NSString **tagOut, NSString **urlOut) {
    long best = 0;
    NSString *tag = nil;
    NSString *url = nil;
    if (![releases isKindOfClass:NSArray.class]) return 0;
    for (id item in releases) {
        NSDictionary *release = [item isKindOfClass:NSDictionary.class] ? item : nil;
        long build;
        NSString *download;
        if (!release || [release[@"draft"] boolValue] || [release[@"prerelease"] boolValue]) continue;
        build = HaloForkUpdateBuildFromTag(release[@"tag_name"]);
        download = macDownloadURL(release);
        if (build <= best || !download) continue;
        best = build;
        tag = release[@"tag_name"];
        url = download;
    }
    if (tagOut) *tagOut = tag;
    if (urlOut) *urlOut = url;
    return best;
}

BOOL HaloConfigAutomaticUpdates(NSString *text) {
    char value[32];
    if (!configValue(text, "update", "auto", value, sizeof(value))) return YES;
    return strcmp(value, "true") == 0;
}

NSString *HaloConfigSetAutomaticUpdates(NSString *text, BOOL automatic) {
    const char *file = text.UTF8String ?: "";
    const char *replacement = automatic ? "auto = true\n" : "auto = false\n";
    NSMutableData *out = [NSMutableData data];
    const char *line;
    char current[64] = "";
    int written = 0, inSection = 0;
    for (line = file; *line; ) {
        const char *end = line + strcspn(line, "\n");
        const char *next = *end ? end + 1 : end;
        if (lineIsSection(line, end, current, sizeof(current))) {
            if (inSection && !written) {
                [out appendBytes:replacement length:strlen(replacement)];
                written = 1;
            }
            inSection = strcmp(current, "update") == 0;
        } else if (inSection && !written && lineIsKey(line, end, "auto")) {
            [out appendBytes:replacement length:strlen(replacement)];
            written = 1;
            line = next;
            continue;
        }
        [out appendBytes:line length:(NSUInteger)(next - line)];
        line = next;
    }
    if (!written) {
        if (out.length && ((const char *)out.bytes)[out.length - 1] != '\n')
            [out appendBytes:"\n" length:1];
        if (!inSection) [out appendBytes:"\n[update]\n" length:10];
        [out appendBytes:replacement length:strlen(replacement)];
    }
    return [[NSString alloc] initWithData:out encoding:NSUTF8StringEncoding] ?: @"";
}

static NSString *quotedValue(const char *value) {
    size_t length = strlen(value);
    if (length < 2 || value[0] != '"') return @"";
    NSMutableString *text = [NSMutableString string];
    for (size_t index = 1; index < length; index++) {
        if (value[index] == '\\' && index + 1 < length) {
            [text appendFormat:@"%c", value[++index]];
            continue;
        }
        if (value[index] == '"') break;
        [text appendFormat:@"%c", value[index]];
    }
    return text;
}

BOOL HaloConfigSuppressesUpdatePrompt(NSString *text) {
    char value[256];
    if (configValue(text, "debug", "hidden_window", value, sizeof(value)) && strcmp(value, "true") == 0)
        return YES;
    if (configValue(text, "debug", "exit_after", value, sizeof(value)) && strtod(value, NULL) > 0.0)
        return YES;
    if (configValue(text, "debug", "network_test", value, sizeof(value)) && quotedValue(value).length)
        return YES;
    return NO;
}

NSString *HaloConfigUpdateAnswer(NSString *text) {
    char value[64];
    NSString *answer;
    if (!configValue(text, "debug", "update_answer", value, sizeof(value))) return nil;
    answer = quotedValue(value);
    return answer.length ? answer : nil;
}

@implementation HaloForkUpdater {
    NSURL *_configURL;
    NSURL *_bundleURL;
    NSURLSession *_session;
    NSURL *_downloadDestination;
    NSError *_downloadError;
    dispatch_semaphore_t _downloadGate;
}
- (instancetype)initWithBuild:(long)build configURL:(NSURL *)configURL bundleURL:(NSURL *)bundleURL {
    if ((self = [super init])) {
        _build = build;
        _configURL = configURL;
        _bundleURL = bundleURL;
        NSURLSessionConfiguration *configuration = NSURLSessionConfiguration.ephemeralSessionConfiguration;
        configuration.timeoutIntervalForRequest = 60;
        configuration.timeoutIntervalForResource = 600;
        _session = [NSURLSession sessionWithConfiguration:configuration delegate:self delegateQueue:nil];
    }
    return self;
}
- (NSString *)configText {
    return [NSString stringWithContentsOfURL:_configURL encoding:NSUTF8StringEncoding error:nil] ?: @"";
}
- (BOOL)automatic { return HaloConfigAutomaticUpdates([self configText]); }
- (BOOL)setAutomatic:(BOOL)automatic error:(NSError **)error {
    NSString *updated = HaloConfigSetAutomaticUpdates([self configText], automatic);
    NSData *data = [updated dataUsingEncoding:NSUTF8StringEncoding];
    if (!data || ![data writeToURL:_configURL options:NSDataWritingAtomic error:error]) return NO;
    return YES;
}
- (BOOL)suppressesPrompt { return HaloConfigSuppressesUpdatePrompt([self configText]); }
- (NSString *)updateAnswer { return HaloConfigUpdateAnswer([self configText]); }

- (BOOL)redirectIsAllowed:(NSURL *)url {
    NSString *host = url.host.lowercaseString;
    return [host isEqualToString:@"api.github.com"] ||
        [host isEqualToString:@"github.com"] ||
        [host isEqualToString:@"objects.githubusercontent.com"] ||
        [host isEqualToString:@"release-assets.githubusercontent.com"] ||
        [host isEqualToString:@"github-releases.githubusercontent.com"];
}
- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task
    willPerformHTTPRedirection:(NSHTTPURLResponse *)response newRequest:(NSURLRequest *)request
    completionHandler:(void (^)(NSURLRequest *))completionHandler {
    (void)session; (void)task; (void)response;
    completionHandler([self redirectIsAllowed:request.URL] ? request : nil);
}
- (void)URLSession:(NSURLSession *)session downloadTask:(NSURLSessionDownloadTask *)task
    didWriteData:(int64_t)bytesWritten totalBytesWritten:(int64_t)written
    totalBytesExpectedToWrite:(int64_t)expected {
    (void)session; (void)task; (void)bytesWritten;
    void (^progress)(unsigned long long, unsigned long long) = self.downloadProgress;
    if (progress) progress((unsigned long long)written, expected > 0 ? (unsigned long long)expected : 0);
}
- (void)URLSession:(NSURLSession *)session downloadTask:(NSURLSessionDownloadTask *)task
    didFinishDownloadingToURL:(NSURL *)location {
    (void)session; (void)task;
    NSError *moveError = nil;
    if (![NSFileManager.defaultManager moveItemAtURL:location toURL:_downloadDestination error:&moveError])
        _downloadError = moveError ?: updateFailure(@"The download could not be saved.");
}
- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didCompleteWithError:(NSError *)error {
    (void)session;
    if (![task isKindOfClass:NSURLSessionDownloadTask.class]) return;
    NSInteger status = [task.response isKindOfClass:NSHTTPURLResponse.class]
        ? ((NSHTTPURLResponse *)task.response).statusCode : 0;
    if (error) _downloadError = error;
    else if (status != 200)
        _downloadError = updateFailure(@"The download did not finish.");
    if (_downloadGate) dispatch_semaphore_signal(_downloadGate);
}

- (NSData *)fetch:(NSURL *)url method:(NSString *)method error:(NSError **)error {
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:url];
    request.HTTPMethod = method;
    [request setValue:@"OpenCE-Mac" forHTTPHeaderField:@"User-Agent"];
    [request setValue:@"application/vnd.github+json" forHTTPHeaderField:@"Accept"];
    dispatch_semaphore_t gate = dispatch_semaphore_create(0);
    __block NSData *body = nil;
    __block NSError *taskError = nil;
    __block NSInteger status = 0;
    NSURLSessionDataTask *task = [_session dataTaskWithRequest:request
        completionHandler:^(NSData *data, NSURLResponse *response, NSError *failure) {
        body = data;
        taskError = failure;
        if ([response isKindOfClass:NSHTTPURLResponse.class]) status = ((NSHTTPURLResponse *)response).statusCode;
        dispatch_semaphore_signal(gate);
    }];
    [task resume];
    if (dispatch_semaphore_wait(gate, dispatch_time(DISPATCH_TIME_NOW, 40 * NSEC_PER_SEC))) {
        [task cancel];
        if (error) *error = updateFailure(@"The update check timed out.");
        return nil;
    }
    if (taskError || status != 200) {
        if (error) *error = taskError ?: updateFailure(@"GitHub did not return the latest release.");
        return nil;
    }
    return body;
}

- (void)checkQuiet:(BOOL)quiet completion:(void (^)(NSError *, NSString *, NSURL *, long))completion {
    (void)quiet;
    long current = self.build;
    NSURLSession *session = _session;
    (void)session;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
        NSError *error = nil;
        NSString *tag = nil;
        NSURL *download = nil;
        long latest = 0;
        /* The newest tag may be an iOS release. Look through the page for a
           Mac disk image instead of treating that tag as this app's update. */
        NSURL *api = [NSURL URLWithString:@"https://api.github.com/repos/innerhat-dev/OpenCE/releases?per_page=100"];
        NSData *body = [self fetch:api method:@"GET" error:&error];
        id json = body ? [NSJSONSerialization JSONObjectWithData:body options:0 error:&error] : nil;
        if (!error && ![json isKindOfClass:NSArray.class])
            error = updateFailure(@"GitHub returned a release list this build could not read.");
        if (!error) {
            NSString *found = nil;
            latest = HaloForkNewestMacRelease(json, &tag, &found);
            if (latest > current && found)
                download = [NSURL URLWithString:found];
            else
                tag = nil;
        }
        dispatch_async(dispatch_get_main_queue(), ^{
            completion(error, tag, download, latest);
        });
    });
}

- (NSString *)runTool:(NSString *)path arguments:(NSArray<NSString *> *)arguments output:(NSString **)output error:(NSError **)error {
    NSTask *task = [[NSTask alloc] init];
    task.executableURL = [NSURL fileURLWithPath:path];
    task.arguments = arguments;
    NSPipe *outPipe = [NSPipe pipe];
    NSPipe *errPipe = [NSPipe pipe];
    task.standardOutput = outPipe;
    task.standardError = errPipe;
    if (![task launchAndReturnError:error]) return nil;
    /* Read both pipes at once. Waiting on one first can stall the tool once
       the other pipe fills. */
    __block NSData *data = nil;
    __block NSData *errData = nil;
    dispatch_group_t pipes = dispatch_group_create();
    dispatch_group_async(pipes, dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
        data = [outPipe.fileHandleForReading readDataToEndOfFile];
    });
    dispatch_group_async(pipes, dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
        errData = [errPipe.fileHandleForReading readDataToEndOfFile];
    });
    [task waitUntilExit];
    dispatch_group_wait(pipes, DISPATCH_TIME_FOREVER);
    NSString *text = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] ?: @"";
    NSString *errText = [[NSString alloc] initWithData:errData encoding:NSUTF8StringEncoding] ?: @"";
    if (output) *output = text;
    if (task.terminationStatus != 0) {
        NSString *reason = errText.length ? errText : text;
        if (error) *error = updateFailure(reason.length ? reason : @"A disk image tool failed.");
        return nil;
    }
    return text;
}

- (BOOL)installDownload:(NSURL *)download expectedBuild:(long)build error:(NSError **)error
               progress:(void (^)(unsigned long long, unsigned long long))progress {
    /* path is /innerhat-dev/OpenCE/releases/download/<tag>/<asset> */
    NSArray<NSString *> *parts = [download.path componentsSeparatedByString:@"/"];
    NSString *tag = parts.count >= 2 ? parts[parts.count - 2] : nil;
    if (!HaloForkUpdateDownloadIsTrusted(tag, download.absoluteString)) {
        if (error) *error = updateFailure(@"That download is not a release from this fork.");
        return NO;
    }
    if (build <= self.build || ![_bundleURL.lastPathComponent isEqualToString:@"OpenCE.app"]) {
        if (error) *error = updateFailure(@"This copy of OpenCE cannot replace itself.");
        return NO;
    }
    if (![NSFileManager.defaultManager isWritableFileAtPath:_bundleURL.URLByDeletingLastPathComponent.path]) {
        if (error) *error = updateFailure(@"OpenCE cannot replace the installed app. It is not writable.");
        return NO;
    }
    NSString *root = NSTemporaryDirectory();
    NSURL *dmg = [NSURL fileURLWithPath:[root stringByAppendingPathComponent:
        [[NSUUID UUID].UUIDString stringByAppendingString:@".dmg"]]];
    NSURL *staged = [NSURL fileURLWithPath:[root stringByAppendingPathComponent:
        [NSUUID UUID].UUIDString] isDirectory:YES];
    _downloadDestination = dmg;
    _downloadError = nil;
    _downloadGate = dispatch_semaphore_create(0);
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:download];
    [request setValue:@"OpenCE-Mac" forHTTPHeaderField:@"User-Agent"];
    self.downloadProgress = progress;
    [[_session downloadTaskWithRequest:request] resume];
    dispatch_semaphore_wait(_downloadGate, DISPATCH_TIME_FOREVER);
    self.downloadProgress = nil;
    _downloadGate = nil;
    if (_downloadError) {
        if (error) *error = _downloadError;
        [NSFileManager.defaultManager removeItemAtURL:dmg error:nil];
        return NO;
    }
    /* A browser download is marked so Gatekeeper can ask about it. This file
       is the same fork the user already runs, and that mark can stop the
       image from mounting. */
    [self runTool:@"/usr/bin/xattr" arguments:@[@"-c", dmg.path] output:nil error:nil];
    NSString *listing = nil;
    if (![self runTool:@"/usr/bin/hdiutil" arguments:@[@"attach", @"-nobrowse", @"-readonly", @"-plist", dmg.path]
            output:&listing error:error]) {
        [NSFileManager.defaultManager removeItemAtURL:dmg error:nil];
        return NO;
    }
    NSDictionary *plist = [NSPropertyListSerialization propertyListWithData:[listing dataUsingEncoding:NSUTF8StringEncoding]
        options:0 format:nil error:error];
    NSString *mount = nil;
    for (NSDictionary *entity in plist[@"system-entities"]) {
        if ([entity[@"mount-point"] isKindOfClass:NSString.class]) mount = entity[@"mount-point"];
    }
    if (!mount) {
        [NSFileManager.defaultManager removeItemAtURL:dmg error:nil];
        if (error) *error = updateFailure(@"The disk image did not mount.");
        return NO;
    }
    NSURL *source = [NSURL fileURLWithPath:[mount stringByAppendingPathComponent:@"OpenCE.app"] isDirectory:YES];
    BOOL copied = [NSFileManager.defaultManager createDirectoryAtURL:staged withIntermediateDirectories:YES attributes:nil error:error] &&
        [self runTool:@"/usr/bin/ditto" arguments:@[source.path, [staged URLByAppendingPathComponent:@"OpenCE.app"].path]
            output:nil error:error] != nil;
    [self runTool:@"/usr/bin/hdiutil" arguments:@[@"detach", mount] output:nil error:nil];
    [NSFileManager.defaultManager removeItemAtURL:dmg error:nil];
    NSURL *app = [staged URLByAppendingPathComponent:@"OpenCE.app" isDirectory:YES];
    if (!copied) {
        [NSFileManager.defaultManager removeItemAtURL:staged error:nil];
        return NO;
    }
    [self runTool:@"/usr/bin/xattr" arguments:@[@"-dr", @"com.apple.quarantine", app.path] output:nil error:nil];
    if (![self runTool:@"/usr/bin/codesign" arguments:@[@"--verify", @"--deep", @"--strict", app.path] output:nil error:error]) {
        [NSFileManager.defaultManager removeItemAtURL:staged error:nil];
        return NO;
    }
    NSDictionary *info = [NSDictionary dictionaryWithContentsOfURL:[app URLByAppendingPathComponent:@"Contents/Info.plist"]];
    NSNumber *marked = info[@"OpenCEMacUpdateBuild"];
    if (![info[@"CFBundleIdentifier"] isEqual:@"local.halo.ce-universal"] ||
        ![marked isKindOfClass:NSNumber.class] || marked.longValue != build) {
        [NSFileManager.defaultManager removeItemAtURL:staged error:nil];
        if (error) *error = updateFailure(@"The downloaded app is not the Mac release that was announced.");
        return NO;
    }
    return [self handOffStagedApp:app error:error];
}

- (BOOL)handOffStagedApp:(NSURL *)app error:(NSError **)error {
    /* This process is about to quit. The swap has to keep running after that,
       or it would be killed while the installed app was half replaced. */
    NSURL *script = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:
        [[NSUUID UUID].UUIDString stringByAppendingPathExtension:@"sh"]]];
    /* The new app is checked under its real name, then the installed one is
       moved aside. If the new one cannot take its place, the old one comes back. */
    NSString *body = @"#!/bin/sh\nset -eu\n"
        "while kill -0 \"$1\" 2>/dev/null; do sleep 0.2; done\n"
        "sleep 0.5\n"
        "parent=$(dirname \"$3\")\n"
        "holding=\"$parent/.OpenCE-update\"\n"
        "previous=\"$3.previous\"\n"
        "rm -rf \"$holding\" \"$previous\"\n"
        "mkdir -p \"$holding\"\n"
        "ditto \"$2\" \"$holding/OpenCE.app\"\n"
        "xattr -dr com.apple.quarantine \"$holding/OpenCE.app\" || true\n"
        "codesign --verify --deep --strict \"$holding/OpenCE.app\"\n"
        "mv \"$3\" \"$previous\"\n"
        "if ! mv \"$holding/OpenCE.app\" \"$3\"; then mv \"$previous\" \"$3\"; exit 1; fi\n"
        "rm -rf \"$previous\" \"$holding\" \"$(dirname \"$2\")\"\n"
        "open \"$3\"\n"
        "rm -f \"$0\"\n";
    if (![[body dataUsingEncoding:NSUTF8StringEncoding] writeToURL:script options:NSDataWritingAtomic error:error]) return NO;
    chmod(script.path.fileSystemRepresentation, 0700);
    NSURL *log = [_configURL.URLByDeletingLastPathComponent URLByAppendingPathComponent:@"update.log"];
    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attr;
    posix_spawn_file_actions_init(&actions);
    posix_spawnattr_init(&attr);
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSID);
    posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&actions, 1, log.path.fileSystemRepresentation, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    posix_spawn_file_actions_addopen(&actions, 2, log.path.fileSystemRepresentation, O_WRONLY | O_CREAT | O_APPEND, 0644);
    char pidText[32];
    snprintf(pidText, sizeof(pidText), "%d", getpid());
    char *argv[] = { "sh", (char *)script.path.fileSystemRepresentation, pidText,
        (char *)app.path.fileSystemRepresentation, (char *)_bundleURL.path.fileSystemRepresentation, NULL };
    pid_t child = 0;
    int spawned = posix_spawn(&child, "/bin/sh", &actions, &attr, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attr);
    if (spawned != 0) {
        if (error) *error = updateFailure(@"OpenCE could not start the installer.");
        return NO;
    }
    return YES;
}
@end
