#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>

#include "sampdll/archive/saa.h"
#include "sampdll/archive/win32_archive_fs.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SAMP_ARCHIVE_MAX_BYTES (32u * 1024u * 1024u)
#define SAMP_ARCHIVE_OPEN_RECORDS 50u
#define SAMP_ARCHIVE_FAKE_HANDLE_BASE 0xFF000001u
#define SAMP_ARCHIVE_FAKE_HANDLE_LIMIT 0xFF000101u
#define SAMP_ARCHIVE_MAIN_HASH 0xD83F24DDu
#define SAMP_ARCHIVE_SCRIPT_HASH 0x11A462D1u

/* OBSERVED_037 + STATIC_037 + PROBE_TRACE:
 * These profiles differ only in three PE-header bytes. The hardened profile
 * sets LARGE_ADDRESS_AWARE and NX_COMPAT; its executable code and all six IAT
 * hook slots are byte-identical to the reference image.
 *
 * Reference SHA256=a559aa772fd136379155efa71f00c47aad34bbfeae6196b0fe1047d0645cbd26.
 * Hardened  SHA256=590b4be1bb005f4e07edf6b32df4a95b7ba403fb85472d68aa4275875b345b6f.
 */
#define SAMP_GTA_PE_ENTRY_RVA 0x00424570u
#define SAMP_GTA_PE_IMAGE_SIZE 0x01177000u
#define SAMP_GTA_PE_CHECKSUM 0x00DC5BEAu

typedef struct samp_gta_pe_profile {
  const char *name;
  DWORD timestamp;
  WORD file_characteristics;
  WORD dll_characteristics;
} samp_gta_pe_profile;

static const samp_gta_pe_profile kGtaPeProfiles[] = {
    {"gta_sa_10_us_reference", 0x427101CAu, 0x010Fu, 0x0000u},
    {"gta_sa_10_us_laa_nx", 0x437101CAu, 0x012Fu, 0x0100u},
};

typedef HANDLE(WINAPI *create_file_a_fn)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef BOOL(WINAPI *read_file_fn)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
typedef DWORD(WINAPI *get_file_size_fn)(HANDLE, LPDWORD);
typedef DWORD(WINAPI *set_file_pointer_fn)(HANDLE, LONG, PLONG, DWORD);
typedef BOOL(WINAPI *close_handle_fn)(HANDLE);
typedef DWORD(WINAPI *get_file_type_fn)(HANDLE);

typedef struct samp_archive_record {
  HANDLE handle;
  const uint8_t *data;
  DWORD size;
  DWORD position;
  int active;
} samp_archive_record;

typedef struct samp_iat_hook {
  const char *name;
  DWORD slot_rva;
  void *replacement;
  void **slot;
  void *original;
  int installed;
} samp_iat_hook;

typedef struct samp_archive_fs_state {
  uint8_t *archive_data;
  DWORD archive_size;
  uint8_t *main_data;
  DWORD main_size;
  uint8_t *script_data;
  DWORD script_size;
  CRITICAL_SECTION record_lock;
  int record_lock_initialized;
  samp_archive_record records[SAMP_ARCHIVE_OPEN_RECORDS];
  DWORD next_fake_handle;
  volatile LONG active;
  samp_archive_fs_trace_fn trace;
  create_file_a_fn create_file_a;
  read_file_fn read_file;
  get_file_size_fn get_file_size;
  set_file_pointer_fn set_file_pointer;
  close_handle_fn close_handle;
  get_file_type_fn get_file_type;
} samp_archive_fs_state;

static HANDLE WINAPI archive_create_file_a(LPCSTR file_name, DWORD desired_access, DWORD share_mode,
                                           LPSECURITY_ATTRIBUTES security_attributes, DWORD creation_disposition,
                                           DWORD flags_and_attributes, HANDLE template_file);
static BOOL WINAPI archive_read_file(HANDLE file, LPVOID buffer, DWORD bytes_to_read, LPDWORD bytes_read,
                                     LPOVERLAPPED overlapped);
static DWORD WINAPI archive_get_file_size(HANDLE file, LPDWORD high_size);
static DWORD WINAPI archive_set_file_pointer(HANDLE file, LONG distance, PLONG high_distance, DWORD move_method);
static BOOL WINAPI archive_close_handle(HANDLE object);
static DWORD WINAPI archive_get_file_type(HANDLE file);

