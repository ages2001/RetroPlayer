/* Actual VLC Speex resampler: long-buffer duration and chunk continuity. */
#include "config.h"
#include <windows.h>
#include <stdio.h>
#include <vlc_common.h>
#include <vlc_block.h>
#include <vlc_aout.h>
#include <vlc_filter.h>
#include <vlc_variables.h>
static __typeof__(&block_Alloc) rp_allocate;
static __typeof__(&aout_BitsPerSample) rp_bits;
static unsigned logged_errors;
#define block_Alloc rp_allocate
#define aout_BitsPerSample rp_bits
#undef var_InheritInteger
#define var_InheritInteger(o,n) 4
#undef msg_Err
#define msg_Err(o,...) ((void)++logged_errors)
#ifdef RP_SPRES_OLD
#include "../../VLCREF/modules/audio_filter/resampler/speex.c"
#else
#include "../../VLCWORK/modules/audio_filter/resampler/speex.c"
#endif
#undef block_Alloc
#undef aout_BitsPerSample
static FILE *report;
static unsigned checks,failures;
static int16_t whole[400000];
#define CHECK(x) do{++checks;if(!(x)){++failures;fprintf(report,"FAIL LINE=%u\r\n",__LINE__);}}while(0)
static void format(filter_t *f,unsigned input,unsigned output)
{
 memset(f,0,sizeof(*f));
 f->fmt_in.audio.i_format=f->fmt_out.audio.i_format=VLC_CODEC_S16N;
 f->fmt_in.audio.i_rate=input;f->fmt_out.audio.i_rate=output;
 f->fmt_in.audio.i_channels=f->fmt_out.audio.i_channels=2;
 f->fmt_in.audio.i_physical_channels=f->fmt_out.audio.i_physical_channels=AOUT_CHANS_STEREO;
 f->fmt_in.audio.i_bitspersample=f->fmt_out.audio.i_bitspersample=16;
 f->fmt_in.audio.i_bytes_per_frame=f->fmt_out.audio.i_bytes_per_frame=4;
}
static int16_t value(unsigned frame,unsigned channel){return (int16_t)((frame*37+channel*7919)&0x7fff);}
static block_t *input(unsigned begin,unsigned frames)
{
 block_t *b=rp_allocate((size_t)frames*4);CHECK(b!=NULL);if(!b)return NULL;
 int16_t *p=(int16_t*)b->p_buffer;
 for(unsigned i=0;i<frames;++i)for(unsigned c=0;c<2;++c)p[2*i+c]=value(begin+i,c);
 b->i_nb_samples=frames;b->i_pts=1234567;return b;
}
static void convert(unsigned irate,unsigned orate,unsigned frames)
{
 filter_t f;format(&f,irate,orate);CHECK(OpenResampler(VLC_OBJECT(&f))==VLC_SUCCESS);if(!f.p_sys)return;
 block_t *b=input(0,frames);if(!b){Close(VLC_OBJECT(&f));return;}
 b=Resample(&f,b);CHECK(b!=NULL);if(!b){Close(VLC_OBJECT(&f));return;}
 unsigned count=b->i_nb_samples;CHECK(count==(uint64_t)frames*orate/irate);
 CHECK(b->i_buffer==(size_t)count*4);CHECK(b->i_pts==1234567);
 CHECK(b->i_length==(int64_t)count*1000000/orate);CHECK(count<=200000);
 if(count>200000){block_Release(b);Close(VLC_OBJECT(&f));return;}
 memcpy(whole,b->p_buffer,b->i_buffer);block_Release(b);Close(VLC_OBJECT(&f));
 format(&f,irate,orate);CHECK(OpenResampler(VLC_OBJECT(&f))==VLC_SUCCESS);if(!f.p_sys)return;
 unsigned consumed=0,produced=0,part=0;BOOL same=TRUE;const unsigned sizes[]={1,17,103,509,2048,7};
 while(consumed<frames){
  unsigned n=sizes[part++%6];if(n>frames-consumed)n=frames-consumed;
  b=input(consumed,n);if(!b)break;b=Resample(&f,b);CHECK(b!=NULL);if(!b)break;
  if(produced+b->i_nb_samples>count)same=FALSE;
  else if(memcmp(whole+produced*2,b->p_buffer,b->i_buffer))same=FALSE;
  if(b->i_length!=(int64_t)b->i_nb_samples*1000000/orate)same=FALSE;
  produced+=b->i_nb_samples;consumed+=n;block_Release(b);
 }
 CHECK(consumed==frames);CHECK(produced==count);CHECK(same);Close(VLC_OBJECT(&f));
 fprintf(report,"RATE=%u TO=%u INPUT=%u OUTPUT=%u CHUNKED=%u\r\n",irate,orate,frames,count,produced);
}
int main(void)
{
 char path[MAX_PATH],*last;DWORD n=GetModuleFileNameA(NULL,path,MAX_PATH);
 if(!n||n>=MAX_PATH-24)return 2;
 last=path;for(char *p=path;*p;++p)if(*p=='\\')last=p+1;*last=0;SetCurrentDirectoryA(path);
#ifdef RP_SPRES_OLD
 report=fopen("SROLD.TXT","wb");
#else
 report=fopen("SPRES.TXT","wb");
#endif
 if(!report)return 3;
 HMODULE core=LoadLibraryA("LIBCORE.DLL");CHECK(core!=NULL);if(!core)goto done;
 rp_allocate=(__typeof__(rp_allocate))GetProcAddress(core,"block_Alloc");CHECK(rp_allocate!=NULL);
 rp_bits=(__typeof__(rp_bits))GetProcAddress(core,"aout_BitsPerSample");CHECK(rp_bits!=NULL);
 if(!rp_allocate||!rp_bits)goto done;
 convert(22050,48000,22050);convert(48000,96000,48000);convert(192000,96000,192000);
 convert(96000,44100,96000);convert(11025,44100,11025);convert(48000,47990,48000);
#ifndef RP_SPRES_OLD
 filter_t f;format(&f,0,44100);CHECK(OpenResampler(VLC_OBJECT(&f))==VLC_EGENERIC);
 format(&f,44100,0);CHECK(OpenResampler(VLC_OBJECT(&f))==VLC_EGENERIC);
 format(&f,44100,48000);CHECK(OpenResampler(VLC_OBJECT(&f))==VLC_SUCCESS);
 if(f.p_sys){f.fmt_out.audio.i_bytes_per_frame=0;block_t *b=input(0,1);if(b)CHECK(Resample(&f,b)==NULL);Close(VLC_OBJECT(&f));}
#endif
 CHECK(logged_errors==0);
done:
 if(core)FreeLibrary(core);
 fprintf(report,"CHECKS=%u FAILURES=%u\r\nRESULT=%s\r\n",checks,failures,failures?"FAIL":"PASS");fclose(report);return failures?1:0;
}
