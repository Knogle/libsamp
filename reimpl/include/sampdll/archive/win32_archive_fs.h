#ifndef SAMPDLL_ARCHIVE_WIN32_ARCHIVE_FS_H
#define SAMPDLL_ARCHIVE_WIN32_ARCHIVE_FS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*samp_archive_fs_trace_fn)(const char *line);

typedef struct samp_archive_fs_status {
  uint32_t archive_size;
  uint32_t signature_size;
  uint32_t main_size;
  uint32_t script_size;
  int signature_verified;
  int hooks_installed;
} samp_archive_fs_status;

/* Installs hooks only for the validated GTA SA 1.0 US executable profile. */
int samp_archive_fs_win32_start(const char *archive_path, samp_archive_fs_trace_fn trace,
                                samp_archive_fs_status *out_status);
void samp_archive_fs_win32_stop(void);

#ifdef __cplusplus
}
#endif

#endif
