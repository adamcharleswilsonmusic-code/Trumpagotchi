/* SPDX-License-Identifier: GPL-3.0-only */
/* Trumpagotchi: a little reedy kazoo and user-accompaniment bongos.
 * Single UI producer, audio-IRQ consumer; only render owns live DSP state. */
#include "omni.h"
#include "fastmath.h"
#include "voice_bank.h"
/* Legacy parameter ranges remain for compatible pet saves; sound is dry. */
static const om_param_t params[P_NPARAMS] = {
    {"REVERB",0,100,15}, {"DELAY",0,100,0}, {"TIME ms",80,500,240}, {"REPEATS",0,70,25}
};
static const int8_t white_index[FMO_KEYS] = {
    0,-1,1,-1,2,-1,3,4,-1,5,-1,6,7,-1,8,-1,9,-1,10,11,-1,12,-1,13,14,-1,15
};
static const float frequencies[16] = {
    130.81278f,146.83238f,164.81378f,195.99772f,220.0f,
    261.62557f,293.66477f,329.62756f,391.99544f,440.0f,
    523.25113f,587.32954f,659.25511f,783.99087f,880.0f,1046.50226f
};
static const char *const note_names[16] = {
    "C3","D3","E3","G3","A3","C4","D4","E4","G4","A4","C5","D5","E5","G5","A5","C6"
};
static int white(int k) { return k >= 0 && k < FMO_KEYS && white_index[k] >= 0; }
const om_param_t *om_param_info(int p) { return p >= 0 && p < P_NPARAMS ? &params[p] : 0; }
const char *om_sample_name(int k) { return white(k) ? note_names[white_index[k]] : ""; }
int om_placeholder(void) { return 0; } /* Synthesized instrument, no recording claim. */
volatile uint32_t om_active_keys, om_triggers, om_dropped, om_bongo_hits, om_bongo_active;
/* Inert legacy exports keep the host/diagnostic ABI intact. */
volatile uint8_t om_seq_playing, om_seq_recording, om_seq_step;
volatile uint16_t om_seq_bpm;
volatile uint32_t om_seq_mask[FMO_STEPS], om_seq_ticks, om_seq_hit_dropped;
void om_seq_toggle_play(void) {}
void om_seq_toggle_record(void) {}
void om_seq_clear(void) {}
void om_seq_set_tempo(int bpm) { (void)bpm; }
int om_take_seq_hit(int *key) { (void)key; return 0; }

#define QN 128u
#define BARRIER() __asm__ volatile("" ::: "memory")
typedef struct { int type, a, b; uint32_t release, bongo_epoch; } command_t;
static command_t queue[QN];
static volatile uint32_t qw, qr, panic_pending;
volatile uint32_t om_voice_active, om_voice_starts, om_voice_finished;
static volatile uint32_t voice_pending, voice_epoch;
static uint32_t seen_voice_epoch, voice_pos, voice_fraction;
static volatile uint32_t foot_requested, foot_step;
volatile uint32_t om_foot_hits;
static uint32_t foot_seen, foot_running, foot_noise;
static volatile uint32_t foot_remaining; /* audio writer, UI reads quiet state */
static float foot_env,foot_phase,foot_filter;
static volatile uint32_t fail_requested,fail_remaining;
volatile uint32_t om_game_over_starts;
static float fail_phase;
/* Release generations cannot be dropped by a full command queue. A queued old
 * note-on cannot resurrect a released key; a later retrigger has a new generation.
 * These aligned words each have one writer (UI), and ISR only reads them. */
