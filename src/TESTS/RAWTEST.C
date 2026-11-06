/* Isolate the guest wave driver from LibVLC clock correction and resampling. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
static char folder[MAX_PATH],path[MAX_PATH];
#include "SOUND.H"
void WINAPI RPEntry(void)
{
 char *last=folder,text[256];DWORD n,w,total=0,begin;unsigned errors=0;
 HANDLE report,file;HWAVEOUT out=NULL;MMTIME position;WAVEHDR header[20];
 WAVEFORMATEX format={WAVE_FORMAT_PCM,1,22050,44100,2,16,0};
 GetModuleFileNameA(NULL,folder,MAX_PATH);
 for(char *p=folder;*p;++p)if(*p=='\\')last=p+1;
 *last=0;
 if(!make_wave())ExitProcess(2);
 file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
 char *pcm=HeapAlloc(GetProcessHeap(),0,441000);
 if(file==INVALID_HANDLE_VALUE || !pcm)ExitProcess(3);
 SetFilePointer(file,44,NULL,FILE_BEGIN);
 if(!ReadFile(file,pcm,441000,&n,NULL)||n!=441000)ExitProcess(4);
 CloseHandle(file);
 lstrcpyA(path,folder);lstrcatA(path,"RAWTEST.TXT");
 report=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
 if(report==INVALID_HANDLE_VALUE)ExitProcess(5);
 MMRESULT rc=waveOutOpen(&out,WAVE_MAPPER,&format,0,0,CALLBACK_NULL);
 wsprintfA(text,"OPEN=%u\r\n",rc);WriteFile(report,text,lstrlenA(text),&w,NULL);
 if(rc)errors++;
 else {
  DWORD vol=0;UINT device=0;MMRESULT gr=waveOutGetVolume(out,&vol),ir=waveOutGetID(out,&device);
  wsprintfA(text,"GET_HANDLE=%u VOLUME=%08lx GET_ID=%u DEVICE=%u\r\n",gr,vol,ir,device);
  WriteFile(report,text,lstrlenA(text),&w,NULL);
  gr=waveOutGetVolume((HWAVEOUT)(UINT_PTR)device,&vol);
  wsprintfA(text,"GET_DEVICE=%u VOLUME=%08lx\r\n",gr,vol);WriteFile(report,text,lstrlenA(text),&w,NULL);
  gr=waveOutSetVolume(out,0xffffffff);
  wsprintfA(text,"SET_HANDLE=%u\r\n",gr);WriteFile(report,text,lstrlenA(text),&w,NULL);
  gr=waveOutSetVolume((HWAVEOUT)(UINT_PTR)device,0xffffffff);
  wsprintfA(text,"SET_DEVICE=%u\r\n",gr);WriteFile(report,text,lstrlenA(text),&w,NULL);
  ZeroMemory(header,sizeof(header));
  for(unsigned i=0;i<20;++i){header[i].lpData=pcm+22050*i;header[i].dwBufferLength=22050;
   rc=waveOutPrepareHeader(out,&header[i],sizeof(header[i]));if(rc)break;}
  if(rc)errors++;
  else {
   begin=GetTickCount();
   for(unsigned i=0;i<20;++i){rc=waveOutWrite(out,&header[i],sizeof(header[i]));if(rc)break;}
   if(rc)errors++;
   else do {
    Sleep(100);position.wType=TIME_SAMPLES;
    rc=waveOutGetPosition(out,&position,sizeof(position));
    if(rc||position.wType!=TIME_SAMPLES){errors++;break;}
    if(position.u.sample<total)errors++;
    total=position.u.sample;
    wsprintfA(text,"MS=%lu SAMPLES=%lu\r\n",GetTickCount()-begin,total);
    WriteFile(report,text,lstrlenA(text),&w,NULL);
   } while(!(header[19].dwFlags&WHDR_DONE)&&GetTickCount()-begin<20000);
   if(!(header[19].dwFlags&WHDR_DONE)||total!=220500)errors++;
   waveOutReset(out);for(unsigned i=0;i<20;++i)waveOutUnprepareHeader(out,&header[i],sizeof(header[i]));
  }
  if(waveOutClose(out))errors++;
 }
 HeapFree(GetProcessHeap(),0,pcm);
 wsprintfA(text,"ERRORS=%u\r\nRESULT=%s\r\n",errors,errors?"FAIL":"PASS");
 WriteFile(report,text,lstrlenA(text),&w,NULL);CloseHandle(report);ExitProcess(errors?1:0);
}
