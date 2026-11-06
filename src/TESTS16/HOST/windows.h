/* Host shim: just enough of the Win16 API for DECODE.C regression tests. */
#ifndef RP16_HOST_WINDOWS_H
#define RP16_HOST_WINDOWS_H
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define FAR
#define NEAR
#define PASCAL
typedef int BOOL; typedef uint8_t BYTE; typedef uint16_t WORD; typedef uint32_t DWORD; typedef int32_t LONG;
typedef const char *LPCSTR; typedef char *LPSTR; typedef int HFILE; typedef void *HGLOBAL;
#define TRUE 1
#define FALSE 0
#define HFILE_ERROR (-1)
#define OF_READ 0
#define OF_SHARE_DENY_WRITE 0
#define GMEM_MOVEABLE 0
static inline HFILE _lopen(LPCSTR p, int m){(void)m;FILE*f=fopen(p,"rb");return f?(HFILE)(intptr_t)fileno(f):HFILE_ERROR;}
#include <unistd.h>
static inline WORD _lread(HFILE f, void *b, WORD n){ssize_t r=read(f,b,n);return r<0?(WORD)HFILE_ERROR:(WORD)r;}
static inline LONG _llseek(HFILE f, LONG o, int w){return (LONG)lseek(f,o,w);}
static inline void _lclose(HFILE f){close(f);}
static inline HGLOBAL GlobalAlloc(int f, DWORD n){(void)f;return malloc(n);}
static inline void *GlobalLock(HGLOBAL h){return h;}
static inline void GlobalUnlock(HGLOBAL h){(void)h;}
static inline void GlobalFree(HGLOBAL h){free(h);}
#define lstrcpy strcpy
#define _fmemcmp memcmp
#define _fmemset memset
#define _fmemcpy memcpy
#endif
