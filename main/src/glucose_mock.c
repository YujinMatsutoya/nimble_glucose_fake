/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Includes */
#include "common.h"
#include "esp_timer.h"
#include "glucose.h"

/* Private defines (mmol/L x 10) */
#define GLUCOSE_START 56
#define GLUCOSE_MIN 30
#define GLUCOSE_MAX 150
#define GLUCOSE_MAX_STEP 3

/* Private variables */
static uint32_t rng_state = CONFIG_GLUCOSE_SEED;
static struct glucose_reading last = {.glucose = GLUCOSE_START};

/* xorshift32: deterministic for a given seed */
static uint32_t rng_next(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

/* Public functions */
struct glucose_reading glucose_next_reading(void) {
    /* Random step in [-MAX_STEP, +MAX_STEP], clamped to the plausible range */
    int step = (int)(rng_next() % (2 * GLUCOSE_MAX_STEP + 1)) - GLUCOSE_MAX_STEP;
    int value = (int)last.glucose + step;
    if (value < GLUCOSE_MIN) {
        value = GLUCOSE_MIN;
    } else if (value > GLUCOSE_MAX) {
        value = GLUCOSE_MAX;
    }

    last.seq++; /* uint16_t wraps 65535 -> 0 */
    last.timestamp_s = (uint32_t)(esp_timer_get_time() / 1000000);
    last.glucose = (uint16_t)value;
    last.flags = 0;
    return last;
}

struct glucose_reading glucose_last_reading(void) { return last; }

void glucose_encode(const struct glucose_reading *r, uint8_t *buf) {
    buf[0] = r->seq & 0xff;
    buf[1] = r->seq >> 8;
    buf[2] = r->timestamp_s & 0xff;
    buf[3] = (r->timestamp_s >> 8) & 0xff;
    buf[4] = (r->timestamp_s >> 16) & 0xff;
    buf[5] = (r->timestamp_s >> 24) & 0xff;
    buf[6] = r->glucose & 0xff;
    buf[7] = r->glucose >> 8;
    buf[8] = r->flags;
}
