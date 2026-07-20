#include "sampdll/archive/saa.h"

#include <string.h>

#define SAMP_SAA_ID 0x00083433u
#define SAMP_SAA_VERSION 2u
#define SAMP_SAA_FAKE_WORDS 120u
#define SAMP_SAA_HEADER_BYTES (8u + SAMP_SAA_FAKE_WORDS * 2u + 8u)
#define SAMP_SAA_OBFUSCATE_KEY 0xC103D3E7u
#define SAMP_SAA_TEA_ROUNDS 32u
#define SAMP_SAA_TEA_DELTA 0x9E3779B9u
#define SAMP_SAA_TEA_KEY_BYTES 16u
#define SAMP_SAA_HASH_INIT 0x9E3779B9u
#define SAMP_SAA_HASH_SEED 0x12345678u

/* STATIC_037: samp.dll+0x64C79 loads 16 XOR-obfuscated bytes from
 * samp.dll+0xE9A2C and passes XOR key 0xAA to the XTEA key loader. */
static const uint8_t kSampSaaTableKeyObfuscated[SAMP_SAA_TEA_KEY_BYTES] = {
    0xB9u, 0xEAu, 0x40u, 0x0Au, 0xA3u, 0x1Fu, 0x01u, 0x23u,
    0xB0u, 0xEAu, 0x46u, 0x64u, 0x78u, 0xAFu, 0x50u, 0x80u,
};

static uint32_t load_le32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8u) | ((uint32_t)p[2] << 16u) |
         ((uint32_t)p[3] << 24u);
}

static void store_le32(uint8_t *p, uint32_t value) {
  p[0] = (uint8_t)value;
  p[1] = (uint8_t)(value >> 8u);
  p[2] = (uint8_t)(value >> 16u);
  p[3] = (uint8_t)(value >> 24u);
}

static size_t round_block_size(uint32_t size) {
  return ((size_t)size + (SAMP_SAA_BLOCK_BYTES - 1u)) & ~(size_t)(SAMP_SAA_BLOCK_BYTES - 1u);
}

static uint32_t unobfuscate_u32(uint32_t value) {
  value ^= SAMP_SAA_OBFUSCATE_KEY;
  return (value >> 19u) | (value << 13u);
}

static void tea_load_key(uint32_t key[4], const uint8_t bytes[16], uint8_t xor_key) {
  uint8_t decoded[16];
  size_t i;

  for (i = 0; i < sizeof(decoded); ++i) {
    decoded[i] = bytes[i] ^ xor_key;
  }
  for (i = 0; i < 4u; ++i) {
    key[i] = load_le32(decoded + i * 4u);
  }
}

static void tea_decrypt_with_key(uint8_t *data, size_t size, uint32_t key[4]) {
  size_t offset;

  for (offset = 0; offset + 8u <= size; offset += 8u) {
    uint32_t v0 = load_le32(data + offset);
    uint32_t v1 = load_le32(data + offset + 4u);
    uint32_t old_v0 = v0;
    uint32_t old_v1 = v1;
    uint32_t sum = SAMP_SAA_TEA_DELTA * SAMP_SAA_TEA_ROUNDS;
    uint32_t round;

    for (round = 0; round < SAMP_SAA_TEA_ROUNDS; ++round) {
      v1 -= (((v0 << 4u) ^ (v0 >> 5u)) + v0) ^ (sum + key[(sum >> 11u) & 3u]);
      sum -= SAMP_SAA_TEA_DELTA;
      v0 -= (((v1 << 4u) ^ (v1 >> 5u)) + v1) ^ (sum + key[sum & 3u]);
    }

    store_le32(data + offset, v0);
    store_le32(data + offset + 4u, v1);
    key[0] ^= old_v0;
    key[1] ^= old_v1;
    key[2] ^= old_v0;
    key[3] ^= old_v1;
  }
}

static void tea_decrypt(uint8_t *data, size_t size, const uint8_t key_bytes[16], uint8_t xor_key) {
  uint32_t key[4];

  tea_load_key(key, key_bytes, xor_key);
  tea_decrypt_with_key(data, size, key);
}

static void hash_mix(uint32_t *a, uint32_t *b, uint32_t *c) {
  *a -= *b;
  *a -= *c;
  *a ^= *c >> 13u;
  *b -= *c;
  *b -= *a;
  *b ^= *a << 8u;
  *c -= *a;
  *c -= *b;
  *c ^= *b >> 13u;
  *a -= *b;
  *a -= *c;
  *a ^= *c >> 12u;
  *b -= *c;
  *b -= *a;
  *b ^= *a << 16u;
  *c -= *a;
  *c -= *b;
  *c ^= *b >> 5u;
  *a -= *b;
  *a -= *c;
  *a ^= *c >> 3u;
  *b -= *c;
  *b -= *a;
  *b ^= *a << 10u;
  *c -= *a;
  *c -= *b;
  *c ^= *b >> 15u;
}