/* STATIC_037: gta_sa.exe 1.0 US IAT slots validated from the reference image. */
static samp_iat_hook g_iat_hooks[] = {
    {"GetFileSize", 0x00458074u, (void *)archive_get_file_size, NULL, NULL, 0},
    {"CloseHandle", 0x00458078u, (void *)archive_close_handle, NULL, NULL, 0},
    {"SetFilePointer", 0x00458090u, (void *)archive_set_file_pointer, NULL, NULL, 0},
    {"ReadFile", 0x00458098u, (void *)archive_read_file, NULL, NULL, 0},
    {"CreateFileA", 0x004580A0u, (void *)archive_create_file_a, NULL, NULL, 0},
    {"GetFileType", 0x004581B4u, (void *)archive_get_file_type, NULL, NULL, 0},
};

static samp_archive_fs_state g_archive_fs;

/* STATIC_037: XOR-obfuscated PUBLICKEYBLOB at samp.dll+0xE9998, 148 bytes. */
static const uint8_t kPublicKeyObfuscated[148] = {
    0xACu, 0xA8u, 0xAAu, 0xAAu, 0xAAu, 0x8Eu, 0xAAu, 0xAAu, 0xF8u, 0xF9u, 0xEBu, 0x9Bu,
    0xAAu, 0xAEu, 0xAAu, 0xAAu, 0xABu, 0xAAu, 0xABu, 0xAAu, 0x45u, 0x7Fu, 0xEDu, 0xCDu,
    0x03u, 0x0Au, 0x2Au, 0x39u, 0xF8u, 0xC1u, 0xCEu, 0xB1u, 0x9Cu, 0xC5u, 0x86u, 0x5Au,
    0xA2u, 0x1Du, 0x0Bu, 0xBEu, 0xE4u, 0xF5u, 0xF4u, 0xA6u, 0xE2u, 0x44u, 0x12u, 0x06u,
    0xEEu, 0x4Cu, 0xC7u, 0x4Au, 0x76u, 0xB9u, 0xECu, 0xECu, 0x70u, 0xC6u, 0x22u, 0x6Au,
    0x3Cu, 0x9Fu, 0x30u, 0x42u, 0x15u, 0xCBu, 0x97u, 0x88u, 0xA9u, 0x09u, 0x1Au, 0xA2u,
    0xE2u, 0x0Au, 0x70u, 0x5Du, 0xDFu, 0x71u, 0x2Eu, 0x1Eu, 0xFFu, 0x73u, 0x13u, 0xB1u,
    0xE8u, 0xE5u, 0x93u, 0x9Fu, 0x74u, 0xF3u, 0x4Cu, 0xE9u, 0x98u, 0xA4u, 0x38u, 0xD7u,
    0x03u, 0xC5u, 0x88u, 0x14u, 0x8Au, 0x59u, 0x30u, 0x55u, 0xEBu, 0x21u, 0x4Fu, 0xDBu,
    0xB5u, 0xFEu, 0x11u, 0xEAu, 0x7Au, 0xBBu, 0x2Cu, 0x94u, 0x30u, 0x9Cu, 0x7Du, 0xD8u,
    0x40u, 0x29u, 0x6Du, 0x4Eu, 0x1Eu, 0x1Du, 0x35u, 0xBEu, 0xA0u, 0x5Cu, 0x33u, 0x79u,
    0xECu, 0xACu, 0x4Cu, 0x2Du, 0x8Cu, 0x7Cu, 0x03u, 0x02u, 0xAAu, 0x2Fu, 0x12u, 0x33u,
    0x03u, 0x5Eu, 0xC6u, 0x17u,
};

static void archive_tracef(const char *format, ...) {
  char line[512];
  va_list args;

  if (g_archive_fs.trace == NULL || format == NULL) {
    return;
  }
  va_start(args, format);
  if (vsnprintf(line, sizeof(line), format, args) >= 0) {
    g_archive_fs.trace(line);
  }
  va_end(args);
}

static int copy_resolved_proc(FARPROC proc, void *out_function, size_t function_size) {
  if (proc == NULL || out_function == NULL || function_size != sizeof(proc)) {
    return 0;
  }
  memcpy(out_function, &proc, function_size);
  return 1;
}

