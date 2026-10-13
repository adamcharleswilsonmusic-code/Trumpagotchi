/* Praise must respond visually without bypassing care reward limits. */
#define main scripted_host_main
#include "../../host/omni_host.c"
#undef main
#include <assert.h>
int main(void) {
    boot();run_ms(200);
    care(1,now_ms);assert(pet.mood==85 && ui.action_kind==2);
    run_ms(2300);assert(ui.action_kind==0);
    care(1,now_ms);assert(pet.mood==85 && ui.action_kind==2);
    run_ms(600);uint32_t started=ui.action_at;
    care(1,now_ms);assert(ui.action_at==started && pet.mood==85);
    run_ms(1700);assert(ui.action_kind==0);
    pet.mood=95;care(1,now_ms);assert(pet.mood==95 && ui.action_kind==2);
    run_ms(2300);pet.food=10;care(1,now_ms);assert(ui.action_kind==0 && pet.mood==95);
    pet.food=75;pet.sleeping=1;care(1,now_ms);assert(ui.action_kind==0 && pet.sleeping);
    pet.sleeping=0;pet.energy=10;care(1,now_ms);assert(ui.action_kind==0);
    pet.energy=75;pet.cleanliness=10;care(1,now_ms);assert(ui.action_kind==0);
    pet.cleanliness=80;pet.day_minute=1260;care(1,now_ms);assert(ui.action_kind==0);
    puts("Praise PASS: capped/cooldown reactions, no extra rewards, no restart spam, needs/sleep priority.");
    return 0;
}
