/* Mess appears only after a visible crouch; sleep and user input interrupt it. */
#define main scripted_host_main
#include "../../host/omni_host.c"
#undef main
#include <assert.h>
static void ready(void) {
    pet_init(now_ms);ui_init();ui.idle_delay=now_ms;ui.map=1;
    run_ms(90000);assert(pet_poo_ready() && !pet.poo_count);
    ui.map=0;ui.idle_delay=now_ms;run_ms(100);assert(ui.action_kind==7);
}
int main(void) {
    boot();ready();run_ms(1300);assert(!pet.poo_count);
    png_write("build/pet/preview/poo-crouch.png");
    run_ms(1000);assert(pet.poo_count==1 && ui.poo_done);
    run_ms(1300);assert(ui.action_kind==0);
    png_write("build/pet/preview/poo-floor.png");
    assert(!pet_save());boot();assert(pet.poo_count==1);
    pet.cleanliness=100;care(2,now_ms);assert(!pet.poo_count);run_ms(200);
    png_write("build/pet/preview/poo-clean.png");
    ready();run_ms(1200);care(3,now_ms);assert(pet.sleeping);
    run_ms(3000);assert(!pet.poo_count);
    ready();run_ms(1200);held_keys=1u<<WHITE_K[0];run_ms(100);
    assert(ui.action_kind!=7 && !pet.poo_count && om_active_keys);
    run_ms(3000);assert(!pet.poo_count);held_keys=0;run_ms(100);
    puts("Poo UI PASS: crouch before deposit, saved mess, CLEAN, sleep and keyboard interruption.");
}
