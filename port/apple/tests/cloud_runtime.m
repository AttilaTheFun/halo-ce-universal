// Exercises the real runtime's opt-in and stale-auth boundaries without iCloud.
#import <objc/runtime.h>
#import "../cloud_runtime.m"
static int preparations, requests, identities;
static void (^accountReply)(CKAccountStatus, NSError *);
int halo_save_checkpoint_prepare(void) { preparations++; return 1; }
@interface TestCloudContainer : NSObject
@end
@implementation TestCloudContainer
- (void)accountStatusWithCompletionHandler:(void (^)(CKAccountStatus, NSError *))reply {
    requests++; accountReply = [reply copy];
}
- (void)fetchUserRecordIDWithCompletionHandler:(void (^)(CKRecordID *, NSError *))reply {
    (void)reply; identities++;
}
@end
static void drain(void) {
    [NSRunLoop.mainRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.03]];
}
int main(void) { @autoreleasepool {
    NSString *suite = [@"HaloCloudRuntimeTest." stringByAppendingString:NSUUID.UUID.UUIDString];
    NSUserDefaults *defaults = [[NSUserDefaults alloc] initWithSuiteName:suite];
    Method defaultsMethod = class_getClassMethod(NSUserDefaults.class, @selector(standardUserDefaults));
    IMP oldDefaults = method_setImplementation(defaultsMethod, imp_implementationWithBlock(^id(id cls) { (void)cls; return defaults; }));
    TestCloudContainer *fake = [TestCloudContainer new];
    Method factory = class_getClassMethod(CKContainer.class, @selector(containerWithIdentifier:));
    IMP oldFactory = method_setImplementation(factory, imp_implementationWithBlock(^id(id cls, NSString *identifier) { (void)cls; (void)identifier; return fake; }));
    containerIdentifier = @"iCloud.test.Halo";
    NSCAssert(!syncEnabled(), @"Fresh install must opt out");
    startSync(); authenticate(); pulse();
    NSCAssert(!requests && !preparations && !container && !timer, @"Disabled runtime must not contact cloud or wait for it");
    setSyncEnabled(YES);
    NSCAssert(requests == 1 && preparations == 1 && timer && accountObserver, @"Explicit enable starts sync");
    NSUserDefaults *readback = [[NSUserDefaults alloc] initWithSuiteName:suite];
    NSCAssert([readback boolForKey:syncPreference], @"Opt-in must persist");
    void (^staleReply)(CKAccountStatus, NSError *) = accountReply;
    setSyncEnabled(NO);
    NSCAssert(!container && !timer && !accountObserver && !session, @"Disable tears down sync");
    staleReply(CKAccountStatusAvailable, nil); drain();
    [NSNotificationCenter.defaultCenter postNotificationName:CKAccountChangedNotification object:nil];
    halo_cloud_checkpoint_ready(); drain();
    NSCAssert(!identities && requests == 1 && [runtimeStatus isEqual:@"disabled"], @"Disabled callbacks must not restart authentication");
    NSCAssert(![readback boolForKey:syncPreference], @"Opt-out must persist");
    guestRunning = YES;
    setSyncEnabled(YES);
    NSCAssert(requests == 2 && preparations == 1, @"Enabling during gameplay must not scan live save handles");
    staleReply(CKAccountStatusAvailable, nil); drain();
    NSCAssert(!identities, @"Prior generation must not authenticate after re-enable");
    accountReply(CKAccountStatusAvailable, nil); drain();
    NSCAssert(identities == 1, @"Current enabled generation may authenticate");
    setSyncEnabled(NO);
    [defaults removePersistentDomainForName:suite];
    method_setImplementation(factory, oldFactory);
    method_setImplementation(defaultsMethod, oldDefaults);
    puts("Cloud runtime opt-in tests passed");
} return 0; }
