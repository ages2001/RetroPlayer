/* Private guest report export; copies application text reports to the floppy. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
void WINAPI RPEntry(void)
{
    static const char *names[]={"SUITE.TXT","CORETEST.TXT","PCMTEST.TXT","MEDTEST.TXT",
        "KTEST.TXT","CRTEST.TXT","PCMLOG.TXT","MEDLOG.TXT","TRACE.TXT"};
    char source[MAX_PATH],dest[MAX_PATH];
    unsigned i;
    BOOL success=TRUE;
    for(i=0;i<sizeof(names)/sizeof(names[0]);++i) {
        lstrcpyA(source,LOBYTE(LOWORD(GetVersion()))<4 ? "C:\\RETROPLA\\" :
                  "C:\\Program Files\\RetroPlayer\\");
        lstrcatA(source,names[i]); lstrcpyA(dest,"A:\\"); lstrcatA(dest,names[i]);
        if(!CopyFileA(source,dest,FALSE)) success=FALSE;
    }
    ExitProcess(success?0:1);
}
