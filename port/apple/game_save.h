#import "cloud_save.h"
#import <GameSave/GameSave.h>

/* Same validated checkpoint/reconciliation contract as CloudKit, backed by
   GameSave files. CKRecord is only an in-memory revision envelope: no database
   operation or CloudKit account is used by this transport. Main thread only. */
API_AVAILABLE(ios(26.0), macos(26.0))
@interface HaloGameSaveTransport : HaloCloudSaveTransport
- (instancetype)initWithContainer:(NSString *)identifier backups:(NSURL *)backups;
@property(nonatomic, readonly) NSArray<NSString *> *conflictChoices;
- (BOOL)resolveConflictAtIndex:(NSUInteger)index error:(NSError **)error;
- (void)invalidate;
/* Injection seam for tests; production supplies GSSyncedDirectory. */
@property(nonatomic, copy) GSSyncedDirectory *(^openDirectory)(NSString *identifier);
@end
