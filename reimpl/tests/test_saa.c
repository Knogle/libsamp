#include "sampdll/archive/saa.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition, message)                                                                  \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      fprintf(stderr, "test_saa: %s\n", message);                                                  \
      result = 1;                                                                                  \
      goto cleanup;                                                                                \
    }                                                                                              \
  } while (0)

static int read_file(const char *path, unsigned char **out_data, size_t *out_size) {
  FILE *stream;
  long length;
  unsigned char *data;

  stream = fopen(path, "rb");
  if (stream == NULL || fseek(stream, 0, SEEK_END) != 0) {
    if (stream != NULL) {
      fclose(stream);
    }
    return 0;
  }
  length = ftell(stream);
  if (length <= 0 || fseek(stream, 0, SEEK_SET) != 0) {
    fclose(stream);
    return 0;
  }
  data = (unsigned char *)malloc((size_t)length);
  if (data == NULL || fread(data, 1, (size_t)length, stream) != (size_t)length) {
    free(data);
    fclose(stream);
    return 0;
  }
  fclose(stream);
  *out_data = data;
  *out_size = (size_t)length;
  return 1;
}

int main(int argc, char **argv) {
  unsigned char *archive_data = NULL;
  unsigned char *main_data = NULL;
  unsigned char *script_data = NULL;
  unsigned char *corrupt_data = NULL;
  size_t archive_size = 0;
  samp_saa_layout layout;
  samp_saa_archive archive;
  samp_saa_file main_file;
  samp_saa_file script_file;
  uint32_t hash = 0;
  int result = 0;

  if (argc != 2) {
    fprintf(stderr, "usage: test_saa <samp.saa>\n");
    return 2;
  }

  CHECK(read_file(argv[1], &archive_data, &archive_size), "cannot read archive fixture");
  CHECK(samp_saa_parse_layout(archive_data, archive_size, &layout), "header/layout rejected");
  CHECK(layout.header_size == 256u, "unexpected header size");
  CHECK(layout.signature_size == 128u, "unexpected signature size");
  CHECK(!samp_saa_open_verified(&archive, archive_data, archive_size, 0),
        "archive opened without external signature verification");

  /* The portable core deliberately receives the verification result from the
   * Win32 CryptoAPI adapter. This fixture is the already hashed R5 archive. */
  CHECK(samp_saa_open_verified(&archive, archive_data, archive_size, 1), "entry table decode failed");
  CHECK(samp_saa_hash_filename("main.scm") == 0xD83F24DDu, "main.scm hash mismatch");
  CHECK(samp_saa_hash_filename("SCRIPT.IMG") == 0x11A462D1u, "script.img hash mismatch");
  CHECK(samp_saa_hash_path("data\\script\\MAIN.SCM", &hash) && hash == 0xD83F24DDu,
        "Windows path normalization mismatch");
  CHECK(samp_saa_hash_path("data/script/script.img", &hash) && hash == 0x11A462D1u,
        "slash path normalization mismatch");
  CHECK(samp_saa_hash_path("data/script/main.scm ", &hash) && hash != 0xD83F24DDu,
        "path normalizer changed the original basename bytes");

  CHECK(samp_saa_find(&archive, 0xD83F24DDu, &main_file), "main.scm entry missing");
  CHECK(samp_saa_find(&archive, 0x11A462D1u, &script_file), "script.img entry missing");
  CHECK(main_file.size == 103876u, "main.scm size mismatch");
  CHECK(script_file.size == 194560u, "script.img size mismatch");

  main_data = (unsigned char *)malloc(main_file.size);
  script_data = (unsigned char *)malloc(script_file.size);
  CHECK(main_data != NULL && script_data != NULL, "file buffer allocation failed");
  CHECK(samp_saa_extract(&archive, &main_file, main_data, main_file.size), "main.scm extraction failed");
  CHECK(samp_saa_extract(&archive, &script_file, script_data, script_file.size),
        "script.img extraction failed");
  CHECK(main_data[0] == 0x02u && main_data[1] == 0x00u && main_data[2] == 0x01u,
        "main.scm decoded prefix mismatch");
  CHECK(memcmp(script_data, "VER2", 4u) == 0, "script.img decoded prefix mismatch");

  corrupt_data = (unsigned char *)malloc(archive_size);
  CHECK(corrupt_data != NULL, "corrupt fixture allocation failed");
  memcpy(corrupt_data, archive_data, archive_size);
  corrupt_data[248] ^= 1u;
  CHECK(!samp_saa_parse_layout(corrupt_data, archive_size, &layout), "corrupt header accepted");

cleanup:
  free(corrupt_data);
  free(script_data);
  free(main_data);
  free(archive_data);
  return result;
}
