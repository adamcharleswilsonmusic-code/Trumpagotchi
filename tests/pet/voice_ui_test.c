/* Real UI routing and animation timing, not just a duplicated mapping table. */
#define main scripted_host_main
#include "../../host/omni_host.c"
#undef main
#include <assert.h>
static void fresh(void){boot();pet_init(now_ms);ui_init();ui.idle_delay=now_ms;run_ms(20);}
static void active(int id){assert(om_voice_active==(uint32_t)id+1u);}
int main(void){
    fresh();care(0,now_ms);run_ms(100);active(PV_EAT_FEED);
    care(0,now_ms);run_ms(100);active(PV_REFUSE); /* feed cooldown */
    fresh();care(1,now_ms);run_ms(100);active(PV_PRAISE_1);
    run_ms(2400);care(1,now_ms);run_ms(100);active(PV_PRAISE_2);
    pet.cleanliness=40;care(2,now_ms);run_ms(100);active(PV_CLEAN);
    fresh();pet.food=25;run_ms(100);active(PV_HUNGRY);
    uint32_t starts=om_voice_starts;run_ms(2500);assert(!om_voice_busy());
    run_ms(2000);assert(om_voice_starts==starts); /* no every-frame spam */
    fresh();pet.energy=20;run_ms(100);active(PV_TIRED);
    fresh();ui.idle_delay=now_ms-21000u;ui.news_seated=1;
    ui.voice_news=1;ui.voice_news_due=now_ms;run_ms(100);active(PV_NEWS);
    assert(watching_news(now_ms));
    fresh();ui.idle_delay=now_ms-21000u;ui.news_seated=1;
    pet_last_snack=now_ms-45000u;run_ms(100);assert(ui.action_kind==6);
    assert(!om_voice_busy());run_ms(850);active(PV_EAT_TV);
    fresh();ui.map=1;run_ms(90000);ui.map=0;ui.idle_delay=now_ms;
    run_ms(100);assert(ui.action_kind==7);uint32_t began=ui.action_at;
    run_ms(600);assert(!om_voice_busy());run_ms(300);active(PV_POO);
    while(now_ms-began<2100u)run_ms(1);
    assert(!pet.poo_count && om_voice_starts>=1);
    run_ms(250);assert(pet.poo_count==1);active(PV_PERFECT);
    /* Artificially delay completion: Perfect must wait for the audio channel. */
    fresh();assert(om_voice_play(PV_EAT_FEED));run_ms(20);
    ui.voice_perfect_pending=1;run_ms(100);active(PV_EAT_FEED);
    run_ms(2000);active(PV_PERFECT);
    fresh();ui.map=1;run_ms(90000);ui.map=0;ui.idle_delay=now_ms;
    run_ms(1100);active(PV_POO);care(3,now_ms);run_ms(100);
    assert(pet.sleeping && !pet.poo_count && !ui.voice_perfect_pending && !om_voice_busy());
    run_ms(1900);active(PV_SNORE);
    care(3,now_ms);run_ms(100);assert(!pet.sleeping && !om_voice_busy());
    fresh();care(0,now_ms);run_ms(100);active(PV_EAT_FEED);
    uint32_t writes=store_writes;held_btn=1u<<B_SAVE;run_ms(40);held_btn=0;run_ms(40);
    assert(ui.pending_save && store_writes==writes);
    run_ms(2500);assert(!ui.pending_save && store_writes==writes+1);
    fresh();care(1,now_ms);run_ms(100);
    assert(!strcmp(caption_text(),"THANK YOU, THANK YOU"));
    run_ms(2400);care(1,now_ms);run_ms(100);
    assert(!strcmp(caption_text(),"THANK YOU VERY MUCH"));
    fresh();care(0,now_ms);run_ms(100);assert(!strcmp(caption_text(),"I LOVE BURGERS"));
    fresh();ui.idle_delay=now_ms-21000u;ui.news_seated=1;
    ui.voice_news=1;ui.voice_news_due=now_ms;run_ms(100);
    assert(!strcmp(caption_text(),"FAKE NEWS"));
    run_ms(900);assert(!strcmp(caption_text(),"NEWS IS UNFAIR"));
    fresh();pet.food=25;run_ms(100);assert(!strcmp(caption_text(),"I WANT FOOD"));
    fresh();pet.energy=20;run_ms(100);assert(!strcmp(caption_text(),"I WANT SLEEP"));
    fresh();ui.map=1;run_ms(90000);pet.energy=75;ui.map=0;ui.idle_delay=now_ms;
    run_ms(2450);assert(!strcmp(caption_text(),"PERFECT SPECIMEN"));
    run_ms(1100);assert(!strcmp(caption_text(),"A LITTLE ACCIDENT"));
    /* Idle need captions vary, while playing speech still says its exact words. */
    fresh();pet.food=25;run_ms(100);assert(!strcmp(caption_text(),"I WANT FOOD"));
    om_voice_stop();ui.caption_voice=0;pet_message=0;ui.message=0;
    uint32_t saved_clock=now_ms;now_ms=8000u;assert(!strcmp(caption_text(),"WHERE IS MY SNACK?"));
    now_ms=16000u;assert(!strcmp(caption_text(),"BRING ME A BURGER"));
    ui.caption_voice=PV_HUNGRY+1;assert(!strcmp(caption_text(),"I WANT FOOD"));
    ui.caption_voice=0;pet.food=75;pet.energy=20;
    assert(!strcmp(caption_text(),"I NEED MY BEAUTY SLEEP"));
    now_ms=saved_clock; /* keep the host audio scheduler on its monotonic clock */
    /* Footsteps use actual gait changes; seated TV and bed are quiet. */
    fresh();run_ms(2400);assert(idle_walking(now_ms) && idle_dx(now_ms)>0 && om_foot_hits>0);
    uint32_t feet=om_foot_hits;ui.idle_delay=now_ms-21000u;ui.news_seated=1;
    run_ms(1000);assert(om_foot_hits==feet);
    care(3,now_ms);run_ms(4000);feet=om_foot_hits;
    run_ms(1000);assert(pet.sleeping && om_foot_hits==feet);
    puts("Voice UI PASS: feed/snack distinction, praise alternates, refusal, low needs, random TV, crouch then Perfect, interruption and snore/wake.");
}