static volatile uint32_t released[FMO_KEYS], bongo_requested, bongo_epoch;
static uint32_t seen_bongo_epoch;
static int par[P_NPARAMS];
typedef struct {
    uint32_t age, release;
    int key, active, gated;
    float phase, step, gain, envelope, scoop;
} voice_t;
static voice_t voices[FMO_VOICES];
static uint32_t age;
static float wobble;
#define BONGO_GRACE 132300u /* Three seconds: continue across human pauses between notes. */
#define BONGO_TICK 6615u  /* sixteenth at 100 BPM */
static uint32_t held_keys, held_generation[FMO_KEYS], bongo_grace, bongo_clock, bongo_step;
static int bongo_running;
typedef struct { float phase, env, pitch; uint32_t remaining; } drum_t;
static drum_t drums[2];
static void stop_bongo(void) {
    bongo_running=0; om_bongo_active=0; bongo_grace=bongo_clock=bongo_step=0;
    drums[0]=(drum_t){0}; drums[1]=(drum_t){0};
}
static int post(int type,int a,int b) {
    uint32_t w=qw;
    if(w-qr>=QN) {om_dropped++;return 0;}
    queue[w%QN]=(command_t){type,a,b,type==0?released[a]:type==2?voice_epoch:0,bongo_epoch};
    BARRIER();qw=w+1u;
    return 1;
}
unsigned om_voice_frames(int clip) { return clip>=0 && clip<PV_COUNT?pet_voice_clips[clip].length:0; }
void om_footstep(int walking,unsigned step) { foot_step=step;BARRIER();foot_requested=walking?1u:0u; }
int om_foot_busy(void) { return foot_requested || foot_remaining; }
int om_voice_busy(void) { return voice_pending || om_voice_active; }
void om_voice_stop(void) { voice_epoch++;BARRIER();voice_pending=0; }
int om_voice_play(int clip) {
    if(clip<0 || clip>=PV_COUNT || om_voice_busy())return 0;
    voice_pending=1;BARRIER();
    if(!post(2,clip,0)){voice_pending=0;return 0;}
    return 1;
}
void om_set(int p,int value) { if(p>=0 && p<P_NPARAMS)post(1,p,value); }
void om_trigger(int key,int velocity) {
    if(white(key) && velocity>0)post(0,key,velocity>127?127:velocity);
}
void om_note_off(int key) { if(white(key)) {released[key]++;BARRIER();} }
void om_bongo_enable(int enabled) {
    if(!enabled && bongo_requested) {bongo_epoch++;BARRIER();}
    bongo_requested=enabled?1u:0u;
}
void om_game_over(void) { fail_requested=1; }
int om_game_over_busy(void) { return panic_pending || fail_requested || fail_remaining; }
void om_panic(void) { om_voice_stop();fail_requested=0;panic_pending=1; }
static void start_voice(command_t c) {
    uint32_t j; int slot=-1;
    for(j=0;j<FMO_VOICES;j++)if(voices[j].active && voices[j].key==c.a){slot=(int)j;break;}
    if(slot<0)for(j=0;j<FMO_VOICES;j++)if(!voices[j].active){slot=(int)j;break;}
    if(slot<0) {
        slot=0;
        for(j=1;j<FMO_VOICES;j++)if((uint32_t)(age-voices[j].age)>(uint32_t)(age-voices[slot].age))slot=(int)j;
    }
    voices[slot]=(voice_t){++age,c.release,c.a,1,1,0,frequencies[white_index[c.a]]/44100.0f,
        (float)c.b/127.0f,0,0.018f};
    held_keys|=1u<<c.a;held_generation[c.a]=c.release;om_triggers++;
    if(bongo_requested && c.bongo_epoch==seen_bongo_epoch) {
        if(!bongo_running){bongo_running=1;om_bongo_active=1;bongo_clock=bongo_step=0;}
        bongo_grace=BONGO_GRACE;
    }
}
static void clear_sound(void) {
    uint32_t i;
    for(i=0;i<FMO_VOICES;i++)voices[i]=(voice_t){0};
    held_keys=0;wobble=0;stop_bongo();om_active_keys=0;
    om_voice_active=voice_pending=voice_pos=voice_fraction=0;
    foot_requested=foot_running=foot_remaining=0;foot_env=foot_phase=foot_filter=0;
    fail_remaining=0;fail_phase=0;
}
void om_init(void) {
    int i;qw=qr=panic_pending=0;age=om_triggers=om_dropped=om_bongo_hits=0;
    bongo_requested=bongo_epoch=seen_bongo_epoch=0;
    voice_epoch=seen_voice_epoch=om_voice_starts=om_voice_finished=0;
    om_foot_hits=foot_seen=0;foot_noise=0x93582731u;
    fail_requested=om_game_over_starts=0;
    for(i=0;i<FMO_KEYS;i++)released[i]=held_generation[i]=0;
    for(i=0;i<P_NPARAMS;i++)par[i]=params[i].def;
    om_seq_playing=om_seq_recording=om_seq_step=0;om_seq_bpm=100;
    om_seq_ticks=om_seq_hit_dropped=0;
    for(i=0;i<FMO_STEPS;i++)om_seq_mask[i]=0;
    clear_sound();
}
static float limit(float x) { return x/(1.0f+fm_fabsf(x)); }
static float recorded_sample(void) {
    if(!om_voice_active)return 0;
    const pet_voice_clip_t *c=&pet_voice_clips[om_voice_active-1u];
    uint32_t next=voice_pos+1u<c->length?voice_pos+1u:voice_pos;
    float a=(float)c->data[voice_pos],b=(float)c->data[next];
    float result=(a+(b-a)*(float)voice_fraction*(1.0f/44100.0f))*(0.55f/32768.0f);
    /* Rational resampling preserves duration/pitch without a growing float cursor. */
    voice_fraction+=PET_VOICE_RATE;
    if(voice_fraction>=44100u){voice_fraction-=44100u;voice_pos++;}
    if(voice_pos>=c->length){om_voice_active=0;om_voice_finished++;}
    return result;
}
static float failure_sample(void) {
    if(!fail_remaining)return 0;
    /* Two short wahs, then a longer descending wah; no sample-bank storage. */
    uint32_t age=44100u-fail_remaining,segment=age/11025u;
    uint32_t t=segment<2u?age%11025u:age-22050u;
    uint32_t length=segment<2u?9000u:22050u;
    fail_remaining--;
    if(t>=length)return 0;
    float progress=(float)t/(float)length;
    float hz=segment==0?220.0f:segment==1?174.6f:146.8f-36.8f*progress;
    fail_phase+=hz/44100.0f;if(fail_phase>=1.0f)fail_phase-=1.0f;
    float envelope=t<441u?(float)t/441.0f:t>length-1323u?(float)(length-t)/1323.0f:1.0f;
    float wah=0.3f+0.7f*fm_sin_turns(progress*0.5f);
    return (fm_sin_turns(fail_phase)+0.3f*wah*fm_sin_turns(fail_phase*3.0f))*envelope*0.075f;
}
static float foot_sample(void) {
    if(!foot_requested){foot_running=foot_remaining=0;foot_env=foot_filter=0;return 0;}
    if(!foot_running || foot_seen!=foot_step){
        foot_seen=foot_step;foot_running=1;foot_remaining=1058u;
        foot_env=1;foot_phase=foot_filter=0;om_foot_hits++;
    }
    if(!foot_remaining)return 0;
    foot_noise=foot_noise*1664525u+1013904223u;
    float noise=(float)(foot_noise>>8)*(1.0f/8388608.0f)-1.0f;
    foot_filter+=0.12f*(noise-foot_filter);
    foot_phase+=(foot_seen&1u?520.0f:430.0f)/44100.0f;
    if(foot_phase>=1.0f)foot_phase-=1.0f;
    float attack=foot_remaining>1014u?(float)(1058u-foot_remaining)*(1.0f/44.0f):1.0f;
    float result=(0.65f*foot_filter+0.35f*fm_sin_turns(foot_phase))*foot_env*attack*0.025f;
    foot_env*=0.996f;foot_remaining--;
    return result;
}
static float bongo_sample(void) {
    /* Low on beat one/three, answering high taps. Never start from idle. */
    static const uint8_t pattern[16]={1,0,0,2,0,0,2,0,1,0,0,2,0,2,0,0};
    float out=0;int j;
    if(!bongo_running)return 0;
    if(held_keys)bongo_grace=BONGO_GRACE;
    else if(!bongo_grace){stop_bongo();return 0;}
    else bongo_grace--;
    if(!bongo_clock) {
        int hit=pattern[bongo_step];bongo_step=(bongo_step+1u)%16u;bongo_clock=BONGO_TICK;
        if(hit){drums[hit-1]=(drum_t){0,1.0f,1.35f,hit==1?7938u:5733u};om_bongo_hits++;}
    }
    bongo_clock--;
    for(j=0;j<2;j++)if(drums[j].remaining) {
        drum_t *d=&drums[j];
        d->phase+=(j?255.0f:175.0f)*(1.0f/44100.0f)*d->pitch;
        if(d->phase>=1.0f)d->phase-=1.0f;
        out+=fm_sin_turns(d->phase)*d->env*0.19f;
        d->env*=j?0.9988f:0.9991f;d->pitch+=0.002f*(1.0f-d->pitch);d->remaining--;
    }
    /* Brief final fade prevents cutting a drum waveform at the grace boundary. */
    if(!held_keys && bongo_grace<441u)out*=(float)bongo_grace/441.0f;
    return out;
}
void om_render(int32_t *out,uint32_t n,uint32_t gain_q12) {
    uint32_t i,j,keys=0;float master=(float)(gain_q12>4096?4096:gain_q12)*(1.0f/4096.0f);
    if(panic_pending){qr=qw;panic_pending=0;clear_sound();}
    if(fail_requested){fail_remaining=44100u;fail_phase=0;fail_requested=0;om_game_over_starts++;}
    if(seen_voice_epoch!=voice_epoch){om_voice_active=0;voice_pos=voice_fraction=0;seen_voice_epoch=voice_epoch;}
    if(!bongo_requested || seen_bongo_epoch!=bongo_epoch)stop_bongo();
    seen_bongo_epoch=bongo_epoch;
    while(qr!=qw) {
        command_t c;BARRIER();c=queue[qr%QN];BARRIER();qr++;
        if(c.type==1)par[c.a]=c.b<params[c.a].lo?params[c.a].lo:c.b>params[c.a].hi?params[c.a].hi:c.b;
        else if(c.type==2){
            if(c.release==voice_epoch){
                voice_pending=0;
                if(!om_voice_active){om_voice_active=(uint32_t)c.a+1u;voice_pos=voice_fraction=0;om_voice_starts++;}
            }
        }
        else if(c.release==released[c.a])start_voice(c);
    }
    for(j=0;j<FMO_KEYS;j++)if(held_generation[j]!=released[j])held_keys&=~(1u<<j);
    for(j=0;j<FMO_VOICES;j++)if(voices[j].active && voices[j].release!=released[voices[j].key])voices[j].gated=0;
    for(i=0;i<n;i++) {
        float dry=0,drum,l,r,vibrato;
        wobble+=5.2f/44100.0f;if(wobble>=1.0f)wobble-=1.0f;
        vibrato=1.0f+0.003f*(4.0f*fm_fabsf(wobble-0.5f)-1.0f);
        for(j=0;j<FMO_VOICES;j++)if(voices[j].active) {
            voice_t *v=&voices[j];float s,s3,s5,tone;
            if(v->gated){v->envelope+=1.0f/308.0f;if(v->envelope>1.0f)v->envelope=1.0f;}
            else {v->envelope-=1.0f/3969.0f;if(v->envelope<=0){v->active=0;continue;}}
            v->phase+=v->step*(vibrato-v->scoop);if(v->phase>=1.0f)v->phase-=1.0f;
            v->scoop=fm_flush(v->scoop*0.9985f);
            s=fm_sin_turns(v->phase);s3=s*s*s;s5=s3*s*s;
            /* Smooth finite harmonics: round, nasal, no discontinuous saw alias. */
            tone=0.65f*s+0.22f*fm_sin_turns(2.0f*v->phase)+0.12f*(3*s-4*s3)+0.065f*(5*s-20*s3+16*s5);
            dry+=tone*v->gain*v->envelope*0.15f;
        }
        /* Dry instrument and drums; recorded speech still gently ducks them. */
        float duck=om_voice_active?0.75f:1.0f;
        float speech=recorded_sample();
        float step=foot_sample()+failure_sample();
        drum=bongo_sample();l=limit((dry+drum)*duck+speech+step)*master;
        r=limit((dry+drum)*duck+speech+step)*master;
        out[2*i]=(int32_t)(l*8388607.0f);out[2*i+1]=(int32_t)(r*8388607.0f);
    }
    for(j=0;j<FMO_VOICES;j++)if(voices[j].active)keys|=1u<<voices[j].key;
    om_active_keys=keys;
}
