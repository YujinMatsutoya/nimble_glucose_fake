/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#ifndef GLUCOSE_H
#define GLUCOSE_H

#include <stdint.h>

/* Defines */
#define GLUCOSE_TASK_PERIOD (CONFIG_GLUCOSE_INTERVAL_MS / portTICK_PERIOD_MS)
#define GLUCOSE_PACKET_LEN 9

/* Reading, glucose in mmol/L x 10 (56 = 5.6 mmol/L) */
struct glucose_reading {
    uint16_t seq;
    uint32_t timestamp_s; /* seconds since boot */
    uint16_t glucose;
    uint8_t flags; /* reserved, 0 */
};

/* Public function declarations */
/* Produce the next reading: advances seq and the random walk */
struct glucose_reading glucose_next_reading(void);
/* Latest reading produced (seq 0, glucose start value before the first) */
struct glucose_reading glucose_last_reading(void);
/* Fault injection: jump the sequence forward by k (stays advanced) */
void glucose_skip_seq(uint16_t k);
/* Little-endian packet encoding, buf must hold GLUCOSE_PACKET_LEN bytes */
void glucose_encode(const struct glucose_reading *r, uint8_t *buf);

#endif // GLUCOSE_H
