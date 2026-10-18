#import <Foundation/Foundation.h>
#import <CloudKit/CloudKit.h>

/* Injectable database boundary; tests never contact an Apple account. */
@protocol HaloCloudDatabase <NSObject>
- (void)fetchRecordWithID:(CKRecordID *)recordID completionHandler:(void (^)(CKRecord *, NSError *))completion;
- (void)addOperation:(CKDatabaseOperation *)operation;
@end

/* Uses only the configured container's PRIVATE database. Callbacks may arrive
   on a CloudKit queue. No method installs saves into the running game.
   A nil expectedHead means "head observed absent", never "overwrite latest".
   The coordinator must bind cached heads/pending saves to the iCloud user ID. */
@interface HaloCloudSaveTransport : NSObject
- (BOOL)uploadsRequireConfirmation;
- (instancetype)initWithDatabase:(id<HaloCloudDatabase>)database stagingDirectory:(NSURL *)directory;
- (void)fetchCompatibility:(NSString *)compatibility
               completion:(void (^)(CKRecord *head, NSData *snapshot, NSError *error))completion;
- (void)uploadSnapshot:(NSData *)snapshot compatibility:(NSString *)compatibility
         expectedHead:(CKRecord *)expected
           completion:(void (^)(CKRecord *savedHead, NSError *error))completion;
+ (CKRecordID *)headIDForCompatibility:(NSString *)compatibility;
@end
