/* A short session must need a nap, recover, then reach bedtime tired. */
#define main scripted_host_main
#include "../../host/omni_host.c"
#undef main
#include <assert.h>
int main(void){
    boot();pet.food=pet.mood=pet.energy=pet.cleanliness=100;pet.day_minute=360;
    run_ms(100000);assert(pet.energy<=20 && pet.state==PET_TIRED && !pet.sleeping);
    care(3,now_ms);assert(pet.sleeping && (ui.action_kind==4 || ui.action_kind==10));
    run_ms(12000);assert(pet.sleeping && pet.energy>=42 && pet.energy<=45);
    run_ms(24000);assert(!pet.sleeping && pet.energy>=85 && pet.energy<=90);
    pet.day_minute=1260;run_ms(300);assert(pet.energy<=20 && pet.state==PET_TIRED);
    care(3,now_ms);assert(pet.sleeping);run_ms(8000);
    assert(!pet.sleeping && pet.day_minute>=360 && pet.day_minute<400 && pet.energy>=99);
    boot();pet_init(now_ms);ui_init();ui.idle_delay=now_ms;pet.food=pet.energy=pet.cleanliness=100;pet.mood=60;pet.day_minute=540;
    held_keys=1u<<WHITE_K[0];run_ms(12000);
    assert(om_bongo_active && pet.mood>=64); /* initial note plus sustained bongo rewards */
    held_keys=0;run_ms(4000);assert(!om_bongo_active);
    int mood=pet.mood;run_ms(1000);assert(pet.mood==mood);
    puts("Energy UI PASS: daytime fatigue, useful nap recovery, low energy at bedtime, fast-night recovery/dawn.");
}