static int resolve_kernel_apis(void) {
  HMODULE kernel = GetModuleHandleA("kernel32.dll");

  if (kernel == NULL) {
    return 0;
  }
  return copy_resolved_proc(GetProcAddress(kernel, "CreateFileA"), &g_archive_fs.create_file_a,
                            sizeof(g_archive_fs.create_file_a)) &&
         copy_resolved_proc(GetProcAddress(kernel, "ReadFile"), &g_archive_fs.read_file,
                            sizeof(g_archive_fs.read_file)) &&
         copy_resolved_proc(GetProcAddress(kernel, "GetFileSize"), &g_archive_fs.get_file_size,
                            sizeof(g_archive_fs.get_file_size)) &&
         copy_resolved_proc(GetProcAddress(kernel, "SetFilePointer"), &g_archive_fs.set_file_pointer,
                            sizeof(g_archive_fs.set_file_pointer)) &&
         copy_resolved_proc(GetProcAddress(kernel, "CloseHandle"), &g_archive_fs.close_handle,
                            sizeof(g_archive_fs.close_handle)) &&
         copy_resolved_proc(GetProcAddress(kernel, "GetFileType"), &g_archive_fs.get_file_type,
                            sizeof(g_archive_fs.get_file_type));
}

static int load_archive(const char *path) {
  HANDLE file;
  DWORD high_size = 0;
  DWORD size;
  DWORD total = 0;

  file = g_archive_fs.create_file_a(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
  if (file == INVALID_HANDLE_VALUE) {
    archive_tracef("open_failed path='%s' gle=%lu evidence=TODO_VERIFY", path, (unsigned long)GetLastError());
    return 0;
  }
  size = g_archive_fs.get_file_size(file, &high_size);
  if (high_size != 0u || size == 0u || size > SAMP_ARCHIVE_MAX_BYTES) {
    archive_tracef("size_rejected low=%lu high=%lu max=%lu evidence=INFERRED", (unsigned long)size,
                   (unsigned long)high_size, (unsigned long)SAMP_ARCHIVE_MAX_BYTES);
    g_archive_fs.close_handle(file);
    return 0;
  }
  g_archive_fs.archive_data = (uint8_t *)HeapAlloc(GetProcessHeap(), 0u, size);
  if (g_archive_fs.archive_data == NULL) {
    g_archive_fs.close_handle(file);
    return 0;
  }
  while (total < size) {
    DWORD chunk = 0;
    if (!g_archive_fs.read_file(file, g_archive_fs.archive_data + total, size - total, &chunk, NULL) || chunk == 0u) {
      archive_tracef("read_failed offset=%lu size=%lu gle=%lu", (unsigned long)total, (unsigned long)size,
                     (unsigned long)GetLastError());
      g_archive_fs.close_handle(file);
      return 0;
    }
    total += chunk;
  }
  if (!g_archive_fs.close_handle(file)) {
    archive_tracef("close_failed gle=%lu", (unsigned long)GetLastError());
    return 0;
  }
  g_archive_fs.archive_size = size;
  return 1;
}

static int verify_archive_signature(const samp_saa_layout *layout) {
  HCRYPTPROV provider = 0;
  HCRYPTHASH hash = 0;
  HCRYPTKEY key = 0;
  uint8_t public_key[sizeof(kPublicKeyObfuscated)];
  size_t i;
  BOOL verified = FALSE;

  if (layout == NULL || layout->signature_size != 128u ||
      layout->signature_offset < layout->header_size || layout->signature_offset > g_archive_fs.archive_size) {
    return 0;
  }
  for (i = 0; i < sizeof(public_key); ++i) {
    public_key[i] = kPublicKeyObfuscated[i] ^ 0xAAu;
  }
  if (!CryptAcquireContextA(&provider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT) ||
      !CryptCreateHash(provider, CALG_SHA1, 0, 0, &hash) ||
      !CryptHashData(hash, g_archive_fs.archive_data + layout->header_size,
                     (DWORD)(layout->signature_offset - layout->header_size), 0) ||
      !CryptImportKey(provider, public_key, (DWORD)sizeof(public_key), 0, 0, &key)) {
    archive_tracef("signature_setup_failed gle=%lu evidence=STATIC_037", (unsigned long)GetLastError());
    goto cleanup;
  }
  verified = CryptVerifySignatureA(hash, g_archive_fs.archive_data + layout->signature_offset,
                                   layout->signature_size, key, NULL, CRYPT_NOHASHOID);
  if (!verified) {
    archive_tracef("signature_rejected gle=%lu evidence=STATIC_037", (unsigned long)GetLastError());
  }

cleanup:
  if (key != 0) {
    CryptDestroyKey(key);
  }
  if (hash != 0) {
    CryptDestroyHash(hash);
  }
  if (provider != 0) {
    CryptReleaseContext(provider, 0);
  }
  SecureZeroMemory(public_key, sizeof(public_key));
  return verified ? 1 : 0;
}

static int extract_required_file(const samp_saa_archive *archive, uint32_t hash, const char *name,
                                 uint8_t **out_data, DWORD *out_size) {
  samp_saa_file file;
  uint8_t *data;

  if (!samp_saa_find(archive, hash, &file) || file.size == 0u) {
    archive_tracef("required_file_missing name='%s' hash=0x%08lx", name, (unsigned long)hash);
    return 0;
  }
  data = (uint8_t *)HeapAlloc(GetProcessHeap(), 0u, file.size);
  if (data == NULL || !samp_saa_extract(archive, &file, data, file.size)) {
    if (data != NULL) {
      HeapFree(GetProcessHeap(), 0u, data);
    }
    archive_tracef("required_file_extract_failed name='%s' size=%lu", name, (unsigned long)file.size);
    return 0;
  }
  *out_data = data;
  *out_size = file.size;
  return 1;
}

static int validate_gta_and_iat(void) {
  uint8_t *base = (uint8_t *)GetModuleHandleA(NULL);
  IMAGE_DOS_HEADER *dos;
  IMAGE_NT_HEADERS32 *nt;
  const samp_gta_pe_profile *profile = NULL;
  size_t i;

  if (base == NULL) {
    return 0;
  }
  dos = (IMAGE_DOS_HEADER *)base;
  if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < (LONG)sizeof(*dos) ||
      (DWORD)dos->e_lfanew > SAMP_GTA_PE_IMAGE_SIZE - sizeof(*nt)) {
    archive_tracef("gta_profile_rejected reason=dos_header");
    return 0;
  }
  nt = (IMAGE_NT_HEADERS32 *)(base + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
      nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
      nt->OptionalHeader.AddressOfEntryPoint != SAMP_GTA_PE_ENTRY_RVA ||
      nt->OptionalHeader.SizeOfImage != SAMP_GTA_PE_IMAGE_SIZE ||
      nt->OptionalHeader.CheckSum != SAMP_GTA_PE_CHECKSUM) {
    archive_tracef("gta_profile_rejected reason=pe_fields timestamp=0x%08lx entry=0x%08lx size=0x%08lx "
                   "checksum=0x%08lx characteristics=0x%04x dll_characteristics=0x%04x "
                   "evidence=STATIC_037,PROBE_TRACE",
                   (unsigned long)nt->FileHeader.TimeDateStamp,
                   (unsigned long)nt->OptionalHeader.AddressOfEntryPoint,
                   (unsigned long)nt->OptionalHeader.SizeOfImage, (unsigned long)nt->OptionalHeader.CheckSum,
                   (unsigned int)nt->FileHeader.Characteristics,
                   (unsigned int)nt->OptionalHeader.DllCharacteristics);
    return 0;
  }

  for (i = 0; i < sizeof(kGtaPeProfiles) / sizeof(kGtaPeProfiles[0]); ++i) {
    const samp_gta_pe_profile *candidate = &kGtaPeProfiles[i];
    if (nt->FileHeader.TimeDateStamp == candidate->timestamp &&
        nt->FileHeader.Characteristics == candidate->file_characteristics &&
        nt->OptionalHeader.DllCharacteristics == candidate->dll_characteristics) {
      profile = candidate;
      break;
    }
  }
  if (profile == NULL) {
    archive_tracef("gta_profile_rejected reason=header_variant timestamp=0x%08lx characteristics=0x%04x "
                   "dll_characteristics=0x%04x evidence=STATIC_037,PROBE_TRACE",
                   (unsigned long)nt->FileHeader.TimeDateStamp,
                   (unsigned int)nt->FileHeader.Characteristics,
                   (unsigned int)nt->OptionalHeader.DllCharacteristics);
    return 0;
  }
  archive_tracef("gta_profile_accepted name=%s timestamp=0x%08lx characteristics=0x%04x "
                 "dll_characteristics=0x%04x evidence=STATIC_037,PROBE_TRACE",
                 profile->name, (unsigned long)profile->timestamp,
                 (unsigned int)profile->file_characteristics,
                 (unsigned int)profile->dll_characteristics);

  for (i = 0; i < sizeof(g_iat_hooks) / sizeof(g_iat_hooks[0]); ++i) {
    FARPROC expected = GetProcAddress(GetModuleHandleA("kernel32.dll"), g_iat_hooks[i].name);
    g_iat_hooks[i].slot = (void **)(base + g_iat_hooks[i].slot_rva);
    g_iat_hooks[i].original = *g_iat_hooks[i].slot;
    if (expected == NULL || g_iat_hooks[i].original != (void *)expected) {
      archive_tracef("iat_rejected api=%s rva=0x%08lx current=%p expected=%p evidence=STATIC_037",
                     g_iat_hooks[i].name, (unsigned long)g_iat_hooks[i].slot_rva,
                     g_iat_hooks[i].original, expected);
      return 0;
    }
  }
  return 1;
}

