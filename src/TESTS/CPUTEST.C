/* Verify CPUID/OS gating in the separately assembled FFmpeg profile. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <libavutil/cpu.h>
static unsigned cpuid_supported(void)
{
 unsigned before,after;
 __asm__ volatile("pushfl; popl %0; movl %0,%1; xorl $0x200000,%1; "
                  "pushl %1; popfl; pushfl; popl %1; pushl %0; popfl"
                  : "=&r"(before),"=&r"(after) : : "cc","memory");
 return !!((before^after)&0x200000);
}
int main(void)
{
 char path[MAX_PATH],*last=path;unsigned caps=0,max=0,b,c,d,checks=0,failures=0;
 DWORD length=GetModuleFileNameA(NULL,path,MAX_PATH);
 if(!length || length>=MAX_PATH-16) return 2;
 for(char *p=path;*p;++p) if(*p=='\\') last=p+1;
 lstrcpyA(last,"CPUTEST.TXT");FILE *out=fopen(path,"wb");if(!out)return 3;
 unsigned supported=cpuid_supported();
 if(supported) {
  __asm__ volatile("cpuid" : "=a"(max),"=b"(b),"=c"(c),"=d"(d) : "a"(0),"c"(0));
  if(max>=1) __asm__ volatile("cpuid" : "=a"(b),"=b"(c),"=c"(d),"=d"(caps) : "a"(1),"c"(0));
 }
 typedef BOOL(WINAPI *query_t)(DWORD);
 query_t query=(query_t)GetProcAddress(GetModuleHandleA("KERNEL32.DLL"),"IsProcessorFeaturePresent");
 unsigned xmm=query && query(6);
 int flags=av_get_cpu_flags();
 int scalar=AV_CPU_FLAG_MMX|AV_CPU_FLAG_MMXEXT|AV_CPU_FLAG_3DNOW|AV_CPU_FLAG_3DNOWEXT|
            AV_CPU_FLAG_CMOV|AV_CPU_FLAG_BMI1|AV_CPU_FLAG_BMI2;
#define CHECK(e) do { ++checks;if(!(e))++failures; } while(0)
 CHECK(supported || flags==0);
 CHECK(xmm || (flags&~scalar)==0);
 CHECK(!!(caps&(1U<<23))==!!(flags&AV_CPU_FLAG_MMX));
 CHECK(!(flags&AV_CPU_FLAG_SSE) || (xmm && (caps&(1U<<25))));
 CHECK(!(flags&AV_CPU_FLAG_SSE2) || (xmm && (caps&(1U<<26))));
 fprintf(out,"CPUID=%u LEAF1_EDX=%08x OS_XMM=%u FFMPEG_FLAGS=%08x\r\n",supported,caps,xmm,flags);
 fprintf(out,"CHECKS=%u FAILURES=%u\r\nRESULT=%s\r\n",checks,failures,failures?"FAIL":"PASS");
 fclose(out);return failures?1:0;
}
