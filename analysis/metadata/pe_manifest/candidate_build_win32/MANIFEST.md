# PE Manifest

Input: `build-win32/samp.dll`

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
file_type	PE32 executable for MS Windows 4.00 (DLL), Intel i386 (stripped to external PDB), 9 sections
sha256	33625f76118fff4d67d5c005202d36684222441b080fb2f5c29ee2962703f436
size_bytes	606222
```

## Sections

```text
name	raw_size	raw_ptr	vma	lma	file_off	align	flags
.text	00068b84	00001000	10001000	10001000	00001000	2**2	CONTENTS, ALLOC, LOAD, READONLY, CODE, DATA
.data	0000473c	0006a000	1006a000	1006a000	0006a000	2**2	CONTENTS, ALLOC, LOAD, DATA
.rdata	000070cc	0006f000	1006f000	1006f000	0006f000	2**2	CONTENTS, ALLOC, LOAD, READONLY, DATA
.eh_frame	00015584	00077000	10077000	10077000	00077000	2**2	CONTENTS, ALLOC, LOAD, READONLY, DATA
.bss	00001874	00000000	1008d000	1008d000	00000000	2**2	ALLOC
.idata	00000e2c	0008d000	1008f000	1008f000	0008d000	2**2	CONTENTS, ALLOC, LOAD, READONLY, DATA
.tls	00000008	0008e000	10090000	10090000	0008e000	2**2	CONTENTS, ALLOC, LOAD, DATA
.rsrc	000002d0	0008f000	10091000	10091000	0008f000	2**2	CONTENTS, ALLOC, LOAD, READONLY, DATA
.reloc	000030a8	00090000	10092000	10092000	00090000	2**2	CONTENTS, LOAD, READONLY, DATA
```

## Import Counts

```text
dll	count
GDI32.dll	2
KERNEL32.dll	40
WS2_32.dll	26
libwinpthread-1.dll	10
msvcrt.dll	50
```
