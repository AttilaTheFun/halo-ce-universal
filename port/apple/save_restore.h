#import <Foundation/Foundation.h>
/* The guest must be stopped for both calls. Recover before starting it.
   An interrupted, uncommitted restore rolls back; a committed restore finishes
   cleanup. Failure leaves the transaction and original data for retry. */
BOOL halo_save_restore_recover(NSURL *saveRoot, NSError **error);
BOOL halo_save_restore(NSData *snapshot, NSString *compatibility, NSURL *saveRoot, NSError **error);

/* Caller obtains access to the selected folder with the system picker and
   ensures the old game is stopped. Queue copies only persistent saves; it
   never mutates the source or the current game. Apply only before guest start. */
BOOL halo_save_import_queue(NSURL *selectedRoot, NSString *compatibility, NSURL *saveRoot, NSError **error);
BOOL halo_save_import_apply(NSString *compatibility, NSURL *saveRoot, NSError **error);
