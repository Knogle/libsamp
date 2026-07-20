#ifndef SAMPDLL_ARCHIVE_SAA_H
#define SAMPDLL_ARCHIVE_SAA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SAMP_SAA_ENTRY_COUNT 256u
#define SAMP_SAA_ENTRY_BYTES 8u
#define SAMP_SAA_TABLE_BYTES (SAMP_SAA_ENTRY_COUNT * SAMP_SAA_ENTRY_BYTES)
#define SAMP_SAA_BLOCK_BYTES 2048u

typedef struct samp_saa_layout {
  size_t header_size;
  size_t entry_offset;
  size_t signature_offset;
  uint32_t signature_size;
  uint8_t invalid_index;
} samp_saa_layout;

typedef struct samp_saa_archive {
  const uint8_t *data;
  size_t data_size;
  samp_saa_layout layout;
  uint8_t entries[SAMP_SAA_TABLE_BYTES];
  int ready;
} samp_saa_archive;

typedef struct samp_saa_file {
  uint32_t hash;
  uint32_t size;
  size_t data_offset;
  uint16_t entry_index;
} samp_saa_file;

/*
 * STATIC_037:
 * Original SA-MP 0.3.7 R5 samp.dll SHA256=
 * b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2.
 * Archive constructor samp.dll+0x65350 fixes 256 entries and 120 fake-header
 * words. LoadEntries at samp.dll+0x64AD0 verifies a 128-byte RSA/SHA-1
 * signature before enabling entry decoding.
 */
int samp_saa_parse_layout(const uint8_t *data, size_t data_size, samp_saa_layout *out_layout);

/* signature_verified must only be true after verification with the R5 public key. */
int samp_saa_open_verified(samp_saa_archive *archive, const uint8_t *data, size_t data_size,
                           int signature_verified);

uint32_t samp_saa_hash_filename(const char *filename);
int samp_saa_hash_path(const char *path, uint32_t *out_hash);
int samp_saa_find(const samp_saa_archive *archive, uint32_t hash, samp_saa_file *out_file);
int samp_saa_extract(const samp_saa_archive *archive, const samp_saa_file *file, uint8_t *output,
                     size_t output_size);

#ifdef __cplusplus
}
#endif

#endif
