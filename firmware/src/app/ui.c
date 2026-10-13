/* SPDX-License-Identifier: GPL-3.0-only */
/* Input runs independently of the 12 fps display. Only changed strips are sent
 * to the panel; animation never runs in the audio interrupt. */
#include "pet_art.h"
static const uint8_t WHITE_K[16]={0,2,4,6,7,9,11,12,14,16,18,19,21,23,24,26};
static const uint8_t BLACK_K[11]={1,3,5,8,10,13,15,17,20,22,25};
/* Steady guides identify usable controls without creating input edges. */
static const uint32_t CARE_LIGHTS=(1u<<1)|(1u<<3)|(1u<<5)|(1u<<8);
static const uint32_t BUTTON_LIGHTS=(1u<<B_GLO)|(1u<<B_HOME)|(1u<<B_SAVE)|(1u<<B_PLAY);
static const char *const CARE_NAMES[4]={"FEED","PRAISE","CLEAN","REST"};
static const enum pet_action_t CARE_ACTIONS[4]={PET_FEED,PET_PRAISE,PET_CLEAN,PET_REST};
static struct {
    uint32_t keys,buttons,midi_keys,deadline,last_frame,last_note,last_save;
    uint32_t pet_signature,bongo_hits,bongo_at,scene_motion,action_at,idle_delay,highlight_until;
    uint8_t selected,map,pending_save,blink,playing,hand,action_kind;
    uint8_t poo_done,news_seated,after_chair;
    uint8_t voice_poo_started,voice_perfect_pending,voice_praise,voice_sleeping,voice_news;
    uint8_t voice_wait_action;
    uint8_t caption_voice; /* snapshot of the actual playing clip, not its queued request */
    uint8_t autosave_flash;
    uint8_t game_over,game_over_saved; /* 1 waiting, 2 fresh press requested restart */
    uint32_t autosave_until,save_notice_until;
    int voice_care;
    uint32_t voice_news_due,voice_snore_at,voice_hunger_at,voice_tired_at,voice_refuse_at,voice_random;
    uint8_t paused;uint32_t paused_at;
    uint8_t reset_menu,reset_yes,highlight; /* 0 idle, 1 pressed briefly, 2 choosing */
    uint8_t redraw,header_dirty,meters_dirty,scene_dirty,caption_dirty,controls_dirty;
    int action_dx;
    const char *message,*feedback;
    char sub[32];
} ui;
static uint32_t control_lights(void)
{ return BUTTON_LIGHTS | (ui.reset_menu ? 1u<<B_SEL : 0u); }
static void shift_paused_time(uint32_t elapsed)
{
    pet_shift_time(elapsed);
    ui.action_at+=elapsed;ui.idle_delay+=elapsed;ui.last_note+=elapsed;ui.last_save+=elapsed;
    ui.deadline+=elapsed;ui.bongo_at+=elapsed;ui.highlight_until+=elapsed;
    ui.autosave_until+=elapsed;ui.save_notice_until+=elapsed;
    ui.voice_news_due+=elapsed;ui.voice_snore_at+=elapsed;
    ui.voice_hunger_at+=elapsed;ui.voice_tired_at+=elapsed;ui.voice_refuse_at+=elapsed;
}
static void decimal(int v,char *b) {char t[12];int n=0;do{t[n++]=(char)('0'+v%10);v/=10;}while(v);while(n)*b++=t[--n];*b=0;}
static int white_key(int k) {int i;for(i=0;i<16;i++)if(WHITE_K[i]==k)return 1;return 0;}
static void centered(int y,const felucca_font_t *font,const char *s,uint16_t c)
{
    char fit[48];int n=0;
    if(!s)s="";
    while(s[n] && n<47){fit[n]=s[n];n++;}fit[n]=0;
    while(n && text_w(font,fit)>224)fit[--n]=0;
    cv_text((240-text_w(font,fit))/2,y,font,fit,c);
}
int ui_dirty(void) {return 0;}
void ui_say(const char *a,const char *b)
{
    int i;ui.message=a;
    for(i=0;i<31 && b && b[i];i++)ui.sub[i]=b[i];ui.sub[i]=0;
    ui.deadline=plat_ms()+1800u;ui.caption_dirty=1;
}
void ui_init(void)
{
    uint32_t now=plat_ms();
    ui.keys=ui.buttons=ui.midi_keys=0;ui.message=ui.feedback=0;
    ui.selected=ui.map=ui.pending_save=ui.playing=ui.hand=ui.blink=0;
    ui.paused=0;ui.paused_at=now;
    ui.reset_menu=ui.reset_yes=ui.highlight=ui.poo_done=ui.news_seated=0;
    ui.action_kind=ui.after_chair=0;ui.action_at=now;ui.idle_delay=0;
    ui.redraw=1;ui.pet_signature=0xffffffffu;
    ui.last_frame=now-84u;ui.last_note=now-3000u;ui.last_save=now;
    ui.scene_motion=0xffffffffu;ui.bongo_hits=om_bongo_hits;ui.bongo_at=now-1000u;
    ui.voice_care=PV_NONE;ui.voice_wait_action=0;
    ui.caption_voice=0;
    ui.autosave_flash=0;ui.autosave_until=ui.save_notice_until=now;
    ui.game_over=ui.game_over_saved=0;
    ui.voice_poo_started=ui.voice_perfect_pending=ui.voice_praise=ui.voice_news=0;
    ui.voice_sleeping=pet.sleeping;ui.voice_random=now^0x93d28731u;
    ui.voice_snore_at=now-4000u;ui.voice_hunger_at=ui.voice_tired_at=now-45000u;
    ui.voice_refuse_at=now-1500u;ui.voice_news_due=now;
    plat_glow(0);plat_play_red(0);plat_leds(control_lights(),CARE_LIGHTS);
}
static void finish_action(uint32_t now)
{
    if(ui.action_kind){ui.idle_delay+=now-ui.action_at;ui.action_kind=ui.after_chair=0;ui.scene_dirty=ui.header_dirty=1;}
}
static int idle_dx(uint32_t now)
{
    uint32_t t=(now-ui.idle_delay)%45000u,phase=t/1000u;
    if(pet.sleeping || ui.playing)return 0;
    if(phase>=2u && phase<8u){int leg=(int)t-2000;return leg<3000?leg/250:(6000-leg)/250;}
    if(phase>=8u&&phase<18u){int t=(int)((now-ui.idle_delay)%45000u)-8000;return t<5000?-t/250:(t-5000)/250-20;}
    if(phase>=28u){int leg=(int)t-28000;return leg<8500?-leg/500:-(17000-leg)/500;}
    return phase>=18u&&phase<28u?8:0;
}
static int idle_walking(uint32_t now)
{
    uint32_t phase=((now-ui.idle_delay)/1000u)%45u;
    return !pet.sleeping && !ui.playing && ((phase>=2u && phase<18u) || phase>=28u);
}
static int walking_now(uint32_t now)
{
    uint32_t age=now-ui.action_at;
    if(ui.map || ui.reset_menu || ui.keys || ui.midi_keys || om_active_keys || om_bongo_active)return 0;
    switch(ui.action_kind){
        case 4:return age<1800u;
        case 5:return age<800u || age>=3400u;
        case 7:return age<800u || age>=2400u;
        case 8:return age<1000u;
        case 9:return age>=1000u;
        case 10:return age>=500u;
        case 0:return idle_walking(now);
        default:return 0;
    }
}
static int action_position(uint32_t now)
{
    uint32_t age=now-ui.action_at;
    if(ui.action_kind==9){if(age<1000u)return -74;age-=1000u;if(age>2000u)age=2000u;return -74+74*(int)age/2000;}
    if(ui.action_kind==10){if(age<500u)return 8;age-=500u;if(age>1000u)age=1000u;return 8-8*(int)age/1000;}
    if(ui.action_kind==8){if(age>1000u)age=1000u;return ui.action_dx+(8-ui.action_dx)*(int)age/1000;}
    if(ui.action_kind==4){if(age>1800u)age=1800u;return ui.action_dx+(-74-ui.action_dx)*(int)age/1800;}
    if(ui.action_kind==7){
        int target=pet_poo_x(pet.poo_count-(ui.poo_done?1:0))-120;
        if(age<800u)return ui.action_dx+(target-ui.action_dx)*(int)age/800;
        if(age<2400u)return target;
        age-=2400u;if(age>1000u)age=1000u;
        return target+(ui.action_dx-target)*(int)age/1000;
    }
    if(ui.action_kind==5){
        if(age<800u)return ui.action_dx+(44-ui.action_dx)*(int)age/800;
        if(age<3400u)return 44;
        age-=3400u;if(age>800u)age=800u;
        return 44+(ui.action_dx-44)*(int)age/800;
    }
    return ui.action_dx;
}
static int news_session(uint32_t now)
{
    uint32_t pose_now=(ui.action_kind?ui.action_at:now)-ui.idle_delay;
    uint32_t phase=(pose_now/1000u)%45u;
    return phase>=18u && phase<28u && !pet.sleeping;
}
static int watching_news(uint32_t now)
{
    return news_session(now) && ui.news_seated && (!ui.action_kind || ui.action_kind==6);
}
static int bongo_eligible(uint32_t now)
{
    return pet_can_dance() && !news_session(now) && ui.action_kind!=7 && ui.action_kind!=9 && ui.action_kind!=10;
}
static void cancel_reactions(void)
{
    om_voice_stop();ui.voice_care=PV_NONE;
    ui.voice_perfect_pending=ui.voice_poo_started=0;
}
static void request_care_voice(int clip,int wait_action)
{
    cancel_reactions();ui.voice_care=clip;ui.voice_wait_action=(uint8_t)wait_action;
}
static void wake_animation(uint32_t now,int in_bed)
{
    cancel_reactions();
    finish_action(now);ui.news_seated=0;
    if(in_bed){ui.action_kind=9;ui.action_at=now;ui.action_dx=-74;}
    else ui.idle_delay=now;
    ui.scene_dirty=ui.header_dirty=1;
}
static void care(int choice,uint32_t now)
{
    ui.selected=(uint8_t)choice;ui.highlight=1;ui.highlight_until=now+400u;
    ui.message=0;ui.caption_dirty=1;
    if(ui.map)ui.scene_dirty=1;
    int from_dx=ui.action_kind?action_position(now):idle_dx(now);
    int was_asleep=pet.sleeping;
    int in_bed=was_asleep && ui.action_kind!=10 && (ui.action_kind!=4 || now-ui.action_at>=1800u);
    int from_chair=ui.news_seated || (ui.action_kind==8 && now-ui.action_at>=1000u);
    int accepted=pet_action(CARE_ACTIONS[choice],now);
    /* A compliment still earns a reaction when the mood reward is capped or
     * cooling down. Basic needs/sleep still take priority; stats stay in pet.c. */
    int acknowledge_praise=choice==1 && !pet.sleeping && !pet_is_night() &&
        pet.food>25 && pet.energy>20 && pet.cleanliness>25;
    if(accepted || acknowledge_praise){
        ui.news_seated=0;
        if(!accepted)ui_say("YES, I KNOW. THANK YOU.",0);
        if(!accepted && ui.action_kind==2)return;
        int dx=from_dx;
        finish_action(now);
        if(choice==0 || choice==1){
            ui.action_kind=(uint8_t)(choice+1);ui.action_at=now;ui.action_dx=dx;
            ui.map=0;ui.scene_dirty=1;
        }else if(choice==3 && pet.sleeping){
            ui.action_kind=4;ui.action_at=now;ui.action_dx=dx;
            ui.map=0;ui.scene_dirty=ui.header_dirty=1;
            ui_say("OFF TO BED",0);ui.deadline=now+3000u;
        }
    }
    if(accepted && choice==3 && was_asleep && !pet.sleeping)wake_animation(now,in_bed);
    else if((accepted || acknowledge_praise) && from_chair){
        ui.after_chair=ui.action_kind;ui.action_kind=10;ui.action_at=now;ui.action_dx=8;
        ui.scene_dirty=1;
    }
    if(accepted || acknowledge_praise){
        if(choice==0)request_care_voice(PV_EAT_FEED,1);
        else if(choice==1){
            request_care_voice(ui.voice_praise?PV_PRAISE_2:PV_PRAISE_1,2);
            ui.voice_praise=(uint8_t)!ui.voice_praise;
        }else if(choice==2)request_care_voice(PV_CLEAN,0);
        else cancel_reactions();
    }else if(now-ui.voice_refuse_at>=1500u){
        request_care_voice(PV_REFUSE,0);ui.voice_refuse_at=now;
    }
    /* Show the engine's live feedback: a new musical reaction can replace it. */
}
static void trigger_white(int k,int velocity,uint32_t now)
{
    if(!white_key(k))return;
    if(ui.action_kind==7){cancel_reactions();finish_action(now);}
    if(!news_session(now))pet_note(now);
    om_bongo_enable(bongo_eligible(now));om_trigger(k,velocity);
    ui.last_note=now;
}
static void quiet(void)
{
    cancel_reactions();om_panic();ui.midi_keys=0;ui.playing=ui.hand=0;
    ui.bongo_at=plat_ms()-1000u;ui.scene_dirty=1;
}
static void service_save(uint32_t now)
{
    int explicit_save=ui.pending_save;
    if(!explicit_save && (!pet_needs_save() || now-ui.last_save<60000u))return;
    if(ui.keys || ui.midi_keys || om_active_keys || om_bongo_active || om_voice_busy() || om_foot_busy() || om_game_over_busy() || now-ui.last_note<3000u)return;
    /* Record attempts, including failures, so unavailable storage cannot spin. */
    ui.last_save=now;ui.pending_save=0;
    if(explicit_save)ui.save_notice_until=now+1800u;
    if(pet_save()) ui_say("SAVE FAILED","TRY SAVE AGAIN LATER");
    else if(explicit_save)ui_say("PET SAVED","CARE PROGRESS KEPT");
    else {ui.autosave_flash=1;ui.autosave_until=now+1800u;ui.header_dirty=1;}
}
static uint32_t voice_random_interval(void)
{
    uint32_t x=ui.voice_random;x^=x<<13;x^=x>>17;x^=x<<5;
    ui.voice_random=x?x:0x93d28731u;
    return 5000u+ui.voice_random%6001u;
}
static void service_reactions(uint32_t now)
{
    if(ui.voice_sleeping!=pet.sleeping){
        om_voice_stop();ui.voice_sleeping=pet.sleeping;
        ui.voice_snore_at=now-4000u;
        ui.voice_perfect_pending=ui.voice_poo_started=0;
    }
    if(ui.voice_care!=PV_NONE){
        int ready=!ui.voice_wait_action || ui.action_kind==ui.voice_wait_action;
        if(ui.voice_wait_action && !ready && !(ui.action_kind==10 && ui.after_chair==ui.voice_wait_action))
            ui.voice_care=PV_NONE;
        else if(ready && (ui.voice_wait_action!=6 || now-ui.action_at>=800u)){
            if(om_voice_play(ui.voice_care))ui.voice_care=PV_NONE;
        }
        return;
    }
    if(ui.action_kind==7 && !ui.voice_poo_started && now-ui.action_at>=800u){
        if(om_voice_busy()){om_voice_stop();return;}
        if(om_voice_play(PV_POO))ui.voice_poo_started=1;
        return;
    }
    if(ui.voice_perfect_pending){
        if(om_voice_play(PV_PERFECT))ui.voice_perfect_pending=0;
        return;
    }
    int watching=watching_news(now);
    if(watching && !ui.voice_news)ui.voice_news_due=now+voice_random_interval();
    ui.voice_news=(uint8_t)watching;
    if(om_voice_busy() || ui.pending_save || (int32_t)(now-ui.save_notice_until)<0 || ui.map || ui.reset_menu)return;
    if(pet.sleeping){
        if(ui.action_kind!=10 && (ui.action_kind!=4 || now-ui.action_at>=1800u) &&
           now-ui.voice_snore_at>=4000u && om_voice_play(PV_SNORE))ui.voice_snore_at=now;
        return;
    }
    /* Ambient calls yield to active care, music and transitions. Never fire per tick. */
    if(ui.action_kind || ui.keys || ui.midi_keys || om_active_keys || om_bongo_active)return;
    if(pet.food<=25 && now-ui.voice_hunger_at>=45000u){
        if(om_voice_play(PV_HUNGRY))ui.voice_hunger_at=now;
    }else if((pet.energy<=20 || pet_is_night()) && now-ui.voice_tired_at>=45000u){
        if(om_voice_play(PV_TIRED))ui.voice_tired_at=now;
    }else if(watching && (int32_t)(now-ui.voice_news_due)>=0){
        if(om_voice_play(PV_NEWS))ui.voice_news_due=now+voice_random_interval();
    }
}
static int visible_needs_empty(void)
{
    return !pet.food && !pet.mood && !pet.energy;
}
static void start_fresh_pet(uint32_t now,uint32_t keys,uint32_t buttons)
{
    pet_init(now);pet.food=pet.mood=pet.energy=pet.cleanliness=100;
    int failed=pet_save();
    project_apply(); /* reapply retained settings after the audio cancellation */
    ui_init();ui.keys=keys;ui.buttons=buttons;ui.idle_delay=now;
    if(failed)ui_say("NEW PET NOT SAVED","PRESS SAVE TO RETRY");
    else ui_say("A FRESH START","NEW PET SAVED");
}
static void enter_game_over(uint32_t now,uint32_t keys,uint32_t buttons)
{
    quiet();finish_action(now);om_bongo_enable(0);om_footstep(0,0);
    ui.game_over=1;ui.game_over_saved=0;
    ui.map=ui.reset_menu=ui.highlight=ui.autosave_flash=ui.pending_save=0;
    ui.message=0;ui.caption_voice=0;ui.keys=keys;ui.buttons=buttons;
    ui.redraw=1;om_game_over(); /* once on entry; a held button cannot restart */
}
void ui_input_only(void)
{
    uint32_t now=plat_ms(),keys=plat_keys(),down=keys & ~ui.keys;
    uint32_t up=ui.keys & ~keys,buttons=plat_buttons(),pressed=buttons & ~ui.buttons,pkt;
    int k;
    int was_asleep=pet.sleeping;
    int in_bed=was_asleep && ui.action_kind!=10 && (ui.action_kind!=4 || now-ui.action_at>=1800u);
    if(!ui.game_over && visible_needs_empty())enter_game_over(now,keys,buttons);
    if(ui.game_over){
        for(k=0;k<NE;k++)(void)plat_enc(k);
        for(k=0;k<64 && plat_midi_in(&pkt);k++){}
        /* Only new physical edges count, never a held key or incoming MIDI. */
        down=keys & ~ui.keys;pressed=buttons & ~ui.buttons;
        ui.keys=keys;ui.buttons=buttons;
        if(ui.game_over==1 && (down || pressed)){quiet();ui.game_over=2;}
        if(!om_game_over_busy() && !om_voice_busy() && !om_active_keys && !om_foot_busy()){
            if(ui.game_over==2)start_fresh_pet(now,keys,buttons);
            else if(!ui.game_over_saved){
                ui.game_over_saved=1;
                if(pet_save())ui_say("SAVE FAILED","GAME OVER NOT SAVED");
            }
        }
        plat_leds(control_lights(),CARE_LIGHTS);return; /* care clock freezes until restart */
    }
    if(ui.paused){
        shift_paused_time(now-ui.paused_at);ui.paused_at=now;
        for(k=0;k<NE;k++)(void)plat_enc(k);
        for(k=0;k<64 && plat_midi_in(&pkt);k++){}
        ui.keys=keys;ui.buttons=buttons;
        if(pressed & (1u<<B_PLAY)){ui.paused=0;ui.message=0;ui.redraw=1;}
        else if(pressed & (1u<<B_SAVE)){ui.pending_save=1;ui.caption_dirty=1;}
        if(ui.pending_save)service_save(now);
        plat_leds(control_lights(),CARE_LIGHTS);return;
    }
    pet_tick(now);
    if(visible_needs_empty()){enter_game_over(now,keys,buttons);plat_leds(control_lights(),CARE_LIGHTS);return;}
    if(was_asleep && !pet.sleeping)wake_animation(now,in_bed);
    if(!ui.reset_menu && (pressed & (1u<<B_PLAY))){
        quiet();pet_bongo_reward(0,now);ui.paused=1;ui.paused_at=now;
        ui.last_note=now-3000u;ui.keys=keys;ui.buttons=buttons;ui.message=0;ui.caption_dirty=1;
        plat_leds(control_lights(),CARE_LIGHTS);return;
    }
    if(pressed & (1u<<B_GLO)){
        if(!ui.reset_menu){quiet();finish_action(now);ui.highlight=0;ui.reset_menu=1;ui.reset_yes=0;ui.last_note=now;}
        else ui.reset_menu=0;
        ui.redraw=1;
        /* Consume this frame so GLO cannot also perform a care action. */
        ui.keys=keys;ui.buttons=buttons;plat_leds(control_lights(),CARE_LIGHTS);return;
    }
    if(ui.reset_menu){
        for(k=0;k<NE;k++){
            int d=plat_enc(k);
            if(ui.reset_menu==1 && k==EN_SELECT && d){
                int yes=((int)ui.reset_yes+d)%2;if(yes<0)yes+=2;
                ui.reset_yes=(uint8_t)yes;ui.scene_dirty=1;
            }
        }
        /* Ignore care, notes and PLAY in this confirmation. Only SEL confirms. */
        for(k=0;k<64 && plat_midi_in(&pkt);k++){}
        ui.keys=keys;ui.buttons=buttons;
        pet_watch_news(0,now);pet_bongo_reward(0,now);om_bongo_enable(0);
        if(pressed & (1u<<B_HOME)){ui.reset_menu=0;ui.redraw=1;}
        else if(ui.reset_menu==1){
            if(pressed & (1u<<B_SEL)){
                if(!ui.reset_yes){ui.reset_menu=0;ui.redraw=1;}
                else{ui.reset_menu=2;ui.scene_dirty=1;}
            }
        }
        /* Let the audio tail settle before writing flash. Keep calibration and
         * sound settings; only confirmed pet progress is replaced. */
        if(ui.reset_menu==2 && !keys && !om_active_keys && !om_bongo_active && now-ui.last_note>=3000u){
            start_fresh_pet(now,keys,buttons);
        }
        plat_leds(control_lights(),CARE_LIGHTS);return;
    }
    if(pressed & (1u<<B_HOME)){ui.map=(uint8_t)!ui.map;ui.highlight=0;ui.message=0;ui.scene_dirty=ui.caption_dirty=1;}
    if(pressed & (1u<<B_SAVE)){ui.pending_save=1;ui_say("SAVE QUEUED","WHEN MUSIC IS QUIET");}
    for(k=0;k<4;k++)if(down & (1u<<BLACK_K[k]))care(k,now);
    for(k=0;k<16;k++){
        int key=WHITE_K[k];uint32_t mask=1u<<key;
        if(down & mask)trigger_white(key,127,now);
        if((up & mask) && !(ui.midi_keys & mask))om_note_off(key);
    }
    ui.keys=keys;ui.buttons=buttons;
    for(k=0;k<NE;k++)(void)plat_enc(k); /* SELECT is handled only in GLO. */
    /* Channel 1 F3..G5 only. Black MIDI notes never invoke care. Physical and
     * MIDI ownership are independent, so releasing one cannot cut off the other. */
    for(k=0;k<64 && plat_midi_in(&pkt);k++){
        uint32_t st=(pkt>>8)&255u,note=(pkt>>16)&127u,vel=(pkt>>24)&127u;
        if((st==0x90 || st==0x80) && note>=53 && note<=79){
            int key=(int)note-53;uint32_t mask=1u<<key;
            if(!white_key(key))continue;
            if(st==0x90 && vel){ui.midi_keys|=mask;trigger_white(key,(int)vel,now);}
            else {ui.midi_keys&=~mask;if(!(keys & mask))om_note_off(key);}
        }
        if(st==0xb0 && (note==120 || note==123))quiet();
    }
    if(ui.action_kind==7 && !ui.poo_done && !pet.sleeping && now-ui.action_at>=2200u){
        ui.poo_done=(uint8_t)pet_make_poo(now);
        if(ui.poo_done)ui.voice_perfect_pending=1;
        ui.scene_dirty=ui.caption_dirty=1;
    }
    if(!news_session(now)){
        if(ui.news_seated && !ui.action_kind && !pet.sleeping){
            ui.action_kind=10;ui.action_at=now;ui.action_dx=8;ui.after_chair=0;ui.scene_dirty=1;
        }
        ui.news_seated=0;
    }
    if(news_session(now) && !ui.action_kind && !ui.news_seated && !ui.map){
        ui.action_dx=0;ui.action_kind=8;ui.action_at=now;ui.scene_dirty=1;
    }
    pet_watch_news(watching_news(now),now);
    /* A limited side snack is an idle activity, never an interruption to input. */
    if(!ui.action_kind && !pet.sleeping && !ui.keys && !ui.midi_keys &&
       !om_active_keys && !om_bongo_active && !ui.map && !ui.pending_save && (int32_t)(now-ui.save_notice_until)>=0 &&
       watching_news(now) && pet_take_snack(now)){
        ui.action_dx=8;ui.action_kind=6;ui.action_at=now;
        request_care_voice(PV_EAT_TV,6);
        ui.scene_dirty=ui.caption_dirty=1;
    }
    if(!ui.action_kind && !pet.sleeping && !ui.keys && !ui.midi_keys &&
       !om_active_keys && !om_bongo_active && !ui.map && !ui.pending_save && (int32_t)(now-ui.save_notice_until)>=0 && !news_session(now) && pet_poo_ready()){
        ui.action_dx=idle_dx(now);ui.action_kind=7;ui.action_at=now;ui.poo_done=0;
        ui.voice_poo_started=ui.voice_perfect_pending=0;
        ui.scene_dirty=1;
    }
    om_bongo_enable(bongo_eligible(now));
    pet_bongo_reward(bongo_eligible(now) && om_bongo_active,now);
    service_reactions(now);
    /* Briefly silence gait sounds for pending writes rather than delaying SAVE
     * through an entire walk. Render acknowledges quiet via om_foot_busy. */
    int save_due=ui.pending_save || (pet_needs_save() && now-ui.last_save>=60000u);
    om_footstep(!save_due && walking_now(now),(unsigned)((ui.action_kind?now-ui.action_at:now-ui.idle_delay)/(ui.action_kind?160u:250u)));
    service_save(now);
    plat_leds(control_lights(),CARE_LIGHTS | keys | om_active_keys);
}
static void draw_header(void)
{
    const char *state=ui.game_over?"GAME OVER":ui.action_kind==4?"BEDTIME":pet.sleeping?"SLEEP":pet_state_name();
    cv_begin(240,24,PET_INK);
    cv_text(8,5,&FONT_S,"TRUMPAGOTCHI",PET_GOLD);
    if(ui.autosave_flash)cv_text(110,7,&FONT_XS,"AUTOSAVED",PET_MINT);
    cv_text(232-text_w(&FONT_XS,state),7,&FONT_XS,state,PET_PAPER);
    cv_blit(0,0);ui.header_dirty=0;
}
static void draw_meters(void)
{
    static const char *const labels[3]={"FOOD","MOOD","ENERGY"};
    int values[3]={pet.food,pet.mood,pet.energy},k;
    cv_begin(240,26,PET_PAPER);
    for(k=0;k<3;k++){
        int x=8+k*78,v=values[k];if(v>100)v=100;
        cv_text(x,1,&FONT_XS,labels[k],PET_INK);
        cv_rect(x,16,68,5,RGB(211,197,170));
        cv_rect(x,16,v*68/100,5,v<30?PET_RED:PET_MINT);
    }
    cv_blit(0,24);ui.meters_dirty=0;
}
static void draw_map(void)
{
    static const char *const groups[4]={"1","2","3","4"};
    int k;
    cv_begin(240,156,PET_PAPER);
    centered(0,&FONT_XS,"FIRST FOUR BLACK KEYS",PET_INK);
    for(k=0;k<4;k++){
        int y=17+k*14;
        cv_text(18,y,&FONT_XS,groups[k],PET_INK);
        cv_text(105,y,&FONT_XS,CARE_NAMES[k],ui.highlight && k==ui.selected?PET_RED:PET_INK);
        if(k==3)cv_text(157,y,&FONT_XS,"SLEEP / WAKE",PET_INK);
    }
    centered(91,&FONT_XS,"PLAY: PAUSE",PET_INK);
    centered(114,&FONT_XS,"HOME: BACK TO PET",PET_INK);
}
static void draw_scene(void)
{
    if(ui.game_over){
        cv_begin(240,156,PET_PAPER);
        centered(48,&FONT_M,"GAME OVER",PET_INK);
        centered(88,&FONT_XS,"LET'S TRY THAT AGAIN",PET_INK);
    }
    else if(ui.reset_menu){
        cv_begin(240,156,PET_PAPER);
        centered(10,&FONT_M,"START OVER?",PET_INK);
        centered(42,&FONT_XS,"REPLACE SAVED PET PROGRESS",PET_INK);
        if(ui.reset_menu==2){
            centered(76,&FONT_S,"RELEASE KEYS",PET_RED);
            centered(104,&FONT_XS,"STARTING A NEW PET...",PET_INK);
        }else{
            int x=ui.reset_yes?128:28;
            cv_rect(x,70,84,29,PET_INK);
            cv_text(57,77,&FONT_S,"NO",ui.reset_yes?PET_INK:PET_GOLD);
            cv_text(153,77,&FONT_S,"YES",ui.reset_yes?PET_GOLD:PET_INK);
            centered(106,&FONT_XS,"SELECT KNOB: CHOOSE",PET_INK);
        }
        centered(128,&FONT_XS,"WHITE KEYS: MUSIC",PET_INK);
        centered(142,&FONT_XS,"FIRST 4 BLACK KEYS: CARE",PET_INK);
    }
    else if(ui.map)draw_map();
    else {
        uint32_t now=plat_ms(),pose_now=(ui.action_kind?ui.action_at:now)-ui.idle_delay;
        int walking=idle_walking(now);
        int watching=watching_news(now);
        int dx=watching?8:0;
        /* Walk out and back continuously; return naturally toward the TV chair. */
        if(walking)dx=idle_dx(now);
        if(ui.action_kind){walking=watching=0;dx=action_position(now);}
        pet_room(pet.sleeping,pet.day_minute,now);
        pet_character(pet.state,pet.sleeping,ui.blink,ui.playing&&!ui.action_kind,walking?(int)(pose_now/250u)%2:ui.hand,dx,walking,watching,ui.action_kind,now-ui.action_at);
    }
    cv_blit(0,50);ui.scene_dirty=0;
}
static const char *spoken_caption(unsigned active)
{
    static const char *const captions[PV_COUNT]={
        "A SCRUMPTIOUS TREAT","I LOVE BURGERS","FAKE NEWS","I WANT FOOD","I WANT SLEEP","I'D RATHER NOT",
        "PERFECT SPECIMEN",0,0,"THANK YOU, THANK YOU","THANK YOU VERY MUCH","VERY CLEAN"
    };
    return active && active<=PV_COUNT?captions[active-1u]:0;
}
static int caption_equal(const char *a,const char *b)
{
    while(*a && *a==*b){a++;b++;}return *a==*b;
}
static const char *caption_text(void)
{
    const char *speech=spoken_caption(ui.caption_voice);
    if(speech)return speech;
    if(ui.message)return ui.message;
    if(ui.pending_save)return "SAVE WAITING FOR QUIET";
    if(watching_news(plat_ms()))return "NEWS IS UNFAIR";
    const char *feedback=pet_feedback();
    /* Rotate only the idle need text; actions and actual speech remain exact. */
    unsigned variant=(plat_ms()/8000u)%3u;
    static const char *const hungry[3]={"I WANT FOOD","WHERE IS MY SNACK?","BRING ME A BURGER"};
    static const char *const tired[3]={"I WANT SLEEP","AN EXECUTIVE NAP, PLEASE","I NEED MY BEAUTY SLEEP"};
    static const char *const clean[3]={"THIS PLACE NEEDS A CLEAN","A LITTLE HOUSEKEEPING?","I EXPECT A TIDY ROOM"};
    static const char *const bored[3]={"A LITTLE ATTENTION?","A LITTLE PRAISE, PLEASE","DID YOU SEE MY RATINGS?"};
    if(caption_equal(feedback,"I WANT FOOD"))return hungry[variant];
    if(caption_equal(feedback,"I WANT SLEEP"))return tired[variant];
    if(caption_equal(feedback,"THIS PLACE NEEDS A CLEAN"))return clean[variant];
    if(caption_equal(feedback,"A LITTLE ATTENTION?"))return bored[variant];
    return feedback;
}
static void draw_caption(void)
{
    int k;
    cv_begin(240,34,PET_PAPER);
    if(ui.paused){
        centered(0,&FONT_S,"PAUSED",PET_INK);
        centered(20,&FONT_XS,ui.message?ui.message:"PLAY: CONTINUE",PET_INK);
        cv_blit(0,206);ui.caption_dirty=0;return;
    }
    if(ui.game_over){
        centered(8,&FONT_XS,"PRESS ANY BUTTON TO RESTART",PET_INK);
        cv_blit(0,206);ui.caption_dirty=0;return;
    }
    if(ui.reset_menu){
        centered(0,&FONT_XS,"SEL: CONFIRM",PET_INK);
        centered(18,&FONT_XS,"HOME / GLO: CANCEL",PET_INK);
        cv_blit(0,206);ui.caption_dirty=0;return;
    }
    centered(0,&FONT_XS,caption_text(),PET_INK);
    /* Calibration/status messages retain their second line temporarily. */
    if(ui.message && ui.sub[0])centered(17,&FONT_XS,ui.sub,PET_INK);
    else for(k=0;k<4;k++){
        int x=k*60;
        if(ui.highlight && k==ui.selected)cv_rect(x+1,16,58,17,PET_INK);
        cv_text(x+(60-text_w(&FONT_XS,CARE_NAMES[k]))/2,18,&FONT_XS,CARE_NAMES[k],ui.highlight && k==ui.selected?PET_GOLD:PET_INK);
    }
    cv_blit(0,206);ui.caption_dirty=0;
}
void ui_frame(void)
{
    uint32_t now,sig,hits;int blink,playing,hand;
    ui_input_only();now=plat_ms();
    if(ui.paused){if(ui.caption_dirty)draw_caption();return;}
    if(ui.autosave_flash && (int32_t)(now-ui.autosave_until)>=0){
        ui.autosave_flash=0;ui.header_dirty=1;
    }
    {unsigned active=om_voice_active;
        if(active!=ui.caption_voice){ui.caption_voice=(uint8_t)active;ui.caption_dirty=1;}
    }
    if(ui.highlight==1 && (int32_t)(now-ui.highlight_until)>=0){
        ui.highlight=0;ui.caption_dirty=1;if(ui.map)ui.scene_dirty=1;
    }
    if(ui.action_kind){
        uint32_t length=ui.action_kind==9?3000u:ui.action_kind==10?1500u:ui.action_kind==8?2000u:ui.action_kind==7?3400u:ui.action_kind==5?4200u:ui.action_kind==4?3000u:(ui.action_kind==1 || ui.action_kind==6)?2600u:2200u;
        int cancelled=ui.action_kind==4?!pet.sleeping:ui.action_kind==10&&ui.after_chair==4?!pet.sleeping:pet.sleeping;
        if(cancelled || now-ui.action_at>=length){
            int finished=ui.action_kind,next=ui.after_chair;
            if(finished==8 && !cancelled)ui.news_seated=1;
            finish_action(now);
            if(finished==9 && !cancelled)ui.idle_delay=now;
            if(finished==10 && next && !cancelled){
                ui.action_kind=(uint8_t)next;ui.action_at=now;ui.action_dx=0;
            }
        }
    }
    if(ui.message && (int32_t)(now-ui.deadline)>=0){ui.message=0;ui.caption_dirty=1;}
    sig=(uint32_t)pet.food|((uint32_t)pet.mood<<7)|((uint32_t)pet.energy<<14)|
        ((uint32_t)pet.cleanliness<<21)|((uint32_t)pet.sleeping<<28)|((uint32_t)pet.state<<29);
    if(sig!=ui.pet_signature){
        ui.meters_dirty=1;
        if((sig^ui.pet_signature)&0xf0000000u)ui.header_dirty=ui.scene_dirty=1;
        if(ui.map)ui.scene_dirty=1;
        ui.pet_signature=sig;
    }
    if(ui.feedback!=caption_text()){ui.feedback=caption_text();ui.caption_dirty=1;}
    hits=om_bongo_hits;
    if(hits!=ui.bongo_hits){ui.bongo_hits=hits;ui.bongo_at=now;}
    blink=(now%4300u)<150u;
    playing=bongo_eligible(now) && om_bongo_active;
    hand=playing?(int)(((now-ui.bongo_at)/110u+hits)%2u)+1:0;
    if(blink!=ui.blink || playing!=ui.playing || hand!=ui.hand){
        ui.blink=(uint8_t)blink;ui.playing=(uint8_t)playing;ui.hand=(uint8_t)hand;
        if(!ui.map)ui.scene_dirty=1;
    }
    {uint32_t motion=now/(ui.action_kind?100u:250u);
        if(motion!=ui.scene_motion){ui.scene_motion=motion;if(!ui.map)ui.scene_dirty=1;}
    }
    if(now-ui.last_frame<84u)return;
    ui.last_frame=now;
    if(ui.redraw){
        ui.header_dirty=ui.meters_dirty=ui.scene_dirty=ui.caption_dirty=ui.controls_dirty=1;ui.redraw=0;
    }
    if(ui.header_dirty)draw_header();
    if(ui.meters_dirty)draw_meters();
    if(ui.scene_dirty)draw_scene();
    if(ui.caption_dirty)draw_caption();
    ui.controls_dirty=0; /* Sound knobs work without occupying the pet screen. */
}