static int write_iat_slot(samp_iat_hook *hook, void *expected, void *replacement) {
  DWORD old_protect;
  DWORD ignored;

  if (hook == NULL || hook->slot == NULL || *hook->slot != expected ||
      !VirtualProtect(hook->slot, sizeof(*hook->slot), PAGE_READWRITE, &old_protect)) {
    return 0;
  }
  *hook->slot = replacement;
  FlushInstructionCache(GetCurrentProcess(), hook->slot, sizeof(*hook->slot));
  if (!VirtualProtect(hook->slot, sizeof(*hook->slot), old_protect, &ignored)) {
    /* The pointer write already happened. Treat it as installed so rollback
     * can still restore it; emit the protection failure at the call site. */
    archive_tracef("iat_protection_restore_failed api=%s rva=0x%08lx gle=%lu",
                   hook->name, (unsigned long)hook->slot_rva, (unsigned long)GetLastError());
  }
  return 1;
}

static void restore_iat_hooks(void) {
  size_t i = sizeof(g_iat_hooks) / sizeof(g_iat_hooks[0]);

  while (i > 0u) {
    samp_iat_hook *hook = &g_iat_hooks[--i];
    if (hook->installed) {
      if (write_iat_slot(hook, hook->replacement, hook->original)) {
        archive_tracef("iat_restored api=%s rva=0x%08lx", hook->name, (unsigned long)hook->slot_rva);
      } else {
        archive_tracef("iat_restore_skipped api=%s rva=0x%08lx current=%p reason=changed_or_protect_failed",
                       hook->name, (unsigned long)hook->slot_rva,
                       hook->slot != NULL ? *hook->slot : NULL);
      }
      hook->installed = 0;
    }
  }
}

