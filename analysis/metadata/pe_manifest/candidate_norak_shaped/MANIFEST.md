# PE Manifest

Input: `build-win32-norak/samp-shaped.dll`

## Core Files

- [headers.tsv](headers.tsv)
- [data_directories.tsv](data_directories.tsv)
- [sections.tsv](sections.tsv)
- [imports.tsv](imports.tsv)
- [import_counts.tsv](import_counts.tsv)

## Header Snapshot

```text
key	value
AddressOfEntryPoint	000011f0
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
file_type	PE32 executable for MS Windows 4.00 (DLL), Intel i386 (stripped to external PDB), 10 sections
sha256	d474601b65b937c38a1b751ec679b6db276a605d51270d161c032e0930fcc970
size_bytes	98332
```

## Sections

```text
name	raw_size	raw_ptr	vma	lma	file_off	align	flags
.text	0000bfe0	00001000	10001000	10001000	00001000	2**2	CONTENTS, ALLOC, LOAD, READONLY, CODE
.data	00000c3c	0000d000	1000d000	1000d000	0000d000	2**2	CONTENTS, ALLOC, LOAD, DATA
.rdata	00001b5c	0000e000	1000e000	1000e000	0000e000	2**2	CONTENTS, ALLOC, LOAD, READONLY, DATA
.eh_frame	00001384	00010000	10010000	10010000	00010000	2**2	CONTENTS, ALLOC, LOAD, READONLY, DATA
.bss	00000a30	00000000	10012000	10012000	00000000	2**2	ALLOC
.idata	000009ac	00012000	10013000	10013000	00012000	2**2	CONTENTS, ALLOC, LOAD, READONLY, DATA
.tls	00000008	00013000	10014000	10014000	00013000	2**2	CONTENTS, ALLOC, LOAD, DATA
.rsrc	000002d0	00014000	10015000	10015000	00014000	2**2	CONTENTS, ALLOC, LOAD, READONLY, DATA
.reloc	00000a34	00015000	10016000	10016000	00015000	2**2	CONTENTS, LOAD, READONLY, DATA
.idata	00002000	00016000	10017000	10017000	00016000	2**2	CONTENTS, ALLOC, LOAD, CODE, DATA
```

## Import Counts

```text
dll	count
ADVAPI32.dll	4
BASS.dll	17
COMCTL32.dll	1
GDI32.dll	2
KERNEL32.dll	33
PSAPI.DLL	1
SHELL32.dll	1
USER32.dll	88
WINMM.dll	2
WSOCK32.dll	22
d3dx9_25.dll	46
msvcrt.dll	30
```
