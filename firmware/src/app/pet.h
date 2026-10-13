/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
enum pet_state_t { PET_HAPPY, PET_BORED, PET_HUNGRY, PET_TIRED, PET_SULKING };
typedef enum pet_state_t pet_state_t;
enum pet_action_t { PET_FEED, PET_PRAISE, PET_CLEAN, PET_REST };
typedef struct {
    uint8_t food, mood, energy, cleanliness, sleeping;
    uint8_t neglect, rest_minutes;
    pet_state_t state;
    uint16_t day_minute; /* 0..1439; five-minute awake day, sleeping nights 16x */
    uint8_t snack_stock; /* 0..3 side-table burgers; manual FEED refills */
    uint8_t plumpness; /* 0..3; full-belly meals add a stage, hunger eases it */
    uint8_t poo_count; /* 0..3 visible piles; CLEAN clears them */
} pet_t;
extern pet_t pet;
/* Main/UI loop only. now is the wrapping millisecond clock. No offline decay. */
void pet_init(uint32_t now);
void pet_tick(uint32_t now);
void pet_shift_time(uint32_t elapsed); /* exclude a paused interval from all care timers */
int pet_action(enum pet_action_t action, uint32_t now); /* 1 accepted, 0 refused */
int pet_take_snack(uint32_t now); /* UI requests only when idle; 1 means eaten */
int pet_poo_ready(void); /* call after pet_tick; 90 cumulative awake seconds */
int pet_make_poo(uint32_t now); /* UI animation completion; 1 means added */
void pet_note(uint32_t now); /* human musical input, not loop playback */
/* Call each UI update with actual TV-viewing eligibility. Only changes mood;
 * inactive/sleeping calls reset the viewing interval. Does not run pet_tick. */
void pet_watch_news(int watching, uint32_t now);
void pet_bongo_reward(int active, uint32_t now); /* +1 mood/5s of eligible happy bongos */
int pet_can_dance(void);
int pet_is_night(void); /* 21:00 through 05:59 */
const char *pet_day_phase(void);
const char *pet_state_name(void);
const char *pet_feedback(void); /* uppercase, at most 25 characters */
/* Caller throttles writes and chooses safe quiet time. Never use in an ISR. */
int pet_needs_save(void);
int pet_save(void); /* 0 success; failure leaves state dirty for later retry */
