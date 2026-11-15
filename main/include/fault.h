/*
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#ifndef FAULT_H
#define FAULT_H

#include <stdbool.h>
#include <stdint.h>

enum fault_id {
    FAULT_NONE = 0,
    FAULT_DROP,    /* disconnect; param = HCI reason (0 -> 0x13) */
    FAULT_ID_MAX
};

enum fault_trigger { FAULT_TRIG_NOW, FAULT_TRIG_AFTER, FAULT_TRIG_SEQ };

#define FAULT_CMD_CLEAR 0
#define FAULT_CMD_ARM 1
#define FAULT_FLAG_REPEAT 0x01
#define FAULT_FLAG_NO_READV 0x02 /* drop: do not resume advertising */
#define FAULT_CMD_LEN 10         /* wire size, little-endian, see FAULT_MATRIX.md */

struct fault_cmd {
    uint8_t cmd;
    uint8_t fault_id;
    uint8_t trigger;
    uint16_t trigger_n;
    uint8_t flags;
    uint32_t param;
};

/* Create the lock and start the serial console; call before BLE starts */
void fault_init(void);
/* Arm or clear (BLE write and serial both end up here); 0 = ok */
int fault_command(const struct fault_cmd *c);
/* Called by the sender before each reading. Returns the fault to apply now
 * (FAULT_NONE = send normally) */
enum fault_id fault_on_send(uint16_t next_seq, uint32_t *param);
/* True after a drop fault with FAULT_FLAG_NO_READV, until cleared */
bool fault_adv_suppressed(void);

#endif // FAULT_H
