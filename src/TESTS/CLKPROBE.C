/* Native calibration diagnostic. Report after sampling to avoid disk jitter. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define CLOCK_FREQ 1000000LL
typedef int64_t vlc_tick_t;
static uint64_t diag_ticks[8];
static DWORD diag_ms[8];
static unsigned diag_count,diag_failure;
static int RPFail(unsigned line) { if(!diag_failure)diag_failure=line;return 0; }
#include "RPCLOCK.H"
static HANDLE report;
static void line(const char *text)
{ DWORD n;WriteFile(report,text,lstrlenA(text),&n,NULL); }
void WINAPI RPEntry(void)
{
    char path[MAX_PATH],*last=path,text[256];
    DWORD n=GetModuleFileNameA(NULL,path,MAX_PATH);
    if(!n||n>=MAX_PATH-16)ExitProcess(2);
    for(char *p=path;*p;++p)if(*p=='\\')last=p+1;
    *last=0;lstrcatA(path,"CLKPROBE.TXT");
    report=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
    if(report==INVALID_HANDLE_VALUE)ExitProcess(3);
    HMODULE timer=LoadLibraryA("WINMM.DLL");
    unsigned (WINAPI *begin)(unsigned)=timer?(void*)GetProcAddress(timer,"timeBeginPeriod"):NULL;
    unsigned (WINAPI *end)(unsigned)=timer?(void*)GetProcAddress(timer,"timeEndPeriod"):NULL;
    unsigned period=begin?begin(5):0xffffffff;
    wsprintfA(text,"Scope: actual idle calibration diagnostics; not playback acceptance.\r\nPERIOD_RESULT=%u\r\n",period);line(text);
    for(unsigned run=0;run<3;++run) {
        diag_count=diag_failure=0;
        int selected=rp_tsc_setup();
        wsprintfA(text,"RUN=%u SELECTED=%u FAILURE_LINE=%u SAMPLES=%u RATE_HI=%lu RATE_LO=%lu\r\n",
            run,selected,diag_failure,diag_count,(DWORD)(rp_tsc_clock.frequency>>32),(DWORD)rp_tsc_clock.frequency);line(text);
        for(unsigned i=0;i<diag_count;++i) {
            wsprintfA(text,"SAMPLE=%u MS=%lu TSC_HI=%lu TSC_LO=%lu\r\n",i,diag_ms[i],
                (DWORD)(diag_ticks[i]>>32),(DWORD)diag_ticks[i]);line(text);
        }
        if(selected) {
            vlc_tick_t before=mdate_tsc();Sleep(100);vlc_tick_t after=mdate_tsc();
            wsprintfA(text,"CLOCK_DELTA_US=%ld\r\n",(long)(after-before));line(text);
        }
        Sleep(250);
    }
    if(end && period==0)end(5);
    if(timer)FreeLibrary(timer);
    line("RESULT=DIAGNOSTIC\r\n");CloseHandle(report);ExitProcess(0);
}
