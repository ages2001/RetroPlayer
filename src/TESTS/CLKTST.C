/* Measure scheduler waits before/after opening WinMM and requesting its
 * supported timer resolution. Diagnostic observations, not a speed guarantee. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
static HANDLE report;
static LONGLONG frequency;
static void line(const char *s){DWORD n;WriteFile(report,s,lstrlenA(s),&n,NULL);FlushFileBuffers(report);}
static void measure(const char *mode)
{
 const DWORD waits[]={1,5,10};char text[160];
 for(unsigned k=0;k<3;++k){
  DWORD values[100],sum=0;
  for(unsigned i=0;i<100;++i){LARGE_INTEGER begin,end;QueryPerformanceCounter(&begin);Sleep(waits[k]);QueryPerformanceCounter(&end);
   values[i]=(DWORD)((end.QuadPart-begin.QuadPart)*1000000/frequency);sum+=values[i];}
  for(unsigned i=1;i<100;++i){DWORD value=values[i];unsigned j=i;while(j&&values[j-1]>value){values[j]=values[j-1];--j;}values[j]=value;}
  wsprintfA(text,"MODE=%s SLEEP_MS=%lu MIN_US=%lu MEDIAN_US=%lu P95_US=%lu MAX_US=%lu MEAN_US=%lu\r\n",mode,waits[k],values[0],values[50],values[95],values[99],sum/100);line(text);
 }
}
void WINAPI RPEntry(void)
{
 char path[MAX_PATH],*last=path,text[160];DWORD n=GetModuleFileNameA(NULL,path,MAX_PATH);
 if(!n||n>=MAX_PATH-16)ExitProcess(2);for(char *p=path;*p;++p)if(*p=='\\')last=p+1;*last=0;lstrcatA(path,"CLKTST.TXT");
 report=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);if(report==INVALID_HANDLE_VALUE)ExitProcess(3);
 LARGE_INTEGER f;if(!QueryPerformanceFrequency(&f)||!f.QuadPart)ExitProcess(4);frequency=f.QuadPart;
 line("Scope: legacy scheduler resolution with the installed wave driver.\r\n");measure("BASE");
 WAVEFORMATEX format={WAVE_FORMAT_PCM,1,44100,88200,2,16,0};HWAVEOUT wave=NULL;
 MMRESULT opened=waveOutOpen(&wave,WAVE_MAPPER,&format,0,0,CALLBACK_NULL);
 wsprintfA(text,"WAVE_OPEN=%u\r\n",opened);line(text);if(opened==MMSYSERR_NOERROR)measure("OPEN");
 TIMECAPS caps;MMRESULT got=timeGetDevCaps(&caps,sizeof(caps));UINT period=got?0:caps.wPeriodMin;
 MMRESULT begun=period?timeBeginPeriod(period):TIMERR_NOCANDO;
 wsprintfA(text,"CAPS=%u MIN_PERIOD_MS=%u BEGIN=%u\r\n",got,period,begun);line(text);
 if(begun==TIMERR_NOERROR){measure("PERIOD");timeEndPeriod(period);}
 if(wave)waveOutClose(wave);CloseHandle(report);ExitProcess(0);
}
