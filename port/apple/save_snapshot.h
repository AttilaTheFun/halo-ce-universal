#import <Foundation/Foundation.h>

/* Call only while guest save I/O is quiescent. A snapshot contains persistent
   Xbox profiles/checkpoints, never the working z/savegame.bin or map caches.
   The caller supplies the game-data compatibility ID and retains conflicts. */
NSData *halo_save_snapshot_create(NSURL *saveRoot, NSString *compatibility, NSError **error);
NSDictionary *halo_save_snapshot_decode(NSData *data, NSString *compatibility, NSError **error);
/* Writes a new, private staging directory. Does not replace any live save.
   The caller must stop the guest and transactionally adopt the returned tree. */
NSURL *halo_save_snapshot_stage(NSData *data, NSString *compatibility, NSURL *parent, NSError **error);
