/* SPDX-License-Identifier: GPL-3.0-only */
#include "pet.c" /* application is built as one translation unit */
project_t proj;
/* Explicit little-endian payload, independent of C padding/enum size. Object
 * storage supplies CRC and A/B commit. OBJ_PANEL is untouched. The game clock
 * is saved; elapsed-time timers restart at boot, with no offline punishment. */
#define PET_SAVE_BYTES 30u
#define PET_SAVE_VERSION 5u
static uint8_t pet_saved[PET_SAVE_BYTES], pet_have_save;
static void pet_pack(uint8_t *out)
{
    unsigned i;
    out[0] = 'T'; out[1] = 'G'; out[2] = 'O'; out[3] = 'T';
    out[4] = PET_SAVE_VERSION; out[5] = 0;
    out[6] = PET_SAVE_BYTES; out[7] = 0;
    for (i = 0; i < P_NPARAMS; i++) {
        out[8 + i * 2] = (uint8_t)proj.par[i];
        out[9 + i * 2] = (uint8_t)((uint16_t)proj.par[i] >> 8);
    }
    out[16] = pet.food; out[17] = pet.mood; out[18] = pet.energy;
    out[19] = pet.cleanliness; out[20] = pet.sleeping;
    out[21] = pet.neglect; out[22] = pet.rest_minutes; out[23] = 0;
    out[24] = (uint8_t)pet.day_minute; out[25] = (uint8_t)(pet.day_minute >> 8);
    out[26] = pet.snack_stock; out[27] = 0;
    out[28] = pet.plumpness; out[29] = pet.poo_count;
}
static int pet_unpack_valid(const uint8_t *in, int size)
{
    unsigned i;
    if (size != 24 && size != 26 && size != 28 && size != PET_SAVE_BYTES) return 0;
    if ((size == 24 && in[4] != 1) || (size == 26 && in[4] != 2) ||
        (size == 28 && in[4] != 3) || (size == PET_SAVE_BYTES && in[4] != 4 && in[4] != PET_SAVE_VERSION)) return 0;
    if (in[0] != 'T' || in[1] != 'G' || in[2] != 'O' || in[3] != 'T' ||
        in[5] || in[6] != size || in[7] || in[23]) return 0;
    for (i = 0; i < P_NPARAMS; i++) {
        int value = (int)(in[8 + i * 2] | (uint16_t)in[9 + i * 2] << 8);
        if (value >= 32768) value -= 65536;
        if (value < om_param_info(i)->lo || value > om_param_info(i)->hi) return 0;
    }
    for (i = 16; i < 20; i++) if (in[i] > 100) return 0;
    if (in[20] > 1 || in[21] > 60 || in[22] >= 15 || (!in[20] && in[22])) return 0;
    if (size >= 26 && (in[24] | (uint16_t)in[25] << 8) >= 1440) return 0;
    if (size >= 28 && (in[26] > 3 || in[27])) return 0;
    if (size == PET_SAVE_BYTES && (in[28] > 3 || (in[4] == 4 ? in[29] != 0 : in[29] > 3))) return 0;
    return 1;
}
_Static_assert(P_NPARAMS == 4, "Change pet save format/version when audio parameters change");
int project_load(void)
{
    uint8_t data[PET_SAVE_BYTES + 1]; /* extra byte detects oversized objects */
    unsigned i;
    int size;
    for (i = 0; i < P_NPARAMS; i++) proj.par[i] = om_param_info(i)->def;
    pet_init(plat_ms());
    pet_have_save = 0;
    size = plat_store_load(OBJ_PROJ, data, sizeof data);
    if (!pet_unpack_valid(data, size)) return 0;
    for (i = 0; i < P_NPARAMS; i++) {
        int value = (int)(data[8 + i * 2] | (uint16_t)data[9 + i * 2] << 8);
        if (value >= 32768) value -= 65536;
        proj.par[i] = (int16_t)value;
    }
    pet.food = data[16]; pet.mood = data[17]; pet.energy = data[18];
    pet.cleanliness = data[19]; pet.sleeping = data[20];
    pet.neglect = data[21]; pet.rest_minutes = data[22];
    if (size >= 26) pet.day_minute = (uint16_t)(data[24] | (uint16_t)data[25] << 8);
    if (size >= 28) pet.snack_stock = data[26];
    if (size == PET_SAVE_BYTES) pet.plumpness = data[28];
    if (size == PET_SAVE_BYTES && data[4] == PET_SAVE_VERSION) pet.poo_count = data[29];
    pet_update_state();
    if (size == PET_SAVE_BYTES && data[4] == PET_SAVE_VERSION) {
        for (i = 0; i < PET_SAVE_BYTES; i++) pet_saved[i] = data[i];
        pet_have_save = 1;
    } /* v1-v4 migration remains dirty so the next quiet save upgrades to v5 */
    return 1;
}
void project_apply(void)
{ int i; for (i = 0; i < P_NPARAMS; i++) om_set(i, proj.par[i]); }
int pet_needs_save(void)
{
    uint8_t data[PET_SAVE_BYTES];
    unsigned i;
    if (!pet_have_save) return 1;
    pet_pack(data);
    for (i = 0; i < PET_SAVE_BYTES; i++) if (data[i] != pet_saved[i]) return 1;
    return 0;
}
int pet_save(void)
{
    uint8_t data[PET_SAVE_BYTES];
    unsigned i;
    int rc;
    if (!pet_needs_save()) return 0;
    pet_pack(data);
    rc = plat_store_save(OBJ_PROJ, data, sizeof data);
    if (rc) return rc;
    for (i = 0; i < PET_SAVE_BYTES; i++) pet_saved[i] = data[i];
    pet_have_save = 1;
    return 0;
}