static int install_iat_hooks(void) {
  size_t i;

  for (i = 0; i < sizeof(g_iat_hooks) / sizeof(g_iat_hooks[0]); ++i) {
    if (!write_iat_slot(&g_iat_hooks[i], g_iat_hooks[i].original, g_iat_hooks[i].replacement)) {
      archive_tracef("iat_install_failed api=%s rva=0x%08lx gle=%lu", g_iat_hooks[i].name,
                     (unsigned long)g_iat_hooks[i].slot_rva, (unsigned long)GetLastError());
      restore_iat_hooks();
      return 0;
    }
    g_iat_hooks[i].installed = 1;
    archive_tracef("iat_installed api=%s rva=0x%08lx original=%p replacement=%p evidence=STATIC_037",
                   g_iat_hooks[i].name, (unsigned long)g_iat_hooks[i].slot_rva,
                   g_iat_hooks[i].original, g_iat_hooks[i].replacement);
  }
  return 1;
}

static samp_archive_record *find_record(HANDLE handle) {
  size_t i;

  for (i = 0; i < SAMP_ARCHIVE_OPEN_RECORDS; ++i) {
    if (g_archive_fs.records[i].active && g_archive_fs.records[i].handle == handle) {
      return &g_archive_fs.records[i];
    }
  }
  return NULL;
}

static HANDLE allocate_record(const uint8_t *data, DWORD size) {
  size_t slot;
  DWORD attempts;

  for (slot = 0; slot < SAMP_ARCHIVE_OPEN_RECORDS; ++slot) {
    if (!g_archive_fs.records[slot].active) {
      for (attempts = 0; attempts < 128u; ++attempts) {
        HANDLE candidate = (HANDLE)(uintptr_t)g_archive_fs.next_fake_handle;
        g_archive_fs.next_fake_handle += 2u;
        if (g_archive_fs.next_fake_handle >= SAMP_ARCHIVE_FAKE_HANDLE_LIMIT) {
          g_archive_fs.next_fake_handle = SAMP_ARCHIVE_FAKE_HANDLE_BASE;
        }
        if (find_record(candidate) == NULL) {
          g_archive_fs.records[slot].handle = candidate;
          g_archive_fs.records[slot].data = data;
          g_archive_fs.records[slot].size = size;
          g_archive_fs.records[slot].position = 0u;
          g_archive_fs.records[slot].active = 1;
          return candidate;
        }
      }
      break;
    }
  }
  return NULL;
}

