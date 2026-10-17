#import <Foundation/Foundation.h>
/* The guest must be stopped for both calls. Recover before starting it.
   An interrupted, uncommitted restore rolls back; a committed restore finishes
   cleanup. Failure leaves the transaction and original data for retry. */
BOOL halo_save_restore_recover(NSURL *saveRoot, NSError **error);
BOOL halo_save_restore(NSData *snapshot, NSString *compatibility, NSURL *saveRoot, NSError **error);
