/* SPDX-License-Identifier: GPL-3.0-only */
/* Host-only audition: synthesized prototype, no voice recording. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "omni.h"
static int32_t frames[512];
static FILE *wav;
static void u16(uint32_t x){fputc((int)(x&255u),wav);fputc((int)((x>>8)&255u),wav);}
static void u32(uint32_t x){u16(x);u16(x>>16);}
static void render(uint32_t n) {
    while(n){uint32_t m=n>256?256:n;om_render(frames,m,4096);
        for(uint32_t i=0;i<2*m;i++)u16((uint16_t)(int16_t)(frames[i]/256));n-=m;}
}
int main(int argc,char **argv) {
    const int melody[12]={0,4,7,9,12,9,7,4,2,6,9,12};
    uint32_t total=44100u*13u;
    if(argc!=2)return 2;
    wav=fopen(argv[1],"wb");if(!wav)return 2;
    fwrite("RIFF",1,4,wav);u32(36+total*4);fwrite("WAVEfmt ",1,8,wav);
    u32(16);u16(1);u16(2);u32(44100);u32(44100*4);u16(4);u16(16);fwrite("data",1,4,wav);u32(total*4);
    om_init();render(22050); /* Silent startup. */
    om_bongo_enable(1);
    for(int i=0;i<12;i++){om_trigger(melody[i],105);render(11025);om_note_off(melody[i]);render(2205);}
    render(145530); /* Hear the full three-second accompaniment finish. */
    om_bongo_enable(0); /* Solo chord, no accompaniment. */
    om_trigger(7,90);om_trigger(12,85);om_trigger(18,75);render(44100);
    om_note_off(7);om_note_off(12);om_note_off(18);render(44100);
    om_panic();render(total-(22050+12*13230+145530+88200));
    if(fclose(wav))return 2;
    return 0;
}
