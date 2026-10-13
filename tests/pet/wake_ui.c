/* Dawn changes the room before waking movement; furniture exits are visible. */
#define main scripted_host_main
#include "../../host/omni_host.c"
#undef main
#include <assert.h>
int main(void) {
    boot();pet_init(now_ms);ui_init();ui.idle_delay=now_ms;
    pet.day_minute=1260;pet.energy=10;care(3,now_ms);assert(pet.sleeping);
    run_ms(1900);png_write("build/pet/preview/bed-direct-tucked.png");
    assert(ui.action_kind==4 && pet.sleeping);
    for(int i=0;i<8000 && pet.sleeping;i++)run_ms(1);
    assert(!pet.sleeping && !pet_is_night() && pet.energy==100 && ui.action_kind==9);
    run_ms(200);png_write("build/pet/preview/wake-daylight.png");
    run_ms(1200);assert(ui.action_kind==9);png_write("build/pet/preview/wake-walking.png");
    run_ms(1800);assert(!ui.action_kind && !watching_news(now_ms));
    png_write("build/pet/preview/wake-standing.png");
    pet_init(now_ms);ui_init();ui.idle_delay=now_ms;run_ms(21000);
    assert(watching_news(now_ms));
    care(1,now_ms);assert(ui.action_kind==10 && ui.after_chair==2);
    run_ms(250);png_write("build/pet/preview/chair-rising.png");
    run_ms(1350);assert(ui.action_kind==2);run_ms(2300);
    run_ms(2200);assert(watching_news(now_ms));
    care(3,now_ms);assert(pet.sleeping && ui.action_kind==10 && ui.after_chair==4);
    run_ms(1600);assert(ui.action_kind==4 && pet.sleeping);
    run_ms(3100);assert(!ui.action_kind && pet.sleeping);
    care(3,now_ms);assert(!pet.sleeping && ui.action_kind==9);
    run_ms(3100);assert(!ui.action_kind);
    pet_init(now_ms);ui_init();ui.idle_delay=now_ms;run_ms(30500);
    assert(ui.action_kind==10 && !watching_news(now_ms));
    run_ms(1600);assert(!ui.action_kind);
    puts("Furniture exits PASS: daylight before wake, direct tuck-in, wake walk, chair exit before care/rest, natural chair exit.");
}
