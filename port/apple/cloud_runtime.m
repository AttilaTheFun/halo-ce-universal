#import "cloud_runtime.h"
#import "cloud_sync.h"
#import "save_snapshot.h"
#import "save_runtime.h"
#import <TargetConditionals.h>
#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#else
#import <AppKit/AppKit.h>
#endif

static HaloCloudSync *session;
static CKContainer *container;
static NSURL *saveRoot;
static NSString *compatibility;
static NSString *runtimeStatus = @"notConfigured";
static BOOL guestRunning, presenting;
static NSUInteger generation;
static NSTimer *timer;
#if TARGET_OS_IPHONE
static UIWindow *cloudWindow;
static __weak UIWindow *priorWindow;
@interface HaloCloudViewController : UIViewController
@end
@implementation HaloCloudViewController
- (UIInterfaceOrientationMask)supportedInterfaceOrientations { return UIInterfaceOrientationMaskLandscape; }
@end
#endif
static NSString *statusText(void) {
    NSDictionary *text = @{
        @"notConfigured":@"iCloud saves are not enabled in this build. Your progress is saved on this device.",
        @"notAuthenticated":@"Sign in to iCloud in system settings to sync progress. Your local save is safe.",
        @"checking":@"Checking iCloud for saved progress…",
        @"ready":@"iCloud is ready. Progress will upload after the game saves a campaign checkpoint.",
        @"synced":@"This device and iCloud have the same saved progress.",
        @"pending":@"A newer checkpoint is waiting to upload.",
        @"uploading":@"Uploading saved progress…",
        @"offline":@"iCloud is unavailable. Your progress remains on this device and sync will retry.",
        @"syncError":@"The save could not be uploaded. Sync again to check for conflicting progress.",
        @"conflict":@"Both this device and iCloud have different progress. Choose which save to continue. Either choice keeps a local backup of the progress it replaces.",
        @"accountChanged":@"The iCloud account has changed. Allow this account to sync the progress on this device? This may copy your local save into the new account.",
        @"accountUnavailable":@"Your iCloud account changed or became unavailable. Your local progress is safe.",
        @"cloudReadyForRelaunch":@"Newer progress is available in iCloud. Quit and reopen Halo to load it; it will not replace an active game.",
        @"gameChanged":@"These saves belong to a different game image. Import the same Halo ISO on each device before syncing.",
        @"invalidLocalState":@"The local sync record could not be read. Your saves have not been replaced.",
        @"invalidLocalSave":@"This device’s saved progress could not be prepared for sync. Your original saves have not been removed.",
        @"localError":@"Halo could not update its local sync files. Check available storage and retry.",
        @"restoreError":@"Saved progress could not be restored. Quit and reopen Halo to recover the previous save before continuing."
    };
    NSString *status = session ? session.status : runtimeStatus;
    return text[status] ?: @"Saved progress stays on this device until iCloud sync completes.";
}
static void pulse(void) {
    if (session && !presenting) [session synchronize];
}
static void authenticate(void) {
    NSUInteger token = ++generation;
    [session invalidate]; session = nil;
    runtimeStatus = @"checking";
    [container accountStatusWithCompletionHandler:^(CKAccountStatus status, NSError *error) {
        if (status != CKAccountStatusAvailable || error) {
            dispatch_async(dispatch_get_main_queue(), ^{
                if (token == generation) runtimeStatus = error ? @"offline" : @"notAuthenticated";
            });
            return;
        }
        [container fetchUserRecordIDWithCompletionHandler:^(CKRecordID *identity, NSError *failure) {
            dispatch_async(dispatch_get_main_queue(), ^{
                if (token != generation) return;
                if (failure || !identity.recordName.length) { runtimeStatus = @"notAuthenticated"; return; }
                HaloCloudSaveTransport *transport = [[HaloCloudSaveTransport alloc]
                    initWithDatabase:(id<HaloCloudDatabase>)container.privateCloudDatabase
                    stagingDirectory:[saveRoot URLByAppendingPathComponent:@"Cloud Saves/Uploads"]];
                session = [[HaloCloudSync alloc] initWithRoot:saveRoot compatibility:compatibility
                    account:identity.recordName transport:transport];
                session.guestStopped = !guestRunning;
                session.stateChanged = ^{ [NSNotificationCenter.defaultCenter postNotificationName:@"HaloCloudSaveChanged" object:nil]; };
                [session synchronize];
            });
        }];
    }];
}
void halo_cloud_prepare(const char *saves, const char *data) {
    NSCAssert(NSThread.isMainThread, @"Cloud startup must run on the main thread");
    NSString *identifier = NSBundle.mainBundle.infoDictionary[@"HaloCloudContainer"];
    if (![identifier isKindOfClass:NSString.class] || ![identifier hasPrefix:@"iCloud."]) return;
    NSDictionary *manifest = [NSDictionary dictionaryWithContentsOfFile:
        [[NSString stringWithUTF8String:data] stringByAppendingPathComponent:@"integrity.plist"]];
    NSString *hash = manifest[@"sha256"];
    if (![hash isKindOfClass:NSString.class] || hash.length != 64 ||
        [hash rangeOfCharacterFromSet:[NSCharacterSet characterSetWithCharactersInString:@"0123456789abcdef"].invertedSet].location != NSNotFound) {
        runtimeStatus = @"gameChanged"; return;
    }
    compatibility = [@"halo-native-save-v1:" stringByAppendingString:hash];
    saveRoot = [NSURL fileURLWithPath:[NSString stringWithUTF8String:saves] isDirectory:YES];
    // Existing offline progress must participate in reconciliation, rather than
    // being mistaken for an empty device and overwritten by a cloud download.
    NSURL *candidate = [saveRoot URLByAppendingPathComponent:@"Cloud Saves/local-checkpoint.plist"];
    if (![NSFileManager.defaultManager fileExistsAtPath:candidate.path]) {
        NSError *error = nil;
        NSData *snapshot = halo_save_snapshot_create(saveRoot, compatibility, &error);
        if (!snapshot) { runtimeStatus = @"invalidLocalSave"; return; }
        if ([halo_save_snapshot_decode(snapshot, compatibility, nil)[@"files"] count]) {
            halo_save_checkpoint_committed();
            if (![NSFileManager.defaultManager fileExistsAtPath:candidate.path]) {
                runtimeStatus = @"invalidLocalSave"; return;
            }
        }
    }
    container = [CKContainer containerWithIdentifier:identifier];
    [NSNotificationCenter.defaultCenter addObserverForName:CKAccountChangedNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification *note) {
        (void)note; authenticate();
    }];
    authenticate();
    // Bound startup waiting; never let connectivity prevent offline play.
    NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:12];
    while ((([runtimeStatus isEqual:@"checking"] && !session) || session.busy) && deadline.timeIntervalSinceNow > 0)
        [NSRunLoop.mainRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.025]];
    if ([@[@"conflict", @"accountChanged"] containsObject:session.status]) {
        halo_cloud_show_status();
#if TARGET_OS_IPHONE
        while (presenting) [NSRunLoop.mainRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.025]];
