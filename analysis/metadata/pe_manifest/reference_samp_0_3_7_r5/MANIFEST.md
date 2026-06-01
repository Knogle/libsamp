# PE Manifest

Input: `samp.dll`

## Core Files

- [headers.tsv](headers.tsv)
- [data_directories.tsv](data_directories.tsv)
- [sections.tsv](sections.tsv)
- [imports.tsv](imports.tsv)
- [import_counts.tsv](import_counts.tsv)

## Header Snapshot

```text
key	value
AddressOfEntryPoint	000cbc90
ImageBase	10000000
SectionAlignment	00001000
FileAlignment	00001000
MajorSubsystemVersion	4
MinorSubsystemVersion	0
Subsystem	00000002
DllCharacteristics	00000000
SizeOfStackReserve	00100000
SizeOfStackCommit	00001000
SizeOfHeapReserve	00100000
SizeOfHeapCommit	00001000
file_type	PE32 executable for MS Windows 4.00 (DLL), Intel i386, 5 sections
sha256	b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2
size_bytes	1204224
```

## Sections

```text
name	raw_size	raw_ptr	vma	lma	file_off	align	flags
.text	000e33c4	00001000	10001000	10001000	00001000	2**2	CONTENTS, ALLOC, LOAD, READONLY, CODE
.rdata	00018f1e	000e5000	100e5000	100e5000	000e5000	2**2	CONTENTS, ALLOC, LOAD, READONLY, DATA
.data	0001b000	000fe000	100fe000	100fe000	000fe000	2**2	CONTENTS, ALLOC, LOAD, DATA
.rsrc	000006b0	00119000	10271000	10271000	00119000	2**2	CONTENTS, ALLOC, LOAD, READONLY, DATA
.reloc	0000b9ae	0011a000	10272000	10272000	0011a000	2**2	CONTENTS, LOAD, READONLY, DATA
```

## Import Counts

```text
dll	count
ADVAPI32.dll	4
BASS.dll	17
COMCTL32.dll	1
GDI32.dll	10
KERNEL32.dll	125
PSAPI.DLL	1
SHELL32.dll	1
USER32.dll	88
WINMM.dll	2
WSOCK32.dll	25
d3dx9_25.dll	46
```