static int select_virtual_file(const char *path, const uint8_t **out_data, DWORD *out_size, const char **out_name) {
  uint32_t hash;

  if (!samp_saa_hash_path(path, &hash)) {
    return 0;
  }
  if (hash == SAMP_ARCHIVE_MAIN_HASH) {
    *out_data = g_archive_fs.main_data;
    *out_size = g_archive_fs.main_size;
    *out_name = "main.scm";
    return 1;
  }
  if (hash == SAMP_ARCHIVE_SCRIPT_HASH) {
    *out_data = g_archive_fs.script_data;
    *out_size = g_archive_fs.script_size;
    *out_name = "script.img";
    return 1;
  }
  return 0;
}

static HANDLE WINAPI archive_create_file_a(LPCSTR file_name, DWORD desired_access, DWORD share_mode,
                                           LPSECURITY_ATTRIBUTES security_attributes, DWORD creation_disposition,
                                           DWORD flags_and_attributes, HANDLE template_file) {
  const uint8_t *data;
  const char *name;
  DWORD size;
  HANDLE handle;

  if (InterlockedCompareExchange(&g_archive_fs.active, 0, 0) &&
      select_virtual_file(file_name, &data, &size, &name)) {
    EnterCriticalSection(&g_archive_fs.record_lock);
    handle = allocate_record(data, size);
    LeaveCriticalSection(&g_archive_fs.record_lock);
    if (handle != NULL) {
      archive_tracef("open_virtual name='%s' handle=0x%08lx size=%lu evidence=STATIC_037",
                     name, (unsigned long)(uintptr_t)handle, (unsigned long)size);
      return handle;
    }
    SetLastError(ERROR_TOO_MANY_OPEN_FILES);
    archive_tracef("open_virtual_failed name='%s' reason=record_capacity max=%u evidence=INFERRED",
                   name, (unsigned)SAMP_ARCHIVE_OPEN_RECORDS);
    return NULL;
  }
  return g_archive_fs.create_file_a(file_name, desired_access, share_mode, security_attributes,
                                    creation_disposition, flags_and_attributes, template_file);
}

static BOOL WINAPI archive_read_file(HANDLE file, LPVOID buffer, DWORD bytes_to_read, LPDWORD bytes_read,
                                     LPOVERLAPPED overlapped) {
  samp_archive_record *record;
  HANDLE event = NULL;
  DWORD available;
  DWORD copy_size;
  int eof = 0;

  if (!InterlockedCompareExchange(&g_archive_fs.active, 0, 0)) {
    return g_archive_fs.read_file(file, buffer, bytes_to_read, bytes_read, overlapped);
  }
  EnterCriticalSection(&g_archive_fs.record_lock);
  record = find_record(file);
  if (record == NULL) {
    LeaveCriticalSection(&g_archive_fs.record_lock);
    return g_archive_fs.read_file(file, buffer, bytes_to_read, bytes_read, overlapped);
  }
  if (bytes_read != NULL) {
    *bytes_read = 0u;
  }
  if (overlapped != NULL) {
    record->position = overlapped->Offset;
    event = overlapped->hEvent;
  }
  if (record->position >= record->size) {
    eof = 1;
    copy_size = 0u;
  } else {
    available = record->size - record->position;
    copy_size = bytes_to_read < available ? bytes_to_read : available;
    if (copy_size != 0u && buffer == NULL) {
      LeaveCriticalSection(&g_archive_fs.record_lock);
      SetLastError(ERROR_INVALID_PARAMETER);
      return FALSE;
    }
    if (copy_size != 0u) {
      memcpy(buffer, record->data + record->position, copy_size);
      record->position += copy_size;
    }
    if (bytes_read != NULL) {
      *bytes_read = copy_size;
    }
    eof = bytes_to_read >= available;
  }
  LeaveCriticalSection(&g_archive_fs.record_lock);
  if (event != NULL) {
    SetEvent(event);
  }
  if (eof) {
    SetLastError(ERROR_HANDLE_EOF);
  }
  return TRUE;
}

