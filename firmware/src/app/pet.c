/* SPDX-License-Identifier: GPL-3.0-only */
#include "pet.h"
#define PET_MINUTE 60000u
/* Three game minutes per 625ms gives exactly 1440 per 300000ms (five minutes). */
#define PET_CLOCK_NUMERATOR 3u
#define PET_CLOCK_DENOMINATOR 625u
#define PET_CARE_COOLDOWN (2u * PET_MINUTE)
#define PET_PRAISE_COOLDOWN 15000u
#define PET_FEED_COOLDOWN 30000u
pet_t pet;
static uint32_t pet_last_minute, pet_last_clock_ms, pet_clock_fraction, pet_last_care[3], pet_last_note, pet_feedback_at;
static uint8_t pet_care_used, pet_note_used;
static uint32_t pet_news_at;
static uint32_t pet_last_snack;
static uint32_t pet_night_recovery_fraction;
static int32_t pet_energy_fraction;
static uint32_t pet_nap_mood_ms, pet_bongo_at;
static uint8_t pet_bongo_active;
static uint32_t pet_poo_awake_ms;
static uint8_t pet_news_active, pet_news_complained;
static const char *pet_message;
static uint8_t pet_add(uint8_t value, unsigned amount)
{ return (uint8_t)(value + amount > 100u ? 100u : value + amount); }
static uint8_t pet_sub(uint8_t value, unsigned amount)
{ return (uint8_t)(value < amount ? 0u : value - amount); }
int pet_is_night(void)
{ return pet.day_minute >= 1260u || pet.day_minute < 360u; }
const char *pet_day_phase(void)
{
    if (pet_is_night()) return "NIGHT";
    if (pet.day_minute < 720u) return "MORNING";
    if (pet.day_minute < 1080u) return "DAY";
    return "EVENING";
}
static void pet_energy_ceiling(void)
{
    unsigned ceiling = 100;
    if (pet.sleeping) return;
    if (pet_is_night()) ceiling = 20;
    else if (pet.day_minute >= 1080u) ceiling = 100u - (pet.day_minute - 1080u) * 80u / 180u;
    if (pet.energy > ceiling) pet.energy = (uint8_t)ceiling;
}
static void pet_update_state(void)
{
    pet_energy_ceiling();
    if (pet.sleeping) pet.state = PET_TIRED;
    else if (pet.food <= 25) pet.state = PET_HUNGRY;
    else if (pet.energy <= 20 || pet_is_night()) pet.state = PET_TIRED;
    else if (pet.neglect >= 12 || pet.mood <= 10) pet.state = PET_SULKING;
    else if (pet.mood <= 40 || pet.cleanliness <= 25) pet.state = PET_BORED;
    else pet.state = PET_HAPPY;
}
static void pet_say(const char *message, uint32_t now)
{ pet_message = message; pet_feedback_at = now; }
/* Rates are points per five seconds: awake -4, naps +10. Shared fractional
 * energy prevents rest toggles discarding debt. Divide before multiplying. */
