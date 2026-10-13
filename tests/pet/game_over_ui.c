/* Exhaustion is a recoverable screen, not a death animation. */
#define main scripted_host_main
#include "../../host/omni_host.c"
#undef main
#include <assert.h>
static void empty(void){pet.food=pet.mood=pet.energy=0;}
static void full(void){assert(pet.food==100 && pet.mood==100 && pet.energy==100 && pet.cleanliness==100);assert(pet.snack_stock==3 && pet.plumpness==0 && !pet.poo_count && pet.day_minute==540 && !pet.sleeping);}
int main(void){
    boot();run_ms(100);
    /* Any remaining visible meter prevents game over, even with cleanliness zero. */
    for(int meter=0;meter<3;meter++){
        pet_init(now_ms);empty();pet.cleanliness=0;
        if(meter==0)pet.food=10;else if(meter==1)pet.mood=10;else pet.energy=10;
        run_ms(50);assert(!ui.game_over);
    }
    pet_init(now_ms);pet.food=pet.mood=0;pet.energy=10;pet.cleanliness=100;
    proj.par[P_REVERB]=23;proj.par[P_DELAY]=17;project_apply();
    held_keys=1u<<WHITE_K[0];run_ms(50);empty();run_ms(100);
    assert(pet.cleanliness==100 && ui.game_over==1 && om_game_over_busy() && om_game_over_starts==1);
    uint16_t minute=pet.day_minute;uint32_t writes=store_writes;
    run_ms(1500);assert(ui.game_over==1 && !om_game_over_busy());
    assert(!om_voice_busy() && !om_active_keys && !om_bongo_active && !om_foot_busy());
    assert(store_writes==writes+1);png_write("build/pet/preview/game-over.png");
    run_ms(10000);assert(ui.game_over==1 && pet.day_minute==minute && om_game_over_starts==1);
    held_keys=0;run_ms(50);boot();run_ms(1500);assert(ui.game_over==1 && om_game_over_starts==1);
    held_btn=1u<<B_HOME;run_ms(20);assert(!ui.game_over && !ui.map);full();
    assert(proj.par[P_REVERB]==23 && proj.par[P_DELAY]==17);
    assert(!om_active_keys && !ui.reset_menu);held_btn=0;run_ms(10);
    boot();full();run_ms(20);assert(!ui.game_over);
    /* A keyboard press also restarts, consumes that note, and can cancel the sound. */
    empty();run_ms(100);assert(ui.game_over && om_game_over_busy());
    held_keys=1u<<WHITE_K[0];run_ms(20);assert(!ui.game_over && !om_active_keys);full();
    held_keys=0;run_ms(10);held_keys=1u<<WHITE_K[0];run_ms(30);assert(om_active_keys);
    held_keys=0;run_ms(4000);empty();run_ms(1500);store_fail=1;
    held_btn=1u<<B_SAVE;run_ms(20);assert(!ui.game_over);full();
    assert(!strcmp(ui.message,"NEW PET NOT SAVED"));held_btn=0;run_ms(10);store_fail=0;
    held_btn=1u<<B_SAVE;run_ms(20);assert(!strcmp(ui.message,"PET SAVED"));
    held_btn=0;boot();full();
    puts("Game over PASS: three visible meters, cleanliness independent, one-shot sound, frozen clock, held-key isolation, persisted screen, full restart, consumed press, save failure/retry.");
    return 0;
}