static uint32_t hash_bytes(const uint8_t *bytes, size_t length) {
  uint32_t a = SAMP_SAA_HASH_INIT;
  uint32_t b = SAMP_SAA_HASH_INIT;
  uint32_t c = SAMP_SAA_HASH_SEED;
  size_t remaining = length;

  while (remaining >= 12u) {
    a += load_le32(bytes);
    b += load_le32(bytes + 4u);
    c += load_le32(bytes + 8u);
    hash_mix(&a, &b, &c);
    bytes += 12u;
    remaining -= 12u;
  }

  c += (uint32_t)length;
  switch (remaining) {
    case 11:
      c += (uint32_t)bytes[10] << 24u;
      /* fall through */
    case 10:
      c += (uint32_t)bytes[9] << 16u;
      /* fall through */
    case 9:
      c += (uint32_t)bytes[8] << 8u;
      /* fall through */
    case 8:
      b += (uint32_t)bytes[7] << 24u;
      /* fall through */
    case 7:
      b += (uint32_t)bytes[6] << 16u;
      /* fall through */
    case 6:
      b += (uint32_t)bytes[5] << 8u;
      /* fall through */
    case 5:
      b += bytes[4];
      /* fall through */
    case 4:
      a += (uint32_t)bytes[3] << 24u;
      /* fall through */
    case 3:
      a += (uint32_t)bytes[2] << 16u;
      /* fall through */
    case 2:
      a += (uint32_t)bytes[1] << 8u;
      /* fall through */
    case 1:
      a += bytes[0];
      break;
    default:
      break;
  }
  hash_mix(&a, &b, &c);
  return c;
}

int samp_saa_parse_layout(const uint8_t *data, size_t data_size, samp_saa_layout *out_layout) {
  uint32_t encoded_id;
  uint32_t xor_key;
  uint32_t decoded_id;
  uint32_t signature_size;
  size_t signature_offset;
  size_t entry_offset;

  if (data == NULL || out_layout == NULL || data_size < SAMP_SAA_HEADER_BYTES + SAMP_SAA_TABLE_BYTES + 8u) {
    return 0;
  }

  encoded_id = load_le32(data + SAMP_SAA_HEADER_BYTES - 8u);
  xor_key = load_le32(data + SAMP_SAA_HEADER_BYTES - 4u);
  decoded_id = encoded_id ^ xor_key;
  if ((decoded_id & 0x000FFFFFu) != SAMP_SAA_ID || ((decoded_id >> 20u) & 7u) != SAMP_SAA_VERSION) {
    return 0;
  }

  signature_size = (decoded_id >> 23u) & 0xFFu;
  if (signature_size == 0u || signature_size > data_size - SAMP_SAA_HEADER_BYTES - SAMP_SAA_TABLE_BYTES) {
    return 0;
  }
  signature_offset = data_size - signature_size;
  entry_offset = signature_offset - SAMP_SAA_TABLE_BYTES;
  if (entry_offset < SAMP_SAA_HEADER_BYTES ||
      ((entry_offset - SAMP_SAA_HEADER_BYTES) % SAMP_SAA_BLOCK_BYTES) != 0u) {
    return 0;
  }

  out_layout->header_size = SAMP_SAA_HEADER_BYTES;
  out_layout->entry_offset = entry_offset;
  out_layout->signature_offset = signature_offset;
  out_layout->signature_size = signature_size;
  out_layout->invalid_index = (uint8_t)((xor_key >> 5u) & 0xFFu);
  return 1;
}

int samp_saa_open_verified(samp_saa_archive *archive, const uint8_t *data, size_t data_size,
                           int signature_verified) {
  samp_saa_layout layout;

  if (archive == NULL) {
    return 0;
  }
  memset(archive, 0, sizeof(*archive));
  if (!signature_verified || !samp_saa_parse_layout(data, data_size, &layout)) {
    return 0;
  }

  archive->data = data;
  archive->data_size = data_size;
  archive->layout = layout;
  memcpy(archive->entries, data + layout.entry_offset, sizeof(archive->entries));
  tea_decrypt(archive->entries, sizeof(archive->entries), kSampSaaTableKeyObfuscated, 0xAAu);
  archive->ready = 1;
  return 1;
}

uint32_t samp_saa_hash_filename(const char *filename) {
  uint8_t lowered[260];
  size_t length = 0;

  if (filename == NULL) {
    return 0u;
  }
  while (filename[length] != '\0' && length < sizeof(lowered)) {
    uint8_t ch = (uint8_t)filename[length];
    lowered[length] = (ch >= (uint8_t)'A' && ch <= (uint8_t)'Z') ? (uint8_t)(ch + ('a' - 'A')) : ch;
    ++length;
  }
  if (filename[length] != '\0' || length == 0u) {
    return 0u;
  }
  return hash_bytes(lowered, length);
}

