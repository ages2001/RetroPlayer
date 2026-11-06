/* Observe waveOut sample-position latency without the VLC clock/resampler.
 * Buffered diagnostics avoid disk I/O in the measurement loop. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
static short pcm[19200];
static WAVEHDR headers[16];
static struct { DWORD us, samples, flags; } rows[1000];
static HANDLE report;
static unsigned errors;
static void line(const char *text) { DWORD n; WriteFile(report,text,lstrlenA(text),&n,NULL); }
static BOOL check(MMRESULT rc) { if(rc) ++errors; return rc==MMSYSERR_NOERROR; }
static void measure(HWAVEOUT wave, unsigned run, LONGLONG frequency)
{
    char text[160]; LARGE_INTEGER start, now; unsigned count=0, prepared=0;
    if(!check(waveOutReset(wave)) || !check(waveOutPause(wave))) return;
    for(unsigned i=0, offset=0;i<16;++i) {
        volatile unsigned char *bytes=(volatile unsigned char *)&headers[i];
        for(unsigned j=0;j<sizeof(headers[i]);++j) bytes[j]=0;
        headers[i].lpData=(char *)(pcm+offset);
        headers[i].dwBufferLength=(i?960:4800)*sizeof(short);
        offset+=i?960:4800;
        if(!check(waveOutPrepareHeader(wave,&headers[i],sizeof(headers[i])))) break;
        ++prepared;
        if(!check(waveOutWrite(wave,&headers[i],sizeof(headers[i])))) break;
    }
    QueryPerformanceCounter(&start);
    if(prepared==16 && check(waveOutRestart(wave))) {
        do {
            MMTIME position; position.wType=TIME_SAMPLES;
            if(!check(waveOutGetPosition(wave,&position,sizeof(position)))) break;
            if(position.wType!=TIME_SAMPLES) { ++errors; break; }
            QueryPerformanceCounter(&now);
            rows[count].us=(DWORD)((now.QuadPart-start.QuadPart)*1000000/frequency);
            rows[count].samples=position.u.sample;
            rows[count].flags=headers[15].dwFlags;
            ++count;
            if((headers[15].dwFlags&WHDR_DONE) || rows[count-1].us>=2000000) break;
            Sleep(1);
        } while(count<1000);
        if(!(headers[15].dwFlags&WHDR_DONE)) ++errors;
    }
    check(waveOutReset(wave));
    for(unsigned i=0;i<prepared;++i) check(waveOutUnprepareHeader(wave,&headers[i],sizeof(headers[i])));
    wsprintfA(text,"RUN=%u RATE=48000 TOTAL_SAMPLES=19200 ROWS=%u\r\n",run,count);line(text);
    for(unsigned i=0;i<count;++i) {
        wsprintfA(text,"US=%lu SAMPLES=%lu FLAGS=%lu\r\n",rows[i].us,rows[i].samples,rows[i].flags);line(text);
    }
}
void WINAPI RPEntry(void)
{
    char path[MAX_PATH], *last=path, text[160]; HWAVEOUT wave=NULL;
    DWORD length=GetModuleFileNameA(NULL,path,MAX_PATH);
    if(!length || length>=MAX_PATH-16) ExitProcess(2);
    for(char *p=path;*p;++p) if(*p=='\\') last=p+1;
    *last=0; lstrcatA(path,"POSTEST.TXT");
    report=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
    if(report==INVALID_HANDLE_VALUE) ExitProcess(3);
    LARGE_INTEGER frequency;
    if(!QueryPerformanceFrequency(&frequency) || !frequency.QuadPart) ExitProcess(4);
    for(unsigned i=0;i<19200;++i) pcm[i]=(short)((i%100<50)?3000:-3000);
    line("Scope: actual waveOut position after paused queue startup; diagnostic only.\r\n");
    MMRESULT period=timeBeginPeriod(5);
    wsprintfA(text,"PERIOD_MS=5 RESULT=%u\r\n",period);line(text);
    WAVEFORMATEX format={WAVE_FORMAT_PCM,1,48000,96000,2,16,0};
    if(check(waveOutOpen(&wave,WAVE_MAPPER,&format,0,0,CALLBACK_NULL))) {
        measure(wave,0,frequency.QuadPart);
        Sleep(100);
        measure(wave,1,frequency.QuadPart);
        check(waveOutClose(wave));
    }
    if(period==TIMERR_NOERROR) timeEndPeriod(5);
    wsprintfA(text,"API_ERRORS=%u\r\n",errors);line(text);CloseHandle(report);
    ExitProcess(errors?1:0);
}
