/* Production clock calibration with deterministic API/counter boundaries. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define WINAPI
#define CLOCK_FREQ 1000000LL
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
typedef uint32_t DWORD;
typedef void *HMODULE;
typedef int64_t vlc_tick_t;
static struct { uint64_t frequency,origin; vlc_tick_t epoch; } rp_tsc_clock;
static uint64_t now_ms,counter_hz,counter_extra;
static unsigned quantum,step,reads,fault,checks,failures;
static bool available,timer_present;
static bool rp_tsc_available(void) { return available; }
static uint64_t rp_tsc_read(void)
{
    ++reads;
    if(fault==2 && step>=3)return 1;
    return now_ms*(counter_hz/1000)+counter_extra;
}
static DWORD fake_time(void)
{
    uint64_t clock_ms=fault==1 && step>=3 ? 500 : now_ms;
    if(fault==5 && step==0)clock_ms-=14;
    return (DWORD)(clock_ms/quantum*quantum);
}
static HMODULE GetModuleHandleA(const char *name)
{ (void)name; return timer_present ? (void *)1 : NULL; }
static void *GetProcAddress(HMODULE h,const char *name)
{ (void)h; (void)name; return (void *)fake_time; }
static DWORD GetTickCount(void) { return (DWORD)now_ms; }
static void Sleep(DWORD duration)
{
    ++step;
    now_ms+=duration+(step%3);
    if(fault==3 && step==3)counter_extra+=counter_hz/10;
    if(fault==4 && step==3)now_ms+=1000;
}
#include "TCGFUN.H"
#define CHECK(x) do { ++checks; if(!(x)) { ++failures; printf("FAIL line=%d\n",__LINE__); } } while(0)
static void reset(void)
{
    now_ms=13;counter_hz=2596000000ULL;counter_extra=0;quantum=5;
    step=reads=fault=0;available=timer_present=true;
    memset(&rp_tsc_clock,0,sizeof(rp_tsc_clock));
}
int main(void)
{
    for(unsigned q=1;q<=5;++q) {
        reset();quantum=q;CHECK(rp_tsc_setup());
        uint64_t difference=rp_tsc_clock.frequency>counter_hz ?
            rp_tsc_clock.frequency-counter_hz : counter_hz-rp_tsc_clock.frequency;
        CHECK(difference<=counter_hz*3/1000);
        vlc_tick_t before=mdate_tsc();now_ms+=1000;vlc_tick_t after=mdate_tsc();
        CHECK(after-before>=997000 && after-before<=1003000);
        now_ms+=31536000000ULL; /* One year: exercise split conversion. */
        uint64_t elapsed=rp_tsc_read()-rp_tsc_clock.origin;
        vlc_tick_t expected=rp_tsc_clock.epoch+(vlc_tick_t)(
            (__uint128_t)elapsed*1000000/rp_tsc_clock.frequency);
        CHECK(mdate_tsc()==expected);
    }
    reset();now_ms=0xffffff00ULL;CHECK(rp_tsc_setup());
    reset();now_ms=1013;fault=5;CHECK(rp_tsc_setup());
    for(unsigned f=1;f<=4;++f) { reset();fault=f;CHECK(!rp_tsc_setup()); }
    reset();available=false;CHECK(!rp_tsc_setup());CHECK(reads==0);
    reset();timer_present=false;CHECK(!rp_tsc_setup());CHECK(reads==0);
    reset();counter_hz=100000;CHECK(!rp_tsc_setup());
    /* Stay beyond the timer-quantization uncertainty at the upper bound. */
    reset();counter_hz=101000000000ULL;CHECK(!rp_tsc_setup());
    printf("CHECKS=%u FAILURES=%u RESULT=%s\n",checks,failures,failures?"FAIL":"PASS");
    return failures!=0;
}
