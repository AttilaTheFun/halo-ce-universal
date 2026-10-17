#import <Foundation/Foundation.h>
#import "cloud_save.h"

/* Main-thread coordinator. The caller supplies the authenticated CloudKit user
   ID (never an email), private-database transport and verified game identity.
   Account changes invalidate the old coordinator before constructing a new one.
   Cloud restores require guestStopped to remain YES until completion. */
@interface HaloCloudSync : NSObject
@property(nonatomic, readonly) NSString *status;
@property(nonatomic, readonly) NSError *lastError;
@property(nonatomic, readonly) BOOL busy;
@property(nonatomic) BOOL guestStopped;
@property(nonatomic, copy) void (^stateChanged)(void);
- (instancetype)initWithRoot:(NSURL *)saveRoot compatibility:(NSString *)compatibility
                    account:(NSString *)account transport:(HaloCloudSaveTransport *)transport;
- (void)synchronize;
- (void)keepLocal;
- (void)useCloud;
- (void)allowAccountChange;
- (void)invalidate;
@end
