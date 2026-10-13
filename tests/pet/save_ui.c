/* Exercise real UI save scheduling and explicit failure/retry feedback. */
#define main scripted_host_main
#include "../../host/omni_host.c"
#undef main
#include <assert.h>
static void save_button(void){held_btn=1u<<B_SAVE;run_ms(40);held_btn=0;run_ms(40);}
int main(void){
    boot();run_ms(59900);assert(store_writes==0);
    run_ms(200);assert(store_writes==1 && ui.autosave_flash && !ui.message);
    png_write("build/pet/preview/autosave-header.png");
    run_ms(1900);assert(!ui.autosave_flash);
    uint8_t food=pet.food;boot();assert(pet.food==food);
    /* Loaded low energy can trigger a brief tired reaction; wait for quiet. */
    run_ms(1500);pet.food=50;save_button();assert(store_writes==2 && !strcmp(ui.message,"PET SAVED"));
    boot();assert(pet.food==50);
    held_keys=1u<<WHITE_K[0];run_ms(61000);assert(store_writes==2);
    save_button();assert(ui.pending_save && store_writes==2);
    held_keys=0;run_ms(4000);assert(store_writes==3 && !ui.pending_save);
    store_fail=1;pet.food=41;save_button();run_ms(1200);assert(store_writes==3 && !strcmp(ui.message,"SAVE FAILED"));
    store_fail=0;save_button();assert(store_writes==4 && !strcmp(ui.message,"PET SAVED"));
    boot();assert(pet.food==41);
    /* Manual SAVE must win over newly due ambient news/snore, not interrupt a current clip. */
    boot();pet_init(now_ms);ui_init();ui.idle_delay=now_ms-21000u;ui.news_seated=1;
    ui.voice_news=1;ui.voice_news_due=now_ms;pet.food=55;
    uint32_t writes=store_writes;held_btn=1u<<B_SAVE;run_ms(20);held_btn=0;
    assert(store_writes==writes+1 && !ui.pending_save && !strcmp(ui.message,"PET SAVED") && (lit_btn & (1u<<B_SAVE)));
    run_ms(1000);assert(!strcmp(caption_text(),"PET SAVED") && !om_voice_busy());
    run_ms(900);assert(lit_btn & (1u<<B_SAVE)); /* SAVE is now a steady control guide. */
    boot();assert(pet.food==55);
    pet_init(now_ms);ui_init();pet.day_minute=1260;pet.energy=10;pet.sleeping=1;
    ui.voice_sleeping=1;ui.voice_snore_at=now_ms-4000u;
    writes=store_writes;held_btn=1u<<B_SAVE;run_ms(20);held_btn=0;
    assert(store_writes==writes+1 && !ui.pending_save && !strcmp(ui.message,"PET SAVED"));
    boot();assert(pet.sleeping);
    puts("Save PASS: minute autosave, manual save, reboot, music deferral, visible failure and retry.");
}