static void pet_energy_elapsed(uint32_t ms, int rate)
{
    int32_t fraction = pet_energy_fraction + (int32_t)(ms % 5000u) * rate;
    int32_t points = (int32_t)(ms / 5000u) * rate + fraction / 5000;
    int32_t value = pet.energy + points;
    pet_energy_fraction = fraction % 5000;
    if (value <= 0) {
        pet.energy = 0;
        if (pet_energy_fraction < 0) pet_energy_fraction = 0;
    } else if (value >= 100) {
        pet.energy = 100;
        if (pet_energy_fraction > 0) pet_energy_fraction = 0;
    } else pet.energy = (uint8_t)value;
}
static void pet_clock_to(uint32_t now)
{
    uint32_t elapsed = (uint32_t)(now - pet_last_clock_ms);
    /* Split sleep at dusk, dawn, or full recovery. While awake, visit evening
     * clock-minute ceilings in order; once energy is zero skip the remainder.
     * Awake energy reaches zero within 125s, bounding evening boundary work. */
    while (elapsed) {
        int night_sleep = pet.sleeping && pet_is_night();
        uint32_t speed = PET_CLOCK_NUMERATOR * (night_sleep ? 16u : 1u);
        uint32_t segment = elapsed, boundary = 0;
        uint32_t to_boundary = 0;
        pet_energy_ceiling();
        if (pet.sleeping && !night_sleep && pet.energy >= 90) {
            pet.sleeping = pet.rest_minutes = 0;
            pet_say("READY FOR MY CLOSE-UP", pet_last_clock_ms);
            continue;
        }
        if (pet.sleeping) {
            to_boundary = night_sleep ? (1800u - pet.day_minute) % 1440u : 1260u - pet.day_minute;
        } else if (pet.energy) {
            if (pet_is_night()) to_boundary = (1800u - pet.day_minute) % 1440u;
            else to_boundary = pet.day_minute < 1080u ? 1080u - pet.day_minute : 1u;
        }
        if (to_boundary) {
            boundary = (to_boundary * PET_CLOCK_DENOMINATOR - pet_clock_fraction + speed - 1u) / speed;
            if (segment > boundary) segment = boundary;
        }
        if (pet.sleeping && !night_sleep) {
            uint32_t until_rested = (uint32_t)(((90 - pet.energy) * 5000 - pet_energy_fraction + 9) / 10);
            if (segment > until_rested) segment = until_rested;
        }
        /* Divide before multiplication to avoid overflow on long stalls. */
        uint32_t fraction = (segment % PET_CLOCK_DENOMINATOR) * speed + pet_clock_fraction;
        uint32_t steps = (segment / PET_CLOCK_DENOMINATOR) * speed + fraction / PET_CLOCK_DENOMINATOR;
        if (night_sleep) {
            uint32_t reward = pet_night_recovery_fraction + steps * 40u;
            pet.energy = pet_add(pet.energy, reward / 540u);
            pet.mood = pet_add(pet.mood, reward / 540u);
            pet_night_recovery_fraction = reward % 540u;
        } else if (pet.sleeping) {
            pet_energy_elapsed(segment, 10);
            pet_nap_mood_ms += segment;
            pet.mood = pet_add(pet.mood, pet_nap_mood_ms / 7500u);
            pet_nap_mood_ms %= 7500u;
        } else {
            pet_energy_elapsed(segment, -4);
            if (segment >= 90000u - pet_poo_awake_ms) pet_poo_awake_ms = 90000u;
            else pet_poo_awake_ms += segment;
        }
        pet.day_minute = (uint16_t)((pet.day_minute + steps) % 1440u);
        pet_clock_fraction = fraction % PET_CLOCK_DENOMINATOR;
        pet_last_clock_ms += segment;
        elapsed -= segment;
        if (night_sleep && segment == boundary) {
            /* Any sleep that continues through dawn finishes fully rested,
             * including a daytime nap carried across dusk. */
            pet.energy = 100;
            pet_energy_fraction = 0;
            pet.sleeping = pet.rest_minutes = 0;
            pet_say("A TREMENDOUS MORNING", pet_last_clock_ms);
        } else if (pet.sleeping && !night_sleep && pet.energy >= 90) {
            pet.sleeping = pet.rest_minutes = 0;
            pet_say("READY FOR MY CLOSE-UP", pet_last_clock_ms);
        }
        pet_energy_ceiling();
    }
}
void pet_init(uint32_t now)
{
    pet = (pet_t){75, 70, 75, 80, 0, 0, 0, PET_HAPPY, 540, 3, 0, 0};
    pet_last_minute = pet_last_clock_ms = pet_last_note = pet_feedback_at = now;
    pet_clock_fraction = 0;
    pet_night_recovery_fraction = 0;
    pet_energy_fraction = 0;
    pet_nap_mood_ms = 0;
    pet_bongo_at = now;
    pet_bongo_active = 0;
    pet_poo_awake_ms = 0;
    pet_last_care[0] = pet_last_care[1] = pet_last_care[2] = now;
    pet_care_used = pet_note_used = 0;
    pet_news_at = now;
    pet_last_snack = now;
    pet_news_active = pet_news_complained = 0;
    pet_message = 0;
}
void pet_shift_time(uint32_t elapsed)
{
    pet_last_minute+=elapsed;pet_last_clock_ms+=elapsed;
    pet_last_note+=elapsed;pet_feedback_at+=elapsed;
    pet_news_at+=elapsed;pet_last_snack+=elapsed;pet_bongo_at+=elapsed;
    for(int i=0;i<3;i++)pet_last_care[i]+=elapsed;
}
void pet_tick(uint32_t now)
{
    uint32_t minutes = (uint32_t)(now - pet_last_minute) / PET_MINUTE;
    uint32_t skipped = minutes > 180u ? minutes - 180u : 0u;
    /* Every starting state reaches the same neglected fixed point within 180
     * minutes. Bound work even after a long stall, keeping the remainder. */
    if (minutes > 180u) minutes = 180u;
    while (minutes--) {
        pet_last_minute += PET_MINUTE;
        /* Advance through each care boundary: a long stall must not apply
         * only its final day phase to the entire elapsed interval. */
        pet_clock_to(pet_last_minute);
        pet.food = pet_sub(pet.food, pet.sleeping ? 1u : 14u);
        if (pet.food < 70 && pet.plumpness) pet.plumpness--;
        pet.cleanliness = pet_sub(pet.cleanliness, 1u);
        if (pet.sleeping) {
            if (!pet_is_night() && pet.rest_minutes < 14u) pet.rest_minutes++;
        } else {
            pet.mood = pet_sub(pet.mood, 5u);
        }
        if (pet.food <= 20 || pet.cleanliness <= 20 || (!pet.sleeping && pet.energy <= 20)) {
            if (pet.neglect < 60u) pet.neglect++;
            pet.mood = pet_sub(pet.mood, 1u);
        } else if (pet.food > 35 && pet.cleanliness > 35 && pet.energy > 35 && pet.neglect) {
            pet.neglect--;
        }
    }
    pet_last_minute += skipped * PET_MINUTE;
    pet_clock_to(now);
    if (pet_message && (uint32_t)(now - pet_feedback_at) >= 6000u) pet_message = 0;
    pet_update_state();
}
int pet_action(enum pet_action_t action, uint32_t now)
{
    pet_tick(now);
    if (action < PET_FEED || action > PET_REST) return 0;
    if (action == PET_REST) {
        if (pet.sleeping) {
            pet.sleeping = pet.rest_minutes = 0;
            pet_say("BACK TO BUSINESS", now);
        } else if (pet.energy >= 85 && !pet_is_night()) {
            pet_say("TOO MUCH ENERGY TO NAP", now);
            return 0;
        } else {
            pet.sleeping = 1;
            pet.rest_minutes = 0;
            pet_night_recovery_fraction = 0;
            pet_say(pet_is_night() ? "PRESIDENTIAL BEDTIME" : "EXECUTIVE NAP", now);
        }
    } else {
        if (pet.sleeping) { pet_say("LET ME FINISH MY NAP", now); return 0; }
        if (!(action == PET_CLEAN && pet.poo_count) && (pet_care_used & (1u << action)) &&
            (uint32_t)(now - pet_last_care[action]) <
                (action == PET_PRAISE ? PET_PRAISE_COOLDOWN :
                 action == PET_FEED ? PET_FEED_COOLDOWN : PET_CARE_COOLDOWN)) {
            pet_say("GIVE ME A MINUTE", now);
            return 0;
        }
        if (action == PET_FEED) {
            if (pet.food >= 95 && pet.plumpness < 3) pet.plumpness++;
            pet.food = pet_add(pet.food, 25);
            pet.energy = pet_add(pet.energy, 10);
            pet.mood = pet_add(pet.mood, 2);
            pet.cleanliness = pet_sub(pet.cleanliness, 3);
            pet.snack_stock = 3;
            pet_last_snack = now;
            pet_say("A VERY FINE SNACK", now);
        } else if (action == PET_PRAISE) {
            if (pet.food <= 25) { pet_say("SNACKS BEFORE SPEECHES", now); return 0; }
            if (pet.energy <= 20 || pet_is_night()) { pet_say("PRAISE AFTER MY NAP", now); return 0; }
            if (pet.cleanliness <= 25) { pet_say("A LITTLE CLEANUP FIRST", now); return 0; }
            if (pet.mood >= 100) { pet_say("YES, I KNOW. THANK YOU.", now); return 0; }
            pet.mood = pet_add(pet.mood, 15);
            if (pet.neglect) pet.neglect--;
            pet_say("FINALLY, SOME RESPECT", now);
        } else {
            if (pet.cleanliness > 80 && !pet.poo_count) { pet_say("ALREADY LOOKING SHARP", now); return 0; }
            pet.cleanliness = pet_add(pet.cleanliness, 30);
            pet.poo_count = 0;
            pet.mood = pet_add(pet.mood, 2);
            pet_say("SPOTLESS. AS EXPECTED.", now);
        }
        pet_care_used |= (uint8_t)(1u << action);
        pet_last_care[action] = now;
    }
    pet_update_state();
    return 1;
}
int pet_can_dance(void)
{
    return !pet.sleeping && pet.state == PET_HAPPY;
}
int pet_take_snack(uint32_t now)
{
    pet_tick(now);
    if (pet.sleeping || pet_is_night() || pet.food > 75 || !pet.snack_stock ||
        (uint32_t)(now - pet_last_snack) < 45000u) return 0;
    pet.snack_stock--;
    pet.food = pet_add(pet.food, 8);
    pet.energy = pet_add(pet.energy, 3);
    pet.mood = pet_add(pet.mood, 1);
    pet.cleanliness = pet_sub(pet.cleanliness, 8);
    pet_last_snack = now;
    pet_say("A LITTLE SIDE SNACK", now);
    pet_update_state();
    return 1;
}
int pet_poo_ready(void)
{
    return !pet.sleeping && pet.poo_count < 3 && pet_poo_awake_ms >= 90000u;
}
int pet_make_poo(uint32_t now)
{
    pet_tick(now);
    if (!pet_poo_ready()) return 0;
    pet.poo_count++;
    pet.cleanliness = pet_sub(pet.cleanliness, 12);
    pet_poo_awake_ms = 0;
    pet_say("A LITTLE ACCIDENT", now);
    pet_update_state();
    return 1;
}
void pet_watch_news(int watching, uint32_t now)
{
    uint32_t intervals, drain;
    if (!watching || pet.sleeping) {
        pet_news_active = pet_news_complained = 0;
        pet_news_at = now;
        return;
    }
    if (!pet_news_active) {
        pet_news_active = 1;
        pet_news_at = now;
        return;
    }
    intervals = (uint32_t)(now - pet_news_at) / 900u;
    pet_news_at += intervals * 900u;
    if (!intervals || pet.mood <= 25) return;
    drain = intervals;
    if (drain > (uint32_t)(pet.mood - 25)) drain = (uint32_t)(pet.mood - 25);
    pet.mood = (uint8_t)(pet.mood - drain);
    if (!pet_news_complained) {
        pet_say("NEWS IS UNFAIR", now);
        pet_news_complained = 1;
    }
    pet_update_state();
}
void pet_bongo_reward(int active, uint32_t now)
{
    uint32_t intervals;
    if (!active || !pet_can_dance()) {
        pet_bongo_active = 0;
        pet_bongo_at = now;
        return;
    }
    if (!pet_bongo_active) {
        pet_bongo_active = 1;
        pet_bongo_at = now;
        return;
    }
    intervals = (uint32_t)(now - pet_bongo_at) / 5000u;
    pet_bongo_at += intervals * 5000u;
    if (intervals) {
        pet.mood = pet_add(pet.mood, intervals);
        pet_update_state();
    }
}
void pet_note(uint32_t now)
{
    pet_tick(now);
    /* A bored pet can enjoy music before becoming happy enough to dance. */
    if (pet.sleeping || pet_is_night() || pet.food <= 25 || pet.energy <= 20 ||
        pet.cleanliness <= 25 || pet.neglect >= 12 || pet.mood <= 10) return;
    if (pet_note_used && (uint32_t)(now - pet_last_note) < PET_MINUTE) return;
    pet_note_used = 1;
    pet_last_note = now;
    pet.mood = pet_add(pet.mood, 2);
    pet.energy = pet_sub(pet.energy, 1);
    pet_say("THE CROWD LOVES IT", now);
    pet_update_state();
}
const char *pet_state_name(void)
{
    static const char *const names[] = {"HAPPY", "BORED", "HUNGRY", "TIRED", "SULKING"};
    return names[pet.state];
}
const char *pet_feedback(void)
{
    if (pet_message) return pet_message;
    if (pet.sleeping) return pet_is_night() ? "SLEEPING UNTIL SUNRISE" : "EXECUTIVE NAP";
    if (pet.food <= 25) return "I WANT FOOD";
    if (pet_is_night()) return "I WANT SLEEP";
    if (pet.energy <= 20) return "I WANT SLEEP";
    if (pet.cleanliness <= 25) return "THIS PLACE NEEDS A CLEAN";
    if (pet.state == PET_SULKING) return "I EXPECT BETTER SERVICE";
    if (pet.state == PET_BORED) return "A LITTLE ATTENTION?";
    return "A TREMENDOUS DAY";
}
