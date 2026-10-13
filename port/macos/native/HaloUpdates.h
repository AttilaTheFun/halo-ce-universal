#import <Foundation/Foundation.h>

/* The Mac release number in a fork tag such as test-v0.3.0-net28-mac16.
   Zero when the tag is not one of those releases. An iOS tag is zero. */
long HaloForkUpdateBuildFromTag(NSString *tag);
NSString *HaloForkUpdateAssetName(NSString *tag);
/* The download URL GitHub gives that tag's disk image, and no other host. */
BOOL HaloForkUpdateDownloadIsTrusted(NSString *tag, NSString *url);
/* The newest Mac disk image in a GitHub releases list. A newer iOS release
   can be the newest tag; it is not this app. Drafts and prereleases are
   skipped. Returns 0 when the list has no Mac disk image. */
long HaloForkNewestMacRelease(NSArray *releases, NSString **tag, NSString **url);

/* update.auto in config.toml. Missing means yes, which is the game's default. */
BOOL HaloConfigAutomaticUpdates(NSString *text);
NSString *HaloConfigSetAutomaticUpdates(NSString *text, BOOL automatic);
/* A hidden, timed, or scripted run is not asked. debug.update_answer is not
   one of those: the caller uses that answer instead of a dialog. */
BOOL HaloConfigSuppressesUpdatePrompt(NSString *text);
NSString *HaloConfigUpdateAnswer(NSString *text);

@interface HaloForkUpdater : NSObject
@property(nonatomic, readonly) long build;
- (instancetype)initWithBuild:(long)build configURL:(NSURL *)configURL bundleURL:(NSURL *)bundleURL;
- (BOOL)automatic;
- (BOOL)setAutomatic:(BOOL)automatic error:(NSError **)error;
- (BOOL)suppressesPrompt;
- (NSString *)updateAnswer;
/* completion is on the main queue. download is nil when nothing newer was found. */
- (void)checkQuiet:(BOOL)quiet completion:(void (^)(NSError *error, NSString *tag, NSURL *download, long build))completion;
/* Downloads that disk image, checks it, and leaves a helper that replaces this
   app after this process exits. The caller then quits. */
- (BOOL)installDownload:(NSURL *)download expectedBuild:(long)build error:(NSError **)error
               progress:(void (^)(unsigned long long received, unsigned long long total))progress;
@end
