/* Wide view of CRTDLL's environment, including changes made with _putenv.
 * Cached strings remain valid until that variable is changed or DLL unload.
 * As with getenv, callers must serialize concurrent environment mutations. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "WIDE.H"

typedef struct environment_value {
    struct environment_value *next;
    char *name;
    WCHAR *value;
} ENV_VALUE;
static ENV_VALUE *values;
static HANDLE lock;

WCHAR *rp_wgetenv(const WCHAR *name)
{
    char *narrow, *native;
    WCHAR *wide=NULL;
    ENV_VALUE *entry;
    DWORD wait;
    int count, i;
    if (!name || !*name) { errno=EINVAL; return NULL; }
    narrow=rp_narrow(name);
    if (!narrow) { errno=GetLastError()==ERROR_NOT_ENOUGH_MEMORY ? ENOMEM : EILSEQ; return NULL; }
    wait=WaitForSingleObject(lock,INFINITE);
    if (wait!=WAIT_OBJECT_0 && wait!=WAIT_ABANDONED) {
        rp_free_name(narrow,GetLastError()); errno=EINVAL; return NULL;
    }
    native=getenv(narrow);
    if (!native) goto done;
    count=MultiByteToWideChar(CP_ACP,0,native,-1,NULL,0);
    if (!count) { errno=EILSEQ; goto done; }
    wide=malloc((size_t)count*sizeof(WCHAR));
    if (!wide) { errno=ENOMEM; goto done; }
    if (!MultiByteToWideChar(CP_ACP,0,native,-1,wide,count)) {
        free(wide); wide=NULL; errno=EILSEQ; goto done;
    }
    for (entry=values; entry; entry=entry->next)
        if (!_stricmp(narrow,entry->name)) break;
    if (entry) {
        for (i=0; i<count && wide[i]==entry->value[i]; ++i) {}
        if (i==count) { free(wide); wide=entry->value; }
        else { free(entry->value); entry->value=wide; }
    } else {
        entry=malloc(sizeof(*entry));
        if (!entry) { free(wide); wide=NULL; errno=ENOMEM; goto done; }
        entry->name=narrow; narrow=NULL; entry->value=wide;
        entry->next=values; values=entry;
    }
done:
    ReleaseMutex(lock); rp_free_name(narrow,GetLastError()); return wide;
}

BOOL rp_env_event(DWORD reason)
{
    ENV_VALUE *entry, *next;
    if (reason==DLL_PROCESS_ATTACH) {
        lock=CreateMutexA(NULL,FALSE,NULL);
        return lock!=NULL;
    }
    if (reason==DLL_PROCESS_DETACH) {
        for (entry=values; entry; entry=next) {
            next=entry->next; rp_free_name(entry->name,0);
            free(entry->value); free(entry);
        }
        values=NULL;
        if (lock) CloseHandle(lock);
        lock=NULL;
    }
    return TRUE;
}
