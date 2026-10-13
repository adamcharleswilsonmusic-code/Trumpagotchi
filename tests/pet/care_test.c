/* SPDX-License-Identifier: GPL-3.0-only */
/* Actual care engine and project persistence with bounded in-memory platform. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../firmware/src/app/app.h"
static uint32_t test_now;
static uint8_t disk[64];
static int disk_len = -1, writes, fail_write, applied[P_NPARAMS];
uint32_t plat_ms(void) { return test_now; }
int plat_store_load(uint32_t obj, void *dst, uint32_t max)
{
    assert(obj == OBJ_PROJ); /* calibration must never be read or written */
    if (disk_len < 0) return -1;
    int n = disk_len < (int)max ? disk_len : (int)max;
    memcpy(dst, disk, (size_t)n);
    return n;
}
int plat_store_save(uint32_t obj, const void *src, uint32_t len)
{
    assert(obj == OBJ_PROJ && len <= sizeof disk);
    writes++;
    if (fail_write) return -9;
    memcpy(disk, src, len);
    disk_len = (int)len;
    return 0;
}
const om_param_t *om_param_info(int p)
{
    static const om_param_t par[] = {{"REV",0,100,10},{"DEL",0,100,20},{"TIME",1,1000,300},{"FB",0,95,30}};
    assert(p >= 0 && p < P_NPARAMS);
    return &par[p];
}
void om_set(int p, int value) { applied[p] = value; }
#include "../../firmware/src/app/project.c"

