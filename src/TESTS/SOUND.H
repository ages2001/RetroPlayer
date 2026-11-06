/* Shared deterministic PCM fixture for independent driver and LibVLC tests. */
#ifndef RP_PREROLL
#define RP_PREROLL 0
#endif
static void put32(unsigned char *p,DWORD value)
{
 for(unsigned i=0;i<4;++i)p[i]=(unsigned char)(value>>(8*i));
}
static BOOL make_wave(void)
{
 unsigned char header[44]={'R','I','F','F',0,0,0,0,'W','A','V','E',
  'f','m','t',' ',16,0,0,0,1,0,1,0,0,0,0,0,0,0,0,0,2,0,16,0,'d','a','t','a',0,0,0,0};
 unsigned char block[4410];DWORD written;
 /* Ten seconds after the optional clock-settling preroll: silence, 441 Hz, silence, 735 Hz, silence, 315 Hz,
  * silence. Integer triangle waves make the source independently repeatable. */
 put32(header+4,(10+RP_PREROLL)*44100+36);put32(header+24,22050);put32(header+28,44100);put32(header+40,(10+RP_PREROLL)*44100);
 lstrcpyA(path,folder);lstrcatA(path,"SNDTEST.WAV");
 HANDLE file=CreateFileA(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);
 if(file==INVALID_HANDLE_VALUE)return FALSE;
 BOOL ok=WriteFile(file,header,44,&written,NULL)&&written==44;
 for(unsigned base=0;ok && base<(10+RP_PREROLL)*22050;base+=2205) {
  for(unsigned i=0;i<2205;++i) {
   unsigned sample=base+i,period=0;
#if RP_PREROLL
   if(sample>=RP_PREROLL*22050)sample-=RP_PREROLL*22050;else sample=0;
#endif
   if(sample>=22050 && sample<66150)period=50;
   else if(sample>=88200 && sample<132300)period=30;
   else if(sample>=154350 && sample<198450)period=70;
   int value=0;
   if(period){unsigned phase=sample%period;value=(phase<period/2?(int)phase:(int)(period-phase))*24000/(int)period-6000;}
   block[2*i]=(unsigned char)value;block[2*i+1]=(unsigned char)((unsigned short)value>>8);
  }
  ok=WriteFile(file,block,sizeof(block),&written,NULL)&&written==sizeof(block);
 }
 return CloseHandle(file)&&ok;
}
