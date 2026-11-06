#include <stdbool.h>
#define WORKEREXPORT __declspec(dllexport)
#define LEGACY_SYNC 1
#include "ATWORK.H"
BOOL WINAPI RPEntry(HINSTANCE instance, DWORD reason, LPVOID reserved)
{ (void)instance; (void)reason; (void)reserved; return TRUE; }