static void pacing_and_states(void)
{
    pet_init(123); pet.day_minute = 360; /* daytime fixture for care-state priority */
    pet_tick(123 + 59999);
    assert(pet.food == 75 && pet.mood == 70 && pet.energy == 28);
    pet_tick(123 + 60000);
    assert(pet.food == 61 && pet.mood == 65 && pet.energy == 27 && pet.cleanliness == 79);
    pet_tick(123 + 6u * 60000u);
    assert(pet.food == 0 && pet.mood == 35 && pet.state == PET_HUNGRY);
    pet_tick(123 + 7u * 60000u);
    assert(pet.food == 0 && pet.state == PET_HUNGRY);
    pet_init(0);
    pet.food = 70; pet.energy = 10; pet.neglect = 30;
    pet_tick(0); assert(pet.state == PET_TIRED);
    pet.energy = 70; pet_tick(0); assert(pet.state == PET_SULKING);
    pet.neglect = 0; pet.mood = 40; pet_tick(0); assert(pet.state == PET_BORED);
    pet.mood = 70; pet_tick(0); assert(pet.state == PET_HAPPY);
    pet.cleanliness = 20; pet.day_minute = 480;
    /* Keep food and energy satisfied to isolate accumulating neglect from dirty surroundings. */
    for (uint32_t i = 1; i <= 11; i++) { pet.food = pet.energy = pet.mood = 100; pet_tick(i * 60000u); }
    assert(pet.neglect == 11 && pet.state == PET_BORED);
    pet.energy = 100;
    pet_tick(12u * 60000u); assert(pet.neglect == 12 && pet.state == PET_SULKING);
    pet.food = pet.cleanliness = 80; pet.energy = 100; pet.day_minute = 360;
    pet_tick(13u * 60000u); assert(pet.neglect == 11);
    assert(strlen(pet_state_name()) <= 7 && strlen(pet_feedback()) <= 25);
}
static void care_and_music(void)
{
    pet_init(0);
    assert(pet_action(PET_FEED, 0)); assert(pet.food == 100 && pet.cleanliness == 77);
    assert(!pet_action(PET_FEED, 1) && pet.food == 100);
    pet_tick(120000); pet.energy = 70;
    pet.food = 50; assert(pet_action(PET_FEED, 120000)); assert(pet.food == 75);
    assert(pet_action(PET_PRAISE, 120000));
    int mood = pet.mood;
    for (int i = 0; i < 1000; i++) assert(!pet_action(PET_PRAISE, 120001));
    assert(pet.mood == mood);
    pet.food = 25; assert(!pet_action(PET_PRAISE, 240000));
    pet.food = 70; pet.energy = 10; assert(!pet_action(PET_PRAISE, 240000));
    pet.energy = 70; pet.cleanliness = 20; assert(!pet_action(PET_PRAISE, 240000));
    assert(pet_action(PET_CLEAN, 240000) && pet.cleanliness == 50);
    assert(!pet_action(PET_CLEAN, 240001));
    assert(!pet_action((enum pet_action_t)99, 240001));
    pet_init(0);
    pet_note(0); assert(pet.mood == 72 && pet.energy == 74);
    for (int i = 0; i < 10000; i++) pet_note(10);
    assert(pet.mood == 72 && pet.energy == 74);
    pet_tick(59999); pet.energy = 70;
    pet_note(60000); assert(pet.mood == 69 && pet.energy == 68);
    pet.food = 20; mood = pet.mood; pet_tick(60000);
    assert(!pet_can_dance()); pet_note(60001); assert(pet.mood == mood);
    pet_init(0); pet.mood = 40; pet_tick(0);
    assert(pet.state == PET_BORED && !pet_can_dance());
    pet_note(0); assert(pet.mood == 42 && pet.state == PET_HAPPY && pet_can_dance());
}
static void rest_and_elapsed(void)
{
    pet_init(0); pet.energy = 20;
    assert(pet_action(PET_REST, 0) && pet.sleeping && !pet_can_dance());
    assert(!pet_action(PET_FEED, 0));
    pet_tick(1000); assert(pet.energy == 22 && pet.food == 75 && pet.mood == 70);
    pet_tick(7500); assert(pet.energy == 35 && pet.mood == 71);
    pet_tick(35000);
    assert(pet.energy == 90 && pet.food == 75 && pet.mood == 74 && !pet.sleeping && !pet.rest_minutes);
    assert(!pet_action(PET_REST, 35000));
    pet_init(0); pet.mood = 98; pet.energy = 20;
    assert(pet_action(PET_REST, 0));
    pet_tick(15000); assert(pet.mood == 100); /* sleep reward saturates safely */
    pet_init(0); assert(pet_action(PET_REST, 0));
    assert(pet_action(PET_REST, 1) && !pet.sleeping && pet.energy == 75);
    pet_tick(4000000000u);
    assert(!pet.food && !pet.mood && !pet.energy && !pet.cleanliness && pet.neglect == 60);
    /* Catch-up must equal regular ticks, including automatic waking. */
    pet_init(0); pet.energy = 1; pet_action(PET_REST, 0);
    for (uint32_t i = 1; i <= 200; i++) pet_tick(i * 60000u);
    pet_t expected = pet;
    pet_init(0); pet.energy = 1; pet_action(PET_REST, 0); pet_tick(200u * 60000u);
    assert(!memcmp(&pet, &expected, sizeof pet));
    assert(!strcmp(pet_feedback(), "I WANT FOOD"));
    /* Faster awake decay must also catch up exactly before any meter bottoms out. */
    pet_init(0);
    for (uint32_t i = 1; i <= 6; i++) pet_tick(i * 60000u);
    expected = pet;
    pet_init(0); pet_tick(6u * 60000u);
    assert(!memcmp(&pet, &expected, sizeof pet));
    assert(pet.food == 0 && pet.mood == 35 && pet.energy == 0 && pet.cleanliness == 74);
}
static void wraparound(void)
{
    uint32_t start = UINT32_MAX - 10000u;
    pet_init(start); pet_note(start); assert(pet.mood == 72);
    pet_tick(start + 59999u); assert(pet.food == 75); pet.energy = 70;
    pet_note(start + 60000u); assert(pet.food == 61 && pet.mood == 69);
    pet_init(start); pet.food = 20;
    assert(pet_action(PET_FEED, start));
    assert(!pet_action(PET_FEED, start + 29999u));
    assert(pet_action(PET_FEED, start + 30000u));
}
static void persistence(void)
{
    disk_len = -1; writes = 0; test_now = 0;
    assert(!project_load() && pet_needs_save());
    project_apply(); assert(applied[2] == 300);
    pet.food = 38; pet.neglect = 15; pet.mood = 8; pet.energy = 12;
    pet_action(PET_REST, 0); pet_tick(60000);
    proj.par[0] = 77;
    assert(!pet_save() && writes == 1 && !pet_needs_save());
    assert(!pet_save() && writes == 1); /* no redundant write */
    pet_t expected = pet;
    test_now = 1234567890u;
    assert(project_load() && !memcmp(&pet, &expected, sizeof pet));
    pet_tick(test_now); assert(!memcmp(&pet, &expected, sizeof pet)); /* no offline decay */
    project_apply(); assert(applied[0] == 77);
    pet_tick(test_now + 60000); assert(pet.energy < expected.energy);
    assert(pet_needs_save());
    fail_write = 1; assert(pet_save() == -9 && pet_needs_save());
    fail_write = 0; assert(!pet_save() && !pet_needs_save());
    uint8_t good[30]; memcpy(good, disk, 30);
    const unsigned bad_offsets[] = {0,4,5,6,7,8,16,17,18,19,20,21,22,23,25,26,27,28,29};
    for (unsigned i = 0; i < sizeof bad_offsets / sizeof bad_offsets[0]; i++) {
        memcpy(disk, good, 30); disk[bad_offsets[i]] = 255;
        assert(!project_load() && pet.food == 75 && proj.par[0] == 10);
    }
    memcpy(disk, good, 30); disk[20] = 0; disk[22] = 1;
    assert(!project_load());
    memcpy(disk, good, 30); disk_len = 23; assert(!project_load());
    disk_len = 25; assert(!project_load());
    disk_len = 40; assert(!project_load());
    disk_len = 24; assert(!project_load()); /* v5 header with a truncated payload */
    disk_len = 26; assert(!project_load());
    disk_len = 27; assert(!project_load());
    disk_len = 28; assert(!project_load());
    disk_len = 29; assert(!project_load());
    disk_len = 30; assert(project_load());
    disk[24] = 160; disk[25] = 5; assert(!project_load()); /* 1440 is invalid */
    memcpy(disk, good, 30); disk[26] = 4; assert(!project_load());
    memcpy(disk, good, 30); disk[28] = 4; assert(!project_load());
    memcpy(disk, good, 30); disk[29] = 4; assert(!project_load());
    memcpy(disk, good, 30); disk_len = 24; disk[4] = 1; disk[6] = 24;
    assert(project_load() && pet.day_minute == 540 && pet.food == good[16]);
    assert(pet.energy == good[18] && pet.sleeping == good[20] && proj.par[0] == 77 && pet.snack_stock == 3);
    assert(pet.plumpness == 0);
    assert(pet_needs_save());
    assert(!pet_save() && disk_len == 30 && disk[4] == 5 && !pet_needs_save());
    memcpy(disk, good, 30); disk_len = 26; disk[4] = 2; disk[6] = 26;
    assert(project_load() && pet.day_minute == (good[24] | (uint16_t)good[25] << 8));
    assert(pet.snack_stock == 3 && pet.food == good[16] && proj.par[0] == 77 && pet_needs_save());
    assert(pet.plumpness == 0);
    disk[24] = 160; disk[25] = 5; assert(!project_load()); /* legacy v2 clock is still validated */
    disk[24] = good[24]; disk[25] = good[25]; assert(project_load());
    assert(!pet_save() && disk_len == 30 && disk[4] == 5);
    memcpy(disk, good, 30); disk_len = 28; disk[4] = 3; disk[6] = 28; disk[26] = 0;
    assert(project_load() && pet.snack_stock == 0 && pet.plumpness == 0 && pet_needs_save());
    assert(pet.food == good[16] && pet.day_minute == (good[24] | (uint16_t)good[25] << 8));
    assert(!pet_save() && disk_len == 30 && disk[4] == 5);
    memcpy(disk, good, 30); disk[4] = 4; disk[29] = 0; disk[28] = 2;
    assert(project_load() && pet.poo_count == 0 && pet.plumpness == 2 && pet_needs_save());
    disk[29] = 1; assert(!project_load()); /* v4 still requires its reserved byte to be zero */
    disk[29] = 0; assert(project_load());
    assert(!pet_save() && disk[4] == 5);
    pet.snack_stock = 0;
    pet.plumpness = 3;
    pet.poo_count = 3;
    pet.day_minute = 1430; pet.sleeping = 1; pet.rest_minutes = 0;
    pet_tick(test_now);
    assert(!pet_save());
    expected = pet; test_now += 200000000u;
    assert(project_load() && !memcmp(&pet, &expected, sizeof pet) && pet.snack_stock == 0);
    assert(pet.plumpness == 3 && pet.poo_count == 3 && !pet_poo_ready());
    pet_tick(test_now + 13u);
    assert(pet.day_minute == 1430 && pet.sleeping); /* clock paused while off */
    pet_tick(test_now + 14u); assert(pet.day_minute == 1431 && pet.sleeping);
    pet_tick(test_now + 625u); assert(pet.day_minute == 38 && pet.sleeping);
}
static void day_and_night(void)
{
    pet_init(123);
    assert(pet.day_minute == 540 && !strcmp(pet_day_phase(), "MORNING"));
    pet_tick(331); assert(pet.day_minute == 540);
    pet_tick(332); assert(pet.day_minute == 541 && pet.food == 75);
    pet_tick(540); assert(pet.day_minute == 542 && pet.food == 75);
    pet_tick(748); assert(pet.day_minute == 543);
    pet_tick(300122); assert(pet.day_minute == 539);
    pet_tick(300123); assert(pet.day_minute == 540); /* exact five-minute day */
    pet_tick(900123); assert(pet.day_minute == 540); /* no accumulated clock drift */
    pet_init(0); pet.day_minute = 1259; pet.energy = 100;
    pet_tick(208); assert(!pet_is_night() && !strcmp(pet_day_phase(), "EVENING"));
    pet_tick(209); assert(pet_is_night() && pet.state == PET_TIRED && !pet_can_dance());
    assert(!strcmp(pet_day_phase(), "NIGHT"));
    assert(pet_action(PET_REST, 209)); /* bedtime allowed even at full energy */
    pet_tick(7240); assert(pet.sleeping && pet.day_minute == 359 && pet.energy == 59);
    pet_tick(7241); assert(!pet.sleeping && pet.day_minute == 360 && pet.energy == 100);
    assert(!pet_is_night() && !strcmp(pet_day_phase(), "MORNING"));
    pet_init(0); pet.day_minute = 1300; pet.food = 20;
    pet_tick(0); assert(pet.state == PET_HUNGRY); /* hunger outranks bedtime */
    pet.food = 70; pet_tick(0); assert(pet.state == PET_TIRED);
    int mood = pet.mood; pet_note(0); assert(pet.mood == mood);
    /* Crossing sunset turns an ordinary nap into overnight sleep. */
    pet_init(0); pet.day_minute = 1259; pet.energy = 75;
    assert(pet_action(PET_REST, 0));
    pet_tick(1000); assert(pet.sleeping && pet.mood == 74);
    pet_tick(7241);assert(!pet.sleeping && pet.day_minute==360 && pet.energy==100);
    /* A late bedtime also returns full energy at dawn; daytime naps still wake at90. */
    pet_init(0);pet.day_minute=1799u%1440u;pet.energy=1;
    assert(pet_action(PET_REST,0));pet_tick(14);assert(!pet.sleeping && pet.energy==100);
    /* Frequent ticks and a stalled loop must agree across midnight and dawn. */
    pet_t expected;
    pet_init(0); pet.day_minute = 1300; pet.energy = 1; pet_action(PET_REST, 0);
    for (uint32_t t = 250; t <= 1100000u; t += 250) pet_tick(t);
    expected = pet;
    pet_init(0); pet.day_minute = 1300; pet.energy = 1; pet_action(PET_REST, 0);
    pet_tick(1100000u); assert(!memcmp(&pet, &expected, sizeof pet));
    pet_init(0); pet_tick(4000000000u);
    assert(pet.day_minute == (540u + 19200000u) % 1440u);
    uint32_t start = UINT32_MAX - 500u;
    pet_init(start); pet_tick(start + 1499u); assert(pet.day_minute == 547);
    pet_tick(start + 1500u); assert(pet.day_minute == 547);
    pet_tick(start + 1667u); assert(pet.day_minute == 548);
    pet_tick(start + 2000u); assert(pet.day_minute == 549);
}
static void news_and_praise(void)
{
    pet_init(0);
    pet_watch_news(1, 0);
    pet_watch_news(1, 899); assert(pet.mood == 70);
    pet_watch_news(1, 900); assert(pet.mood == 69);
    assert(!strcmp(pet_feedback(), "NEWS IS UNFAIR"));
    for (uint32_t t = 901; t < 1800; t++) pet_watch_news(1, t);
    assert(pet.mood == 69); /* frame frequency cannot increase drain */
    assert(pet_action(PET_PRAISE, 1799));
    pet_watch_news(1, 1800); assert(pet.mood == 83);
    assert(!strcmp(pet_feedback(), "FINALLY, SOME RESPECT")); /* no repeated complaint */
    pet_watch_news(1, 24600); assert(pet.mood == 58 && pet.state == PET_HAPPY);
    pet_watch_news(1, 25200); assert(pet.mood == 57); /* fractional interval retained */
    assert(pet.food == 75 && pet.energy == 74 && pet.cleanliness == 80 && !pet.neglect);
    pet_watch_news(0, 25800); pet_watch_news(0, 600000);
    assert(pet.mood == 57);
    pet_watch_news(1, 1200000); assert(pet.mood == 57); /* fresh start, no inactive catch-up */
    pet_watch_news(1, 1200899); assert(pet.mood == 57);
    pet_watch_news(1, 1200900); assert(pet.mood == 56);
    pet_watch_news(1, 1202700); assert(pet.mood == 54);
    pet_watch_news(1, 2400000000u); assert(pet.mood == 25); /* bounded arithmetic catch-up */
    pet_watch_news(1, 2400001200u); assert(pet.mood == 25);
    pet.mood = 10;
    pet_watch_news(1, 2400002400u); assert(pet.mood == 10); /* floor never heals */
    pet_init(0); pet_watch_news(1, 0); pet.sleeping = 1;
    pet_watch_news(1, 1800); pet_watch_news(1, 5400); assert(pet.mood == 70);
    pet.sleeping = 0; pet_watch_news(1, 12000); assert(pet.mood == 70);
    pet_watch_news(1, 13200); assert(pet.mood == 69);
    uint32_t start = UINT32_MAX - 1000u;
    pet_init(start); pet_watch_news(1, start);
    pet_watch_news(1, start + 899u); assert(pet.mood == 70);
    pet_watch_news(1, start + 900u); assert(pet.mood == 69);
    pet_init(start); pet.mood = 40;
    assert(pet_action(PET_PRAISE, start) && pet.mood == 55);
    assert(!pet_action(PET_PRAISE, start + 14999u) && pet.mood == 55);
    assert(pet_action(PET_PRAISE, start + 15000u) && pet.mood == 70);
    pet_init(0); pet.food = 20; pet.cleanliness = 20;
    assert(pet_action(PET_FEED, 0) && pet_action(PET_CLEAN, 0));
    assert(pet_action(PET_FEED, 30000) && !pet_action(PET_CLEAN, 30000));
    assert(pet_action(PET_FEED, 120000) && pet_action(PET_CLEAN, 120000));
    pet_init(0); pet.mood = 85;
    assert(pet_action(PET_PRAISE, 0) && pet.mood == 100);
    assert(!pet_action(PET_PRAISE, 15000));
}
static void frequent_feeding(void)
{
    pet_init(0);pet.food=60;
    assert(pet_action(PET_FEED,0) && pet.food==85);
    assert(!pet_action(PET_FEED,29999));
    assert(pet_action(PET_FEED,30000) && pet.food==100);
    assert(!pet_action(PET_FEED,59999));
    assert(pet_action(PET_FEED,60000) && pet.food==100);
    pet_init(0);pet.food=95;assert(pet_action(PET_FEED,0) && pet.food==100 && pet.plumpness==1);
    pet_init(0);pet.food=96;assert(pet_action(PET_FEED,0) && pet.plumpness==1);
}
static void stocked_snacks(void)
{
    pet_init(0); assert(pet.snack_stock == 3);
    assert(!pet_take_snack(44999) && pet.food == 75 && pet.snack_stock == 3);
    assert(pet_take_snack(45000));
    assert(pet.food == 83 && pet.mood == 71 && pet.cleanliness == 72 && pet.energy == 42 && pet.snack_stock == 2);
    assert(!strcmp(pet_feedback(), "A LITTLE SIDE SNACK"));
    assert(!pet_take_snack(45001));
    assert(pet_take_snack(90000) && pet.snack_stock == 1);
    assert(pet_take_snack(135000) && pet.snack_stock == 0);
    assert(!pet_take_snack(180000) && pet.snack_stock == 0);
    assert(pet_action(PET_FEED, 180000) && pet.food == 82 && pet.snack_stock == 3);
    pet.food = 50; pet.day_minute = 540; /* isolate refill timing from nighttime eligibility */
    assert(!pet_take_snack(224999)); /* refill restarts 45-second interval */
    assert(pet_take_snack(225000) && pet.food == 58 && pet.snack_stock == 2);
    pet.food=100;
    assert(pet_action(PET_FEED, 225000) && pet.snack_stock == 3 && pet.plumpness == 1);
    pet.snack_stock = 2;
    assert(!pet_action(PET_FEED, 225001) && pet.snack_stock == 2); /* cooldown refusal does not refill */
    pet_init(0); pet.food = 100;
    assert(!pet_take_snack(45000) && pet.snack_stock == 3); /* only when food <=75 */
    pet_init(0); pet_tick(44999); pet.energy = 10; assert(pet_action(PET_REST, 44999));
    assert(!pet_take_snack(45000) && pet.snack_stock == 3);
    pet_init(0); pet.day_minute = 1300;
    assert(!pet_take_snack(45000) && pet.snack_stock == 3); /* no nighttime grazing */
    pet_init(0); pet.food = 20; pet.mood = 100; pet.cleanliness = 3;
    assert(pet_take_snack(45000));
    assert(pet.food == 28 && pet.mood == 100 && pet.cleanliness == 0 && pet.state == PET_BORED);
    pet_init(0); pet_tick(300000); assert(pet.snack_stock == 3); /* no autonomous consumption */
    uint32_t start = UINT32_MAX - 1000u;
    pet_init(start);
    assert(!pet_take_snack(start + 44999u));
    assert(pet_take_snack(start + 45000u) && pet.snack_stock == 2);
}
static void evening_fatigue_and_fast_sleep(void)
{
    pet_init(0); pet.day_minute = 1080; pet.energy = 100;
    pet_tick(18750); assert(pet.day_minute == 1170 && pet.energy == 60);
    for (int i = 0; i < 100; i++) pet_tick(18750);
    assert(pet.energy == 60); /* repeated calls cannot charge elapsed time twice */
    pet_tick(37500); assert(pet.day_minute == 1260 && pet.energy == 20);
    pet_init(0); pet.day_minute = 1079; pet.energy = 100;
    for (uint32_t t = 17; t < 40000; t += 17) pet_tick(t);
    pet_tick(40000); pet_t evening_expected = pet;
    pet_init(0); pet.day_minute = 1079; pet.energy = 100;
    pet_tick(40000); assert(!memcmp(&pet, &evening_expected, sizeof pet) && pet.energy < 20);
    uint32_t start = UINT32_MAX - 10000u;
    pet_init(start); pet.day_minute = 1080; pet.energy = 100;
    pet_tick(start + 37500u); assert(pet.energy == 20);
    pet_init(0); pet.day_minute = 1080; pet.energy = 0;
    assert(pet_action(PET_REST, 0));
    pet_tick(37500); assert(pet.sleeping && pet.energy == 75 && pet.day_minute == 1260);
    /* Exact full-night reward: 540 game minutes at 16x takes 7032ms. */
    pet_init(0); pet.day_minute = 1260; pet.energy = 20; pet.mood = 25;
    assert(pet_action(PET_REST, 0) && pet.day_minute == 1260);
    pet_tick(3500); assert(pet.sleeping && pet.energy == 39 && pet.mood == 44);
    pet_tick(7031); assert(pet.sleeping && pet.day_minute == 359);
    pet_tick(7032); assert(!pet.sleeping && pet.day_minute == 360 && pet.energy == 100 && pet.mood == 65);
    pet_tick(7032); assert(pet.energy == 100 && pet.mood == 65);
    pet_init(0); pet.day_minute = 1260; pet.energy = 20; pet.mood = 25; pet_action(PET_REST, 0);
    for (uint32_t t = 37; t < 90000; t += 37) pet_tick(t);
    pet_tick(90000); pet_t expected = pet;
    pet_init(0); pet.day_minute = 1260; pet.energy = 20; pet.mood = 25; pet_action(PET_REST, 0);
    pet_tick(90000);
    assert(!memcmp(&pet, &expected, sizeof pet) && pet.energy == 34 && pet.mood == 60);
    pet_init(0); pet.day_minute = 1259; pet.energy = 1; pet.mood = 35; pet_action(PET_REST, 0);
    for (uint32_t t = 17; t < 25000; t += 17) pet_tick(t);
    pet_tick(25000); expected = pet;
    pet_init(0); pet.day_minute = 1259; pet.energy = 1; pet.mood = 35; pet_action(PET_REST, 0);
    pet_tick(25000);
    assert(!memcmp(&pet, &expected, sizeof pet) && pet.energy == 86 && pet.mood == 75);
    /* A care-minute boundary inside accelerated night must not add another +8. */
    pet_init(0); pet_tick(55000); pet.day_minute = 1260; pet.energy = 20; pet.mood = 30;
    assert(pet_action(PET_REST, 55000));
    pet_tick(60000); assert(pet.sleeping && pet.energy == 48 && pet.mood == 58);
    pet_tick(62032); assert(!pet.sleeping && pet.energy == 100 && pet.mood == 70);
    pet_init(start); pet.day_minute = 1260; pet.energy = 95; pet.mood = 98; pet.sleeping = 1;
    pet_tick(start + 7032u); assert(!pet.sleeping && pet.energy == 100 && pet.mood == 100);
}
static void full_belly_feeding(void)
{
    pet_init(0); assert(pet.plumpness == 0); pet.food = 94;
    assert(pet_action(PET_FEED, 0) && pet.plumpness == 0 && pet.food == 100);
    assert(!pet_action(PET_FEED, 29999) && pet.plumpness == 0);
    assert(pet_action(PET_FEED, 30000) && pet.plumpness == 1);
    for (uint32_t t = 60000; t <= 180000; t += 30000) {
        pet_tick(t); pet.food = 95;
        assert(pet_action(PET_FEED, t));
    }
    assert(pet.plumpness == 3 && pet.food == 100 && pet.snack_stock == 3);
    pet_init(0); pet.plumpness = 3; pet.food = 100;
    pet_tick(60000); assert(pet.plumpness == 3); /* only eases below70 food */
    pet.food = 75;
    pet_tick(120000); assert(pet.plumpness == 2 && pet.food == 61);
    pet_tick(120000); assert(pet.plumpness == 2);
    pet_tick(180000); assert(pet.plumpness == 1);
    pet_tick(240000); assert(pet.plumpness == 0);
    pet_tick(300000); assert(pet.plumpness == 0);
    pet_init(0); assert(pet.plumpness == 0);
}
static void daytime_energy_and_bongos(void)
{
    pet_init(0); pet.energy = 100;
    pet_tick(99999); assert(pet.energy == 21);
    pet_tick(100000); assert(pet.energy == 20 && pet.state == PET_TIRED);
    pet_init(0); pet.energy = 20; assert(pet_action(PET_REST, 0));
    pet_tick(34999); assert(pet.energy == 89 && pet.sleeping);
    pet_tick(35000); assert(pet.energy == 90 && !pet.sleeping && pet.mood == 74);
    pet_init(0); pet.energy = 20; pet_action(PET_REST, 0);
    for (uint32_t t = 37; t < 50000; t += 37) pet_tick(t);
    pet_tick(50000); pet_t expected = pet;
    pet_init(0); pet.energy = 20; pet_action(PET_REST, 0); pet_tick(50000);
    assert(!memcmp(&pet, &expected, sizeof pet) && pet.energy == 78 && pet.mood == 74);
    /* Short naps earn only the time spent asleep; toggles retain fractional debt. */
    pet_init(0);
    for (uint32_t t = 0; t < 20000; t += 1000) {
        assert(pet_action(PET_REST, t + 900));
        assert(pet_action(PET_REST, t + 1000));
    }
    assert(pet.energy == 65 && !pet.sleeping);
    uint32_t start = UINT32_MAX - 1500u;
    pet_init(start); pet.energy = 100;
    pet_tick(start + 100000u); assert(pet.energy == 20);
    pet_init(start); pet.energy = 20; pet_action(PET_REST, start);
    pet_tick(start + 35000u); assert(pet.energy == 90 && !pet.sleeping);
    pet_init(0);
    pet_bongo_reward(1, 0); pet_bongo_reward(1, 4999); assert(pet.mood == 70);
    pet_bongo_reward(1, 5000); assert(pet.mood == 71);
    for (int i = 0; i < 1000; i++) pet_bongo_reward(1, 5000);
    assert(pet.mood == 71);
    pet_bongo_reward(1, 17000); assert(pet.mood == 73);
    pet_bongo_reward(0, 18000); pet_bongo_reward(0, 1000000);
    pet_bongo_reward(1, 2000000); assert(pet.mood == 73);
    pet_bongo_reward(1, 2004999); assert(pet.mood == 73);
    pet_bongo_reward(1, 2005000); assert(pet.mood == 74);
    pet.sleeping = 1;
    pet_bongo_reward(1, 2015000); assert(pet.mood == 74);
    pet.sleeping = 0; pet_bongo_reward(1, 2025000); assert(pet.mood == 74);
    pet_bongo_reward(1, 2030000); assert(pet.mood == 75);
    pet.mood = 40; pet.state = PET_BORED;
    pet_bongo_reward(1, 2040000); assert(pet.mood == 40);
    pet_init(start); pet.mood = 99;
    pet_bongo_reward(1, start); pet_bongo_reward(1, start + 5000u); assert(pet.mood == 100);
    pet_bongo_reward(1, start + 20000u); assert(pet.mood == 100);
}
static void poo_cadence_and_cleaning(void)
{
    pet_init(0); assert(pet.poo_count == 0 && !pet_poo_ready());
    pet_tick(89999); assert(!pet_poo_ready() && !pet_make_poo(89999));
    pet_tick(90000); assert(pet_poo_ready());
    unsigned clean = pet.cleanliness;
    assert(pet_make_poo(90000) && pet.poo_count == 1 && pet.cleanliness == clean - 12);
    assert(!pet_poo_ready() && !pet_make_poo(90000));
    assert(!strcmp(pet_feedback(), "A LITTLE ACCIDENT"));
    assert(pet_make_poo(180000) && pet.poo_count == 2);
    assert(pet_make_poo(270000) && pet.poo_count == 3);
    pet_tick(400000); assert(!pet_poo_ready() && !pet_make_poo(400000) && pet.poo_count == 3);
    pet.cleanliness = 100;
    assert(pet_action(PET_CLEAN, 400000) && pet.poo_count == 0 && pet.cleanliness == 100);
    /* Time spent full does not queue several piles: at most one becomes due. */
    assert(pet_make_poo(400000) && pet.poo_count == 1 && pet.cleanliness == 88);
    assert(!pet_make_poo(400000));
    assert(pet_action(PET_CLEAN, 400001) && !pet.poo_count); /* bypass cooldown and high cleanliness */
    assert(!pet_action(PET_CLEAN, 400002)); /* ordinary clean rules remain */
    pet_init(0); pet_tick(40000); assert(pet_action(PET_REST, 40000));
    pet_tick(60000); assert(pet.sleeping && !pet_poo_ready() && !pet_make_poo(60000));
    pet_tick(67500); assert(!pet.sleeping);
    pet_tick(113499); assert(!pet_poo_ready());
    pet_tick(113500); assert(pet_poo_ready()); /* 40s awake +23.5s nap +50s awake */
    pet_init(0); pet.day_minute = 1260; pet.energy = 20; assert(pet_action(PET_REST, 0));
    pet_tick(7032); assert(!pet.sleeping && !pet_poo_ready());
    pet_tick(97031); assert(!pet_poo_ready());
    pet_tick(97032); assert(pet_poo_ready()); /* accelerated night is excluded too */
    pet_init(0); pet_tick(4000000); assert(pet.poo_count == 0 && pet_poo_ready());
    assert(pet_make_poo(4000000) && pet.poo_count == 1);
    assert(!pet_make_poo(4000000) && !pet_poo_ready()); /* no burst after a long stall */
    uint32_t start = UINT32_MAX - 1000u;
    pet_init(start); pet_tick(start + 89999u); assert(!pet_poo_ready());
    assert(pet_make_poo(start + 90000u) && pet.poo_count == 1);
    pet_init(0); assert(pet.poo_count == 0 && !pet_poo_ready());
}
static void feeding_energy(void)
{
    pet_init(0); pet.energy = 35;
    assert(pet_action(PET_FEED, 0) && pet.energy == 45);
    assert(!pet_action(PET_FEED, 0) && pet.energy == 45);
    pet_tick(29999); unsigned energy = pet.energy;
    assert(!pet_action(PET_FEED, 29999) && pet.energy == energy);
    assert(pet_action(PET_FEED, 30000) && pet.energy == 31); /* 24 points drained over30s, plus10 */
    pet_init(0); pet.energy = 95;
    assert(pet_action(PET_FEED, 0) && pet.energy == 100);
    pet_init(0); pet.day_minute = 1170; pet.energy = 58;
    assert(pet_action(PET_FEED, 0) && pet.energy == 60); /* current evening ceiling remains */
    pet_init(0); pet.day_minute = 1260; pet.energy = 18;
    assert(pet_action(PET_FEED, 0) && pet.energy == 20); /* feeding does not postpone bedtime */
    pet_init(0); pet_tick(45000); pet.energy = 99;
    assert(pet_take_snack(45000) && pet.energy == 100);
    assert(!pet_take_snack(45000) && pet.energy == 100);
    pet_init(0); pet_tick(45000); pet.energy = 40;
    assert(pet_take_snack(45000) && pet.energy == 43);
}
int main(void)
{
    pacing_and_states(); care_and_music(); rest_and_elapsed(); wraparound(); persistence(); day_and_night(); news_and_praise(); stocked_snacks(); frequent_feeding(); evening_fatigue_and_fast_sleep(); full_belly_feeding(); daytime_energy_and_bongos(); poo_cadence_and_cleaning(); feeding_energy();
    puts("Pet: 16x nights/recovery, feeding energy/caps, mood/praise, poo/cleaning, naps/bongos, catch-up/wrap, migration/validation, no offline advance PASS");
    return 0;
}
