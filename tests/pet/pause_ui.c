/* Pause is a live-session freeze, with safe audio cancellation and input edges. */
#define main scripted_host_main
#include "../../host/omni_host.c"
#undef main
#include <assert.h>
static void tap(int b){held_btn=1u<<b;run_ms(40);held_btn=0;run_ms(40);}
int main(void){
    boot();run_ms(100);held_keys=1u<<WHITE_K[0];run_ms(100);
    tap(B_PLAY);assert(ui.paused);run_ms(200);
    assert(!om_active_keys && !om_bongo_active && !om_voice_busy() && !om_foot_busy());
    pet_t frozen=pet;uint32_t action_age=now_ms-ui.action_at;
    run_ms(120000);assert(!memcmp(&pet,&frozen,sizeof pet) && now_ms-ui.action_at==action_age);
    /* Care/knobs/MIDI cannot progress a paused pet or leak into resume. */
    held_keys=1u<<BLACK_K[0];enc_acc[EN_SELECT]=1;run_ms(100);assert(!memcmp(&pet,&frozen,sizeof pet));
    held_keys=0;tap(B_SEL);assert(!ui.highlight && !(lit_btn&(1u<<B_SEL)));
    uint32_t writes=store_writes;tap(B_SAVE);run_ms(100);assert(store_writes==writes+1 && !strcmp(ui.message,"PET SAVED"));
    tap(B_PLAY);assert(!ui.paused);run_ms(100);assert(pet.food==frozen.food && pet.energy>=frozen.energy-1);
    held_keys=1u<<WHITE_K[0];run_ms(50);assert(om_active_keys);held_keys=0;run_ms(3200);
    /* Sleeping and TV activity freeze too; resuming cannot catch up a night. */
    pet_init(now_ms);ui_init();pet.day_minute=1260;pet.energy=10;care(3,now_ms);run_ms(1900);
    tap(B_PLAY);frozen=pet;run_ms(60000);assert(!memcmp(&pet,&frozen,sizeof pet));
    tap(B_PLAY);
    for(int i=0;i<8000 && pet.sleeping;i++)run_ms(1);
    assert(!pet.sleeping && pet.energy==100 && !pet_is_night());
    pet_init(now_ms);ui_init();ui.idle_delay=now_ms-21000u;ui.news_seated=1;run_ms(10);
    tap(B_PLAY);frozen=pet;run_ms(60000);assert(!memcmp(&pet,&frozen,sizeof pet));
    tap(B_PLAY);run_ms(900);assert(watching_news(now_ms) && pet.mood==frozen.mood-1);
    png_write("build/pet/preview/resumed-news.png");
    tap(B_PLAY);png_write("build/pet/preview/paused.png");
    /* Unsigned rebasing also survives the hardware millisecond wrap. */
    now_ms=UINT32_MAX-200u;pet_init(now_ms);ui_init();
    held_btn=1u<<B_PLAY;ui_input_only();held_btn=0;ui_input_only();frozen=pet;
    now_ms+=1000u;ui_input_only();assert(ui.paused && !memcmp(&pet,&frozen,sizeof pet));
    held_btn=1u<<B_PLAY;ui_input_only();held_btn=0;ui_input_only();now_ms+=100u;ui_input_only();
    assert(!ui.paused && pet.food==frozen.food);
    puts("Pause PASS: silent cancellation, frozen needs/clock/actions, ignored care, quiet save, fresh notes, TV/night recovery and clock wrap.");
}
