/* Render actual firmware scenes at selected clock phases for visual review. */
#define main scripted_host_main
#include "../../host/omni_host.c"
#undef main
int main(void) {
    boot(); run_ms(1000);
    png_write("build/pet/preview/room-day.png");
    pet.plumpness=3;run_ms(300);
    png_write("build/pet/preview/room-overfed.png");
    pet.plumpness=0;
    pet.day_minute=1140; run_ms(300);
    png_write("build/pet/preview/room-sunset.png");
    pet.day_minute=1260; run_ms(300);
    png_write("build/pet/preview/room-bedtime.png");
    pet_action(PET_REST,now_ms); run_ms(300);
    png_write("build/pet/preview/room-night.png");
    return 0;
}
