#ifndef HALO_APPLE_SAVE_RUNTIME_H
#define HALO_APPLE_SAVE_RUNTIME_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
void halo_save_checkpoint_configure(const char *save_root, const char *data_root);
void halo_save_checkpoint_committed(void);
/* Run on startup before any guest save handles are opened. */
int halo_save_import_before_launch(char *message, size_t capacity);
int halo_save_recover_before_launch(const char *save_root, char *message, size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