int samp_saa_hash_path(const char *path, uint32_t *out_hash) {
  const char *end;
  const char *base;
  char filename[260];
  size_t length;

  if (path == NULL || out_hash == NULL || path[0] == '\0') {
    return 0;
  }
  end = path + strlen(path);
  base = end;
  while (base > path && base[-1] != '\\' && base[-1] != '/') {
    --base;
  }
  length = (size_t)(end - base);
  if (length == 0u || length >= sizeof(filename)) {
    return 0;
  }
  memcpy(filename, base, length);
  filename[length] = '\0';
  *out_hash = samp_saa_hash_filename(filename);
  return *out_hash != 0u;
}

static int decode_entry(const samp_saa_archive *archive, uint16_t index, uint32_t *hash, uint8_t *previous,
                        uint32_t *size) {
  const uint8_t *entry;
  uint32_t decoded;

  if (archive == NULL || !archive->ready || index >= SAMP_SAA_ENTRY_COUNT) {
    return 0;
  }
  entry = archive->entries + (size_t)index * SAMP_SAA_ENTRY_BYTES;
  *hash = load_le32(entry);
  decoded = unobfuscate_u32(load_le32(entry + 4u)) ^ *hash;
  *previous = (uint8_t)decoded;
  *size = decoded >> 8u;
  return 1;
}

int samp_saa_find(const samp_saa_archive *archive, uint32_t hash, samp_saa_file *out_file) {
  uint16_t index;
  uint8_t previous = 0;
  uint32_t size = 0;
  size_t data_offset;
  uint8_t visited[SAMP_SAA_ENTRY_COUNT];

  if (archive == NULL || out_file == NULL || !archive->ready) {
    return 0;
  }
  for (index = 0; index < SAMP_SAA_ENTRY_COUNT; ++index) {
    uint32_t candidate_hash;
    if (!decode_entry(archive, index, &candidate_hash, &previous, &size)) {
      return 0;
    }
    if (candidate_hash == hash) {
      break;
    }
  }
  if (index == SAMP_SAA_ENTRY_COUNT || previous == (uint8_t)index || size == 0u) {
    return 0;
  }

  memset(visited, 0, sizeof(visited));
  data_offset = archive->layout.header_size;
  while (previous != archive->layout.invalid_index) {
    uint32_t previous_hash;
    uint32_t previous_size;
    uint8_t next_previous;
    size_t rounded;

    if (visited[previous]) {
      return 0;
    }
    visited[previous] = 1u;
    if (!decode_entry(archive, previous, &previous_hash, &next_previous, &previous_size) ||
        next_previous == previous || previous_size == 0u) {
      return 0;
    }
    rounded = round_block_size(previous_size);
    if (rounded > archive->layout.entry_offset - data_offset) {
      return 0;
    }
    data_offset += rounded;
    previous = next_previous;
  }

  if (round_block_size(size) > archive->layout.entry_offset - data_offset) {
    return 0;
  }
  out_file->hash = hash;
  out_file->size = size;
  out_file->data_offset = data_offset;
  out_file->entry_index = index;
  return 1;
}

int samp_saa_extract(const samp_saa_archive *archive, const samp_saa_file *file, uint8_t *output,
                     size_t output_size) {
  uint8_t key_bytes[SAMP_SAA_TEA_KEY_BYTES];
  uint8_t block[SAMP_SAA_BLOCK_BYTES];
  uint32_t key[4];
  uint32_t key_offset;
  size_t written = 0;
  size_t source_offset;

  if (archive == NULL || file == NULL || output == NULL || !archive->ready || output_size < file->size ||
      file->data_offset > archive->layout.entry_offset ||
      round_block_size(file->size) > archive->layout.entry_offset - file->data_offset) {
    return 0;
  }

  key_offset = file->hash % (SAMP_SAA_TABLE_BYTES - SAMP_SAA_TEA_KEY_BYTES);
  memcpy(key_bytes, archive->entries + key_offset, sizeof(key_bytes));
  tea_load_key(key, key_bytes, 0u);
  source_offset = file->data_offset;
  while (written < file->size) {
    size_t copy_size = file->size - written;
    if (copy_size > sizeof(block)) {
      copy_size = sizeof(block);
    }
    memcpy(block, archive->data + source_offset, sizeof(block));
    tea_decrypt_with_key(block, sizeof(block), key);
    memcpy(output + written, block, copy_size);
    written += copy_size;
    source_offset += sizeof(block);
  }
  return 1;
}
