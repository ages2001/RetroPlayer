/* Exercise the actual core controller with deterministic device timing.
 * SYNCTEST.PY supplies the extracted production functions in SYNCFUN.H. */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#define RP_LEGACY 1
#define CLOCK_FREQ 1000000LL
#define INPUT_RATE_DEFAULT 1000
#define AOUT_MAX_PTS_ADVANCE (CLOCK_FREQ / 25)
#define AOUT_MAX_PTS_DELAY (3 * CLOCK_FREQ / 50)
#define VLC_TICK_INVALID 0
#define AOUT_RESAMPLING_NONE 0
#define AOUT_RESAMPLING_UP 1
#define AOUT_RESAMPLING_DOWN 2
#define __MIN(a,b) ((a)<(b)?(a):(b))
#define __MAX(a,b) ((a)>(b)?(a):(b))
#define msg_Dbg(...) ((void)0)
#define msg_Warn(...) ((void)0)
typedef int64_t vlc_tick_t;
typedef struct {
    struct {
        vlc_tick_t end, resamp_duration;
        unsigned resamp_start_drift;
        int resamp_type;
        bool discontinuity;
    } sync;
    void *filters;
} aout_owner_t;
typedef struct { aout_owner_t owner; } audio_output_t;
static vlc_tick_t device_drift;
static int rate_offset, checks, failures, unexpected_outputs;
static bool unknown_time;
static aout_owner_t *aout_owner(audio_output_t *a) { return &a->owner; }
static vlc_tick_t mdate(void) { return 1000000; }
static int aout_OutputTimeGet(audio_output_t *a, vlc_tick_t *delay)
{ (void)a; *delay=device_drift; return unknown_time ? -1 : 0; }
static void aout_OutputFlush(audio_output_t *a, bool wait)
{ (void)a; (void)wait; ++unexpected_outputs; }
static void aout_DecSilence(audio_output_t *a, vlc_tick_t n, vlc_tick_t pts)
{ (void)a; (void)n; (void)pts; ++unexpected_outputs; }
static bool aout_FiltersCanResample(void *f) { (void)f; return true; }
static bool aout_FiltersAdjustResampling(void *f, int step)
{ (void)f; rate_offset=step ? rate_offset+step : 0; return rate_offset!=0; }
#include "SYNCFUN.H"
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; printf("FAIL line=%d\n",__LINE__); } } while(0)
static void advance(audio_output_t *a, vlc_tick_t duration)
{
#ifdef RP_BASELINE
    (void)duration;
    aout_DecSynchronize(a,1000000,INPUT_RATE_DEFAULT);
#else
    aout_DecSynchronize(a,1000000,INPUT_RATE_DEFAULT,duration);
#endif
}
static void reset(audio_output_t *a)
{ memset(a,0,sizeof(*a)); rate_offset=0; unknown_time=false; }
int main(void)
{
    audio_output_t a;
    const int packets[]={2500,20000,60000};
    for (unsigned p=0;p<sizeof(packets)/sizeof(packets[0]);++p) {
        for (int sign=-1;sign<=1;sign+=2) {
            reset(&a); device_drift=sign*80000;
            int left=1000000;
            while (left) { int n=__MIN(left,packets[p]);advance(&a,n);left-=n; }
            printf("packet_us=%d drift_sign=%d one_second_rate_hz=%d\n",packets[p],sign,rate_offset);
            CHECK(rate_offset==sign*100);
            /* Recovery must reach zero without crossing to the other sign. */
            device_drift=0;
            for (int i=0;i<500 && a.owner.sync.resamp_type!=AOUT_RESAMPLING_NONE;++i) {
                advance(&a,packets[(p+1)%3]); CHECK(rate_offset*sign>=0);
            }
            CHECK(rate_offset==0); CHECK(a.owner.sync.resamp_type==AOUT_RESAMPLING_NONE);
        }
    }
    reset(&a); device_drift=0;
    for (int i=0;i<400;++i) advance(&a,2500);
    CHECK(rate_offset==0);
    device_drift=80000; unknown_time=true;
    for (int i=0;i<400;++i) advance(&a,2500);
    CHECK(rate_offset==0);
    unknown_time=false; advance(&a,2500); aout_StopResampling(&a);
    CHECK(rate_offset==0); CHECK(a.owner.sync.resamp_duration==0);
    CHECK(unexpected_outputs==0);
    printf("CHECKS=%d FAILURES=%d RESULT=%s\n",checks,failures,failures?"FAIL":"PASS");
    return failures!=0;
}
