/* HOME status and confirmed, persistent pet reset through real input edges. */
#define main scripted_host_main
#include "../../host/omni_host.c"
#undef main
#include <assert.h>
static void tap_button(int b) {held_btn=1u<<b;run_ms(40);held_btn=0;run_ms(40);}
static void choose_yes(void) {enc_acc[EN_SELECT]=1;run_ms(100);assert(ui.reset_yes);}
int main(void) {
    boot();run_ms(200);assert(!ui.highlight);
    const uint32_t care_lights=(1u<<1)|(1u<<3)|(1u<<5)|(1u<<8);
    const uint32_t button_lights=(1u<<B_GLO)|(1u<<B_HOME)|(1u<<B_SAVE)|(1u<<B_PLAY);
    assert(lit_keys==care_lights && lit_btn==button_lights && !om_triggers);
    held_keys=1u<<WHITE_K[0];run_ms(40);assert(lit_keys==(care_lights|1u));
    held_keys=0;run_ms(4000);assert(lit_keys==care_lights);
    boot();run_ms(200);
    tap_button(B_HOME);assert(ui.map && lit_keys==care_lights && lit_btn==button_lights);run_ms(120);png_write("build/pet/preview/home-status.png");
    tap_button(B_FX);assert(ui.map && !(lit_btn&(1u<<B_FX))); /* FX has no status shortcut. */
    tap_button(B_HOME);assert(!ui.map);
    int old_param=proj.par[P_REVERB];enc_acc[EN_K1]=10;run_ms(100);assert(proj.par[P_REVERB]==old_param);
    enc_acc[EN_SELECT]=-1;run_ms(100);assert(ui.selected==0 && !ui.highlight);
    tap_button(B_SEL);assert(ui.selected==0);
    /* Only the first four black keys care; all seven extras are inert. */
    ui.highlight=0;uint8_t before=pet.food;uint32_t voices=om_voice_starts;
    for(int i=4;i<11;i++){
        held_keys=1u<<BLACK_K[i];run_ms(40);held_keys=0;run_ms(40);
        assert(pet.food==before && !ui.map && !ui.highlight && !om_triggers && om_voice_starts==voices);
    }
    held_keys=1u<<BLACK_K[0];run_ms(40);held_keys=0;run_ms(40);
    assert(pet.food==100 && !ui.map && ui.highlight==1);
    run_ms(500);assert(!ui.highlight);
    enc_acc[EN_SELECT]=1;run_ms(100);assert(!ui.highlight);
    tap_button(B_PLAY);assert(ui.paused);tap_button(B_PLAY);assert(!ui.paused && !ui.highlight);
    pet.plumpness=3;pet.food=40;pet.mood=30;pet.energy=35;pet.snack_stock=0;pet.day_minute=1000;
    assert(!pet_save());uint32_t writes=store_writes;
    tap_button(B_GLO);assert(ui.reset_menu==1 && !ui.reset_yes && lit_keys==care_lights && lit_btn==(button_lights|(1u<<B_SEL)));
    png_write("build/pet/preview/start-over.png");
    tap_button(B_SEL);assert(!ui.reset_menu && pet.food==40 && store_writes==writes);
    tap_button(B_GLO);choose_yes();tap_button(B_HOME);assert(!ui.reset_menu && pet.food==40);
    tap_button(B_GLO);assert(!ui.reset_yes);choose_yes();tap_button(B_GLO);assert(!ui.reset_menu);
    tap_button(B_GLO);choose_yes();tap_button(B_PLAY);assert(ui.reset_menu==1 && pet.food==40);
    held_keys=(1u<<WHITE_K[0])|(1u<<BLACK_K[0]);run_ms(100);assert(!om_active_keys && pet.food==40);
    tap_button(B_SEL);assert(ui.reset_menu==2);run_ms(3200);assert(pet.food==40);
    held_keys=0;run_ms(100);assert(!ui.reset_menu && pet.food==100 && pet.mood==100);
    assert(pet.energy==100 && pet.cleanliness==100 && pet.plumpness==0 && pet.snack_stock==3 && pet.day_minute==540 && !pet.sleeping);
    assert(store_writes==writes+1 && !strcmp(ui.message,"A FRESH START"));
    boot();assert(pet.food==100 && pet.mood==100 && pet.energy==100 && pet.cleanliness==100 && pet.plumpness==0 && pet.day_minute==540 && pet.snack_stock==3);
    /* Failure is visible, retryable, and cannot falsely report a saved reset. */
    pet.food=42;assert(!pet_save());writes=store_writes;store_fail=1;
    tap_button(B_GLO);choose_yes();held_btn=1u<<B_SEL;run_ms(3300);
    assert(!ui.reset_menu && ui.selected==0 && pet.food==100 && store_writes==writes);
    assert(!strcmp(ui.message,"NEW PET NOT SAVED"));
    held_btn=0;run_ms(40);store_fail=0;tap_button(B_SAVE);run_ms(100);
    assert(store_writes==writes+1);boot();assert(pet.food==100);
    puts("Menu PASS: HOME status, four care actions, No/cancel, input isolation, quiet confirmed reset, reboot, held SEL, save failure/retry.");
    return 0;
}
