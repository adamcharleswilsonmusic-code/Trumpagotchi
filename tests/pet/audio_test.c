/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "omni.h"
static int32_t buffer[512];
static const int whites[16]={0,2,4,6,7,9,11,12,14,16,18,19,21,23,24,26};
static const double hz[16]={130.8128,146.8324,164.8138,195.9977,220,261.6256,293.6648,329.6276,391.9954,440,523.2511,587.3295,659.2551,783.9909,880,1046.5023};
static double energy(int frames) {
    double e=0;
    while(frames) {
        int n=frames>256?256:frames;om_render(buffer,(uint32_t)n,4096);
        for(int i=0;i<n*2;i++){assert(buffer[i]>-8388607 && buffer[i]<8388607);double x=buffer[i];e+=x*x;}
        frames-=n;
    }
    return e;
}
static void dry(void){om_init();om_set(P_REVERB,0);om_set(P_DELAY,0);energy(4410);}
static uint32_t keyboard_print(int legacy_effects) {
    uint32_t hash=2166136261u;om_init();
    om_set(P_REVERB,legacy_effects?100:0);om_set(P_DELAY,legacy_effects?100:0);
    om_set(P_TIME,legacy_effects?500:80);om_set(P_FEEDBACK,legacy_effects?70:0);
    om_trigger(0,127);
    for(int block=0;block<180;block++){
        if(block==16)om_note_off(0);
        om_render(buffer,256,4096);
        for(int i=0;i<512;i++)hash=(hash^(uint32_t)buffer[i])*16777619u;
        if(block>40)for(int i=0;i<512;i++)assert(buffer[i]==0);
    }
    assert(!om_active_keys);return hash;
}
static int is_white(int key){for(int i=0;i<16;i++)if(whites[i]==key)return 1;return 0;}
static int countbits(uint32_t n){int r=0;while(n){r+=(int)(n&1u);n>>=1;}return r;}
static void release_all(void){for(int i=0;i<27;i++)om_note_off(i);}
int main(void) {
    om_init();assert(energy(44100*2)==0);om_bongo_enable(1);assert(energy(44100*3)==0 && !om_bongo_hits);
    for(int k=0;k<27;k++) {
        dry();om_bongo_enable(1);om_trigger(k,127);
        if(!is_white(k)){assert(energy(44100)==0);assert(!om_triggers && !om_bongo_hits);continue;}
        assert(energy(2048)>1e6);assert(om_triggers==1 && om_active_keys==(1u<<k));
        om_note_off(k);energy(4410);assert(!om_active_keys);energy(44100*3);assert(energy(44100)==0 && !om_bongo_active);
    }
    /* Measure actual rendered fundamentals, not only the note table. */
    for(int k=0;k<16;k++) {
        int crossings=0;int32_t previous=0;dry();om_trigger(whites[k],127);energy(4410);
        for(int f=0;f<22050;f++) {om_render(buffer,1,4096);if(buffer[0]>0 && previous<=0)crossings++;previous=buffer[0];}
        assert(crossings*2>hz[k]*0.98 && crossings*2<hz[k]*1.02);
    }
    dry();om_trigger(-1,127);om_trigger(27,127);om_trigger(0,0);assert(energy(1024)==0 && !om_triggers);
    dry();om_trigger(0,127);double loud=energy(4096);dry();om_trigger(0,63);double quiet=energy(4096);
    assert(quiet>loud*.24 && quiet<loud*.30);
    dry();for(int k=0;k<16;k++)om_trigger(whites[k],127);assert(energy(4096)>1e6);
    assert(om_triggers==16 && countbits(om_active_keys)==FMO_VOICES);release_all();energy(4410);assert(!om_active_keys);
    /* A queued note released before rendering must never resurrect. */
    dry();om_trigger(0,127);om_note_off(0);assert(energy(1024)==0);
    om_trigger(0,127);assert(energy(2048)>1e6);om_note_off(0);energy(4410);assert(!om_active_keys);
    /* Saturated queue cannot lose note-off, even with rapid retriggering. */
    dry();om_trigger(0,127);energy(512);
    for(int i=0;i<300;i++)om_set(P_REVERB,0);
    om_note_off(0);assert(om_dropped>0);energy(4410);assert(!om_active_keys && energy(512)==0);
    for(int round=0;round<200;round++) {
        for(int i=0;i<200;i++){int key=whites[(i+round)%16];om_trigger(key,127);if(i%3)om_note_off(key);}
        release_all();energy(4410);assert(!om_active_keys);
    }
    dry();om_bongo_enable(1);om_trigger(0,100);energy(44100);assert(om_bongo_hits>=3);
    uint32_t hits=om_bongo_hits;om_note_off(0);
    assert(energy(52920)>1e6 && om_bongo_hits>hits && om_bongo_active); /* 1.2 s musical gap */
    om_trigger(2,100);energy(4410);om_note_off(2); /* next note extends the finish */
    assert(energy(88200)>1e6 && om_bongo_active);
    energy(48510);uint32_t stopped=om_bongo_hits;
    assert(stopped>hits);assert(energy(44100)==0 && om_bongo_hits==stopped && !om_bongo_active);
    /* Eligibility alone, legacy seq controls, and black keys cannot restart. */
    om_bongo_enable(1);om_seq_toggle_play();om_seq_toggle_record();om_seq_set_tempo(240);
    for(int i=0;i<27;i++)if(!is_white(i))om_trigger(i,127);
    assert(energy(44100)==0 && om_bongo_hits==stopped && !om_seq_playing && !om_seq_recording);
    assert(!om_take_seq_hit(0));
    /* Disable and re-enable while held cannot spontaneously rearm. */
    om_trigger(0,127);energy(1024);om_bongo_enable(0);om_bongo_enable(1);hits=om_bongo_hits;
    energy(44100);assert(om_bongo_hits==hits);om_note_off(0);energy(44100);assert(energy(1024)==0);
    om_trigger(2,127);energy(1024);assert(om_bongo_hits>hits);om_bongo_enable(0);hits=om_bongo_hits;
    energy(44100);assert(om_bongo_hits==hits);
    /* Old saved effects cannot color notes or leave tails; panic still clears audio. */
    assert(keyboard_print(0)==keyboard_print(1));
    dry();om_set(P_DELAY,100);om_set(P_TIME,80);om_set(P_REVERB,100);om_bongo_enable(1);
    energy(4410);om_trigger(0,127);energy(2205);om_note_off(0);assert(energy(22050)>1e6);
    om_panic();assert(energy(44100)==0);hits=om_bongo_hits;assert(energy(44100)==0 && om_bongo_hits==hits);
    dry();om_trigger(0,127);om_render(buffer,256,0);for(int i=0;i<512;i++)assert(!buffer[i]);
    puts("pet audio: silent boot, 16 pentatonic pitches, black-key rejection, velocity, 8 voices, release/overflow stress, conditional bongos, dry keyboard despite legacy settings, panic, mute PASS");
    return 0;
}