#endif
    }
    timer = [NSTimer timerWithTimeInterval:30 repeats:YES block:^(NSTimer *tick) {
        (void)tick;
        if (!session && container && ![runtimeStatus isEqual:@"checking"]) authenticate();
        else pulse();
    }];
    [NSRunLoop.mainRunLoop addTimer:timer forMode:NSRunLoopCommonModes];
}
void halo_cloud_guest_started(void) { guestRunning = YES; session.guestStopped = NO; }
void halo_cloud_checkpoint_ready(void) {
    dispatch_async(dispatch_get_main_queue(), ^{ pulse(); });
}
void halo_cloud_show_status(void) {
    if (presenting) return;
    presenting = YES;
    NSUInteger shownGeneration = generation;
    NSString *status = session ? session.status : runtimeStatus;
    NSMutableArray<NSString *> *titles = [NSMutableArray new];
    NSMutableArray *actions = [NSMutableArray new];
    if ([status isEqual:@"conflict"]) {
        [titles addObject:@"Keep This Device"]; [actions addObject:^{ [session keepLocal]; }];
        [titles addObject:guestRunning ? @"Use iCloud After Relaunch" : @"Use iCloud"]; [actions addObject:^{ [session useCloud]; }];
    } else if ([status isEqual:@"accountChanged"]) {
        [titles addObject:@"Use Current iCloud Account"]; [actions addObject:^{ [session allowAccountChange]; }];
    } else if (container) {
        [titles addObject:@"Sync Now"]; [actions addObject:^{ if (session) [session synchronize]; else authenticate(); }];
    }
    [titles addObject:@"Close"]; [actions addObject:^{}];
#if TARGET_OS_IPHONE
    UIWindowScene *scene = nil;
    for (UIScene *candidate in UIApplication.sharedApplication.connectedScenes) {
        if (![candidate isKindOfClass:UIWindowScene.class]) continue;
        if (!scene || candidate.activationState == UISceneActivationStateForegroundActive) scene = (UIWindowScene *)candidate;
        if (candidate.activationState == UISceneActivationStateForegroundActive) break;
    }
    if (!scene) { presenting = NO; return; }
    for (UIWindow *window in scene.windows) if (window.isKeyWindow) priorWindow = window;
    cloudWindow = [[UIWindow alloc] initWithWindowScene:scene];
    cloudWindow.frame = scene.coordinateSpace.bounds;
    cloudWindow.rootViewController = [HaloCloudViewController new];
    cloudWindow.windowLevel = UIWindowLevelAlert + 1;
    [cloudWindow makeKeyAndVisible];
    UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"iCloud Saves" message:statusText() preferredStyle:UIAlertControllerStyleAlert];
    for (NSUInteger i = 0; i < titles.count; i++) {
        void (^action)(void) = actions[i];
        [alert addAction:[UIAlertAction actionWithTitle:titles[i] style:UIAlertActionStyleDefault handler:^(UIAlertAction *button) {
            (void)button; if (shownGeneration == generation) action();
            [cloudWindow.rootViewController dismissViewControllerAnimated:NO completion:^{
                cloudWindow.hidden = YES; cloudWindow = nil; [priorWindow makeKeyWindow]; presenting = NO;
            }];
        }]];
    }
    [cloudWindow.rootViewController presentViewController:alert animated:NO completion:nil];
#else
    NSAlert *alert = [NSAlert new]; alert.messageText = @"iCloud Saves"; alert.informativeText = statusText();
    for (NSString *title in titles) [alert addButtonWithTitle:title];
    NSInteger selected = [alert runModal] - NSAlertFirstButtonReturn;
    if (shownGeneration == generation && selected >= 0 && (NSUInteger)selected < actions.count) { void (^action)(void) = actions[selected]; action(); }
    presenting = NO;
#endif
}
