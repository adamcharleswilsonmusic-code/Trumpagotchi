/* News mood follows the actual visible room activity, not a background timer. */
#define main scripted_host_main
#include "../../host/omni_host.c"
#undef main
#include <assert.h>
int main(void) {
    boot();run_ms(17000);assert(pet.mood==70);
    run_ms(1400);assert(ui.action_kind==8 && !watching_news(now_ms));
    assert(pet.mood==70);png_write("build/room-preview/chair-approach.png");
    run_ms(850);assert(ui.action_kind==8 && !watching_news(now_ms));
    png_write("build/room-preview/chair-settle.png");
    run_ms(1000);assert(watching_news(now_ms) && ui.news_seated);
    png_write("build/room-preview/chair-seated.png");
    run_ms(1000);assert(pet.mood==69);
    care(1,now_ms);assert(pet.mood==84 && ui.action_kind==10);
    run_ms(1600);assert(pet.mood==84 && ui.action_kind==2);
    run_ms(2300);assert(ui.action_kind==8 && !watching_news(now_ms));
    run_ms(2000);assert(watching_news(now_ms) && pet.mood==84);
    held_keys=1;ui_input_only();int mood=pet.mood;
    run_ms(2700);assert(pet.mood==mood-3 && watching_news(now_ms));
    assert(om_bongo_hits==0 && om_active_keys!=0);
    held_keys=0;ui_input_only();run_ms(3200);
    care(3,now_ms);assert(pet.sleeping);mood=pet.mood;
    run_ms(3000);assert(pet.mood==mood && !watching_news(now_ms));
    pet.day_minute=1300;run_ms(300);png_write("build/room-preview/bed-night.png");
    boot();
    for(int i=0;i<90000 && ui.action_kind!=6;i++)run_ms(1);
    assert(ui.action_kind==6 && pet.snack_stock==2);
    int food=pet.food; mood=pet.mood;
    run_ms(2100);assert(pet.food==food && pet.mood<mood);
    /* An uninterrupted visit now finishes after ten visible seconds, still
       costing about eleven mood points (the old seventeen-second total). */
    boot();pet_init(now_ms);ui_init();pet.food=pet.energy=pet.cleanliness=pet.mood=100;
    ui.idle_delay=now_ms-18000u;run_ms(2200);assert(watching_news(now_ms));
    run_ms(10000);assert(!watching_news(now_ms) && pet.mood==89);
    puts("News UI PASS: mood tracks TV, praise/sleep interrupt, keyboard stays solo during news, snacking retains mood drain.");
    return 0;
}
