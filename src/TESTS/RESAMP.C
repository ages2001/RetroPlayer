/* Exercise the actual VLC nearest-neighbor resampler with long PCM buffers.
 * Sample positions are checked against an independent absolute-index oracle. */
#include "config.h"
#include <windows.h>
#include <stdio.h>
#include <vlc_common.h>
#include <vlc_block.h>
#include <vlc_aout.h>
static __typeof__(&block_Alloc) rp_allocate;
static __typeof__(&aout_BitsPerSample) rp_bits;
#define block_Alloc rp_allocate
#define aout_BitsPerSample rp_bits
#ifdef RP_RESAMP_DIV
#include "../SOURCES/RSDIV.C"
#elif defined(RP_RESAMP_BASE)
#include "../SOURCES/UGLYBASE.C"
#elif defined(RP_RESAMP_OLD)
#include "../../VLCREF/modules/audio_filter/resampler/ugly.c"
#else
#include "../../VLCWORK/modules/audio_filter/resampler/ugly.c"
#endif
#undef block_Alloc
#undef aout_BitsPerSample
static FILE *report;
static unsigned checks,failures;
static DWORD filter_ms;
#define CHECK(x) do{++checks;if(!(x)){++failures;fprintf(report,"FAIL LINE=%u\r\n",__LINE__);}}while(0)
static void cleanup(filter_t *filter)
{
#if !defined(RP_RESAMP_OLD) && !defined(RP_RESAMP_BASE)
    Close(VLC_OBJECT(filter));
#else
    (void)filter;
#endif
}
static void format(filter_t *filter,unsigned input,unsigned output)
{
    memset(filter,0,sizeof(*filter));
    filter->fmt_in.audio.i_format=filter->fmt_out.audio.i_format=VLC_CODEC_S16N;
    filter->fmt_in.audio.i_rate=input;filter->fmt_out.audio.i_rate=output;
    filter->fmt_in.audio.i_channels=filter->fmt_out.audio.i_channels=2;
    filter->fmt_in.audio.i_physical_channels=filter->fmt_out.audio.i_physical_channels=AOUT_CHANS_STEREO;
    filter->fmt_in.audio.i_bitspersample=filter->fmt_out.audio.i_bitspersample=16;
}
static int16_t sample(unsigned frame,unsigned channel)
{
    return (frame*17+channel*31)&0x7fff;
}
static block_t *timed(filter_t *filter,block_t *block)
{
    DWORD start=GetTickCount();
    block=DoWork(filter,block);
    filter_ms+=GetTickCount()-start;
    return block;
}
static void convert(unsigned input,unsigned output,unsigned frames)
{
    filter_t filter;format(&filter,input,output);
    CHECK(CreateResampler(VLC_OBJECT(&filter))==VLC_SUCCESS);
    block_t *block=rp_allocate((size_t)frames*4);CHECK(block!=NULL);if(!block)return;
    int16_t *pcm=(int16_t *)block->p_buffer;
    for(unsigned i=0;i<frames;++i)for(unsigned c=0;c<2;++c)pcm[i*2+c]=sample(i,c);
    block->i_nb_samples=frames;block->i_pts=1234567;
    block=timed(&filter,block);CHECK(block!=NULL);if(!block)return;
    unsigned expected=(uint64_t)frames*output/input;
    CHECK(block->i_nb_samples==expected);CHECK(block->i_buffer==(size_t)expected*4);
    CHECK(block->i_pts==1234567);
    CHECK(block->i_length==(int64_t)expected*1000000/output);
    BOOL match=block->i_nb_samples==expected;
    pcm=(int16_t *)block->p_buffer;
    for(unsigned i=0;match&&i<expected;++i)for(unsigned c=0;c<2;++c)
        if(pcm[i*2+c]!=sample((uint64_t)i*input/output,c))match=FALSE;
    CHECK(match);block_Release(block);cleanup(&filter);
}
static void streaming(unsigned input,unsigned output,unsigned blocks,unsigned chunk)
{
    filter_t filter;format(&filter,input,output);
    CHECK(CreateResampler(VLC_OBJECT(&filter))==VLC_SUCCESS);
    uint64_t consumed=0,produced=0;BOOL counts=TRUE,pixels=TRUE,metadata=TRUE;
    static const unsigned pattern[]={1,7,120,960,1009};
    for(unsigned b=0;b<blocks;++b) {
        unsigned n=chunk?chunk:pattern[b%5];
        block_t *block=rp_allocate((size_t)n*4);
        if(!block){CHECK(FALSE);break;}
        int16_t *pcm=(int16_t *)block->p_buffer;
        for(unsigned i=0;i<n;++i)for(unsigned c=0;c<2;++c)pcm[i*2+c]=sample((unsigned)consumed+i,c);
        block->i_nb_samples=n;block->i_pts=1234567+(int64_t)(consumed*1000000/input);
        int64_t pts=block->i_pts;consumed+=n;
        block=timed(&filter,block);
        if(block) {
            if(block->i_buffer!=(size_t)block->i_nb_samples*4 || block->i_pts!=pts ||
               block->i_length!=(int64_t)block->i_nb_samples*1000000/output)metadata=FALSE;
            pcm=(int16_t *)block->p_buffer;
            for(unsigned i=0;i<block->i_nb_samples;++i)for(unsigned c=0;c<2;++c)
                if(pcm[i*2+c]!=sample((unsigned)((produced+i)*input/output),c))pixels=FALSE;
            produced+=block->i_nb_samples;block_Release(block);
        }
        /* Samples are emitted at absolute source positions floor(j*in/out).
         * This oracle has no block-local accumulator and requires invariance
         * under arbitrary packet boundaries, including single-frame packets. */
        uint64_t expected=(consumed*output+input-1)/input;
        if(produced!=expected)counts=FALSE;
    }
    fprintf(report,"STREAM IN=%u OUT=%u BLOCKS=%u CHUNK=%u INPUT=%lu OUTPUT=%lu EXPECTED=%lu\r\n",
        input,output,blocks,chunk,(unsigned long)consumed,(unsigned long)produced,
        (unsigned long)((consumed*output+input-1)/input));
    CHECK(counts);CHECK(pixels);CHECK(metadata);cleanup(&filter);
}
static block_t *small_block(unsigned first,unsigned count)
{
    block_t *block=rp_allocate(count*4);CHECK(block!=NULL);if(!block)return NULL;
    int16_t *pcm=(int16_t *)block->p_buffer;
    for(unsigned i=0;i<count;++i)for(unsigned c=0;c<2;++c)pcm[i*2+c]=sample(first+i,c);
    block->i_nb_samples=count;block->i_pts=1234567;
    return block;
}
static void restart_tests(void)
{
    for(unsigned discontinuity=0;discontinuity<2;++discontinuity) {
        filter_t filter;format(&filter,192000,48000);
        CHECK(CreateResampler(VLC_OBJECT(&filter))==VLC_SUCCESS);
        block_t *block=small_block(0,1);
        if(block){block=DoWork(&filter,block);if(block)block_Release(block);}
        if(!discontinuity && filter.pf_flush)filter.pf_flush(&filter);
        block=small_block(200,4);
        if(block) {
            if(discontinuity)block->i_flags|=BLOCK_FLAG_DISCONTINUITY;
            block=DoWork(&filter,block);CHECK(block!=NULL);
            if(block){CHECK(block->i_nb_samples==1 && ((int16_t *)block->p_buffer)[0]==sample(200,0));block_Release(block);}
        }
        cleanup(&filter);
    }
    /* After the first 4:3 sample, the next source position is 1+1/3.
     * Doubling the output rate preserves that position, then advances 2/3
     * per output: the next source indices must be 1, 2, 2. */
    filter_t filter;format(&filter,4,3);CHECK(CreateResampler(VLC_OBJECT(&filter))==VLC_SUCCESS);
    block_t *block=small_block(0,1);
    if(block){block=DoWork(&filter,block);if(block)block_Release(block);}
    filter.fmt_out.audio.i_rate=6;
    block=small_block(1,2);
    if(block) {
        block=DoWork(&filter,block);CHECK(block!=NULL);
        if(block) {
            int16_t *pcm=(int16_t *)block->p_buffer;
            CHECK(block->i_nb_samples==3);
            CHECK(block->i_nb_samples==3 && pcm[0]==sample(1,0) && pcm[2]==sample(2,0) && pcm[4]==sample(2,0));
            block_Release(block);
        }
    }
    cleanup(&filter);
}
int main(void)
{
    char path[MAX_PATH],*last;DWORD n=GetModuleFileNameA(NULL,path,MAX_PATH);
    if(!n||n>=MAX_PATH-24)return 2;
    last=path;for(char *p=path;*p;++p)if(*p=='\\')last=p+1;*last=0;SetCurrentDirectoryA(path);
#ifdef RP_RESAMP_DIV
    report=fopen("RSDIV.TXT","wb");
#elif defined(RP_RESAMP_BASE)
    report=fopen("RESBASE.TXT","wb");
#elif defined(RP_RESAMP_OLD)
    report=fopen("RESOLD.TXT","wb");
#else
    report=fopen("RESAMP.TXT","wb");
#endif
    if(!report)return 3;
    HMODULE core=LoadLibraryA("LIBCORE.DLL");CHECK(core!=NULL);if(!core)goto done;
    rp_allocate=(__typeof__(rp_allocate))GetProcAddress(core,"block_Alloc");CHECK(rp_allocate!=NULL);
    rp_bits=(__typeof__(rp_bits))GetProcAddress(core,"aout_BitsPerSample");CHECK(rp_bits!=NULL);
    if(!rp_allocate||!rp_bits)goto done;
    convert(22050,48000,22050);convert(48000,96000,48000);
    convert(192000,96000,192000);convert(96000,44100,96000);
    convert(11025,44100,11025);convert(48000,47990,48000);
    streaming(48002,48000,2000,120);streaming(47998,48000,2000,120);
    streaming(22052,22050,200,1024);streaming(44100,48000,1000,7);
    streaming(192000,48000,2000,1);streaming(48000,192000,2000,1);
    streaming(48002,48000,500,0);
    restart_tests();
    filter_t filter;format(&filter,0,44100);CHECK(CreateResampler(VLC_OBJECT(&filter))==VLC_EGENERIC);
    format(&filter,44100,0);CHECK(CreateResampler(VLC_OBJECT(&filter))==VLC_EGENERIC);
    format(&filter,1,2);
    CHECK(CreateResampler(VLC_OBJECT(&filter))==VLC_SUCCESS);
    block_t *block=rp_allocate(4);CHECK(block!=NULL);
    if(block){block->i_nb_samples=UINT_MAX;CHECK(DoWork(&filter,block)==NULL);}
    cleanup(&filter);
done:
    if(core)FreeLibrary(core);
    fprintf(report,"FILTER_ELAPSED_MS=%lu (coarse accumulated clock; diagnostic only)\r\n",(unsigned long)filter_ms);
    fprintf(report,"CHECKS=%u FAILURES=%u\r\nRESULT=%s\r\n",checks,failures,failures?"FAIL":"PASS");
    fclose(report);return failures?1:0;
}