static DWORD WINAPI archive_get_file_size(HANDLE file, LPDWORD high_size) {
  samp_archive_record *record;
  DWORD size;

  if (!InterlockedCompareExchange(&g_archive_fs.active, 0, 0)) {
    return g_archive_fs.get_file_size(file, high_size);
  }
  EnterCriticalSection(&g_archive_fs.record_lock);
  record = find_record(file);
  if (record == NULL) {
    LeaveCriticalSection(&g_archive_fs.record_lock);
    return g_archive_fs.get_file_size(file, high_size);
  }
  size = record->size;
  if (high_size != NULL) {
    *high_size = 0;
  }
  LeaveCriticalSection(&g_archive_fs.record_lock);
  return size;
}

static DWORD WINAPI archive_set_file_pointer(HANDLE file, LONG distance, PLONG high_distance, DWORD move_method) {
  samp_archive_record *record;
  int64_t next;
  DWORD result;

  if (!InterlockedCompareExchange(&g_archive_fs.active, 0, 0)) {
    return g_archive_fs.set_file_pointer(file, distance, high_distance, move_method);
  }
  EnterCriticalSection(&g_archive_fs.record_lock);
  record = find_record(file);
  if (record == NULL) {
    LeaveCriticalSection(&g_archive_fs.record_lock);
    return g_archive_fs.set_file_pointer(file, distance, high_distance, move_method);
  }
  if (move_method == FILE_BEGIN) {
    next = distance;
  } else if (move_method == FILE_CURRENT) {
    next = (int64_t)record->position + distance;
  } else if (move_method == FILE_END) {
    /* STATIC_037: original ignores distance for FILE_END. */
    next = record->size;
  } else {
    LeaveCriticalSection(&g_archive_fs.record_lock);
    SetLastError(ERROR_INVALID_PARAMETER);
    return INVALID_SET_FILE_POINTER;
  }
  if (next < 0 || next > 0xFFFFFFFFLL) {
    LeaveCriticalSection(&g_archive_fs.record_lock);
    SetLastError(ERROR_NEGATIVE_SEEK);
    return INVALID_SET_FILE_POINTER;
  }
  record->position = (DWORD)next;
  result = record->position;
  LeaveCriticalSection(&g_archive_fs.record_lock);
  return result;
}

static BOOL WINAPI archive_close_handle(HANDLE object) {
  samp_archive_record *record;

  if (!InterlockedCompareExchange(&g_archive_fs.active, 0, 0)) {
    return g_archive_fs.close_handle(object);
  }
  EnterCriticalSection(&g_archive_fs.record_lock);
  record = find_record(object);
  if (record == NULL) {
    LeaveCriticalSection(&g_archive_fs.record_lock);
    return g_archive_fs.close_handle(object);
  }
  memset(record, 0, sizeof(*record));
  LeaveCriticalSection(&g_archive_fs.record_lock);
  archive_tracef("close_virtual handle=0x%08lx evidence=STATIC_037", (unsigned long)(uintptr_t)object);
  return TRUE;
}

static DWORD WINAPI archive_get_file_type(HANDLE file) {
  samp_archive_record *record;

  if (!InterlockedCompareExchange(&g_archive_fs.active, 0, 0)) {
    return g_archive_fs.get_file_type(file);
  }
  EnterCriticalSection(&g_archive_fs.record_lock);
  record = find_record(file);
  LeaveCriticalSection(&g_archive_fs.record_lock);
  if (record != NULL) {
    return FILE_TYPE_DISK;
  }
  return g_archive_fs.get_file_type(file);
}

static void free_archive_data(void) {
  HANDLE heap = GetProcessHeap();

  if (g_archive_fs.script_data != NULL) {
    HeapFree(heap, 0u, g_archive_fs.script_data);
    g_archive_fs.script_data = NULL;
  }
  if (g_archive_fs.main_data != NULL) {
    HeapFree(heap, 0u, g_archive_fs.main_data);
    g_archive_fs.main_data = NULL;
  }
  if (g_archive_fs.archive_data != NULL) {
    HeapFree(heap, 0u, g_archive_fs.archive_data);
    g_archive_fs.archive_data = NULL;
  }
}

