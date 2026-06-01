#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "sampdll/runtime/boot.h"

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
  return samp_runtime_dispatch(instance, reason, reserved) ? TRUE : FALSE;
}
#endif
