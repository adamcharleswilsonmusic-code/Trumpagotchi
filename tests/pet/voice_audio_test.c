/* Recorded reactions: real bank, exact duration, one speaker, mute and loss-proof stop. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "omni.h"
static int32_t out[512];
static double render(unsigned n, unsigned gain) {
    double energy=0;
    while(n){unsigned k=n>256?256:n;om_render(out,k,gain);
        for(unsigned i=0;i<k*2;i++){
            assert(out[i]>-8388607 && out[i]<8388607);
            double x=out[i];energy+=x*x;
        }n-=k;
    }return energy;
}
int main(void) {
    for(int c=0;c<PV_COUNT;c++){
        om_init();assert(!om_voice_busy());assert(om_voice_play(c));
        assert(om_voice_busy() && !om_voice_play((c+1)%PV_COUNT));
        unsigned duration=(om_voice_frames(c)*44100u+15999u)/16000u;
        assert(render(duration-1,4096)>1e6);
        assert(om_voice_active==(unsigned)c+1 && om_voice_starts==1 && !om_voice_finished);
        render(1,4096);assert(!om_voice_busy() && om_voice_finished==1);
        assert(render(1024,4096)==0); /* recorded speech leaves no FX tail */
    }
    om_init();assert(!om_voice_play(-1) && !om_voice_play(PV_COUNT));
    for(int i=0;i<128;i++)om_set(P_REVERB,0);
    assert(!om_voice_play(PV_POO) && !om_voice_busy());render(256,4096);
    assert(om_voice_play(PV_POO));om_voice_stop();
    assert(om_voice_play(PV_PERFECT));render(256,4096);
    assert(om_voice_active==PV_PERFECT+1 && om_voice_starts==1);
    for(int i=0;i<300;i++)om_set(P_DELAY,0);
    om_voice_stop();assert(render(512,4096)==0 && !om_voice_busy());
    assert(om_voice_play(PV_SNORE));render(256,0);
    for(int i=0;i<512;i++)assert(!out[i]);
    om_panic();assert(render(1024,4096)==0 && !om_voice_busy());
    assert(om_voice_play(PV_PRAISE_1));om_trigger(0,127);
    assert(render(4410,4096)>1e6 && om_active_keys);
    om_init();om_footstep(1,0);assert(render(1200,4096)>0 && om_foot_hits==1);
    assert(render(4410,4096)==0 && om_foot_hits==1); /* no autonomous repeating beat */
    om_footstep(1,1);assert(render(1200,4096)>0 && om_foot_hits==2);
    om_footstep(0,1);assert(render(256,4096)==0 && !om_foot_busy());
    om_init();om_panic();om_game_over();assert(om_game_over_busy());
    assert(render(44100,4096)>1e6 && !om_game_over_busy() && om_game_over_starts==1);
    assert(render(44100,4096)==0 && om_game_over_starts==1);
    om_game_over();assert(render(256,0)==0 && om_game_over_busy());
    om_panic();assert(render(256,4096)==0 && !om_game_over_busy());
    puts("Voice DSP PASS: 12 real clips, exact duration, one speaker, overflow, cancellation, mute, panic and simultaneous keyboard.");
}
