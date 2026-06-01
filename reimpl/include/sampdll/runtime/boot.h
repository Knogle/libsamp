#ifndef SAMPDLL_RUNTIME_BOOT_H
#define SAMPDLL_RUNTIME_BOOT_H

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* Central runtime lifecycle dispatch used by DllMain. */
int samp_runtime_dispatch(HINSTANCE instance, DWORD reason, LPVOID reserved);
#endif

#endif
