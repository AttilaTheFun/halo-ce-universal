#ifndef HALO_CLOUD_RUNTIME_H
#define HALO_CLOUD_RUNTIME_H
/* All lifecycle calls run on the native main thread. */
void halo_cloud_prepare(const char *save_root, const char *game_root);
void halo_cloud_guest_started(void);
void halo_cloud_checkpoint_ready(void);
void halo_cloud_show_status(void);
#endif