int samp_archive_fs_win32_start(const char *archive_path, samp_archive_fs_trace_fn trace,
                                samp_archive_fs_status *out_status) {
  samp_saa_layout layout;
  samp_saa_archive archive;
  int signature_verified = 0;

  if (out_status != NULL) {
    memset(out_status, 0, sizeof(*out_status));
  }
  if (archive_path == NULL || archive_path[0] == '\0' ||
      InterlockedCompareExchange(&g_archive_fs.active, 0, 0)) {
    return 0;
  }
  memset(&g_archive_fs, 0, sizeof(g_archive_fs));
  g_archive_fs.trace = trace;
  g_archive_fs.next_fake_handle = SAMP_ARCHIVE_FAKE_HANDLE_BASE;

  if (!resolve_kernel_apis() || !load_archive(archive_path) ||
      !samp_saa_parse_layout(g_archive_fs.archive_data, g_archive_fs.archive_size, &layout)) {
    archive_tracef("start_failed phase=load_or_layout path='%s'", archive_path);
    goto fail;
  }
  signature_verified = verify_archive_signature(&layout);
  if (!signature_verified ||
      !samp_saa_open_verified(&archive, g_archive_fs.archive_data, g_archive_fs.archive_size,
                              signature_verified) ||
      !extract_required_file(&archive, SAMP_ARCHIVE_MAIN_HASH, "main.scm", &g_archive_fs.main_data,
                             &g_archive_fs.main_size) ||
      !extract_required_file(&archive, SAMP_ARCHIVE_SCRIPT_HASH, "script.img", &g_archive_fs.script_data,
                             &g_archive_fs.script_size)) {
    archive_tracef("start_failed phase=verify_or_extract signature=%d", signature_verified);
    goto fail;
  }
  if (g_archive_fs.main_size < 3u || g_archive_fs.main_data[0] != 0x02u ||
      g_archive_fs.main_data[1] != 0x00u || g_archive_fs.main_data[2] != 0x01u ||
      g_archive_fs.script_size < 4u || memcmp(g_archive_fs.script_data, "VER2", 4u) != 0) {
    archive_tracef("start_failed phase=payload_sanity main_size=%lu script_size=%lu",
                   (unsigned long)g_archive_fs.main_size, (unsigned long)g_archive_fs.script_size);
    goto fail;
  }
  if (!validate_gta_and_iat()) {
    archive_tracef("start_failed phase=gta_iat_validation");
    goto fail;
  }

  InitializeCriticalSection(&g_archive_fs.record_lock);
  g_archive_fs.record_lock_initialized = 1;
  if (!install_iat_hooks()) {
    goto fail;
  }
  InterlockedExchange(&g_archive_fs.active, 1);
  archive_tracef("ready archive=%lu signature=%lu main=%lu script=%lu hooks=6 scope=gta_sa_iat "
                 "evidence=STATIC_037",
                 (unsigned long)g_archive_fs.archive_size, (unsigned long)layout.signature_size,
                 (unsigned long)g_archive_fs.main_size, (unsigned long)g_archive_fs.script_size);

  if (out_status != NULL) {
    out_status->archive_size = g_archive_fs.archive_size;
    out_status->signature_size = layout.signature_size;
    out_status->main_size = g_archive_fs.main_size;
    out_status->script_size = g_archive_fs.script_size;
    out_status->signature_verified = 1;
    out_status->hooks_installed = 1;
  }
  return 1;

fail:
  InterlockedExchange(&g_archive_fs.active, 0);
  restore_iat_hooks();
  if (g_archive_fs.record_lock_initialized) {
    DeleteCriticalSection(&g_archive_fs.record_lock);
    g_archive_fs.record_lock_initialized = 0;
  }
  free_archive_data();
  return 0;
}

void samp_archive_fs_win32_stop(void) {
  samp_archive_fs_trace_fn trace = g_archive_fs.trace;

  InterlockedExchange(&g_archive_fs.active, 0);
  restore_iat_hooks();
  if (g_archive_fs.record_lock_initialized) {
    EnterCriticalSection(&g_archive_fs.record_lock);
    memset(g_archive_fs.records, 0, sizeof(g_archive_fs.records));
    LeaveCriticalSection(&g_archive_fs.record_lock);
    DeleteCriticalSection(&g_archive_fs.record_lock);
    g_archive_fs.record_lock_initialized = 0;
  }
  free_archive_data();
  if (trace != NULL) {
    trace("stopped hooks=0");
  }
  memset(&g_archive_fs, 0, sizeof(g_archive_fs));
}
#endif
