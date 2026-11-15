/*
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Includes */
#include "fault.h"
#include "common.h"
#include "esp_console.h"
#include "freertos/semphr.h"
#include "gap.h"
#include "glucose.h"
#include <inttypes.h>
#include <stdlib.h>

/* Private variables */
static const char *const names[FAULT_ID_MAX] = {[FAULT_DROP] = "drop"};

static SemaphoreHandle_t lock;
/* One slot: arming replaces. count = events seen since arming / last firing */
static struct {
    bool armed;
    struct fault_cmd c;
    uint32_t count;
} slot;
static bool adv_suppressed;

/* Private functions (call with lock held) */
static bool due(uint16_t seq) {
    switch (slot.c.trigger) {
    case FAULT_TRIG_AFTER:
        return slot.count >= slot.c.trigger_n;
    case FAULT_TRIG_SEQ:
        return seq == slot.c.trigger_n;
    default:
        return true;
    }
}

static void fired(void) {
    if (slot.c.flags & FAULT_FLAG_REPEAT) {
        slot.count = 0;
    } else {
        slot.armed = false;
    }
}

static void log_fired(enum fault_id id, uint16_t seq, uint32_t param) {
    ESP_LOGW(TAG, "FAULT fired id=%d name=%s seq=%u param=%" PRIu32, id,
             names[id], seq, param);
}

static void clear(void) {
    xSemaphoreTake(lock, portMAX_DELAY);
    slot.armed = false;
    bool resume = adv_suppressed;
    adv_suppressed = false;
    xSemaphoreGive(lock);
    if (resume) {
        adv_resume();
    }
    ESP_LOGI(TAG, "fault cleared");
}

/* Public functions */
int fault_command(const struct fault_cmd *c) {
    if (c->cmd == FAULT_CMD_CLEAR) {
        clear();
        return 0;
    }
    if (c->cmd != FAULT_CMD_ARM || c->fault_id <= FAULT_NONE ||
        c->fault_id >= FAULT_ID_MAX || c->trigger > FAULT_TRIG_SEQ) {
        return -1;
    }
    /* A seq trigger fires once as the counter passes N, repeating is moot */
    if (c->trigger == FAULT_TRIG_SEQ && (c->flags & FAULT_FLAG_REPEAT)) {
        return -1;
    }
    if (c->param > 0xff) { /* HCI reason */
        return -1;
    }

    uint16_t last = glucose_last_reading().seq;
    if (c->trigger == FAULT_TRIG_SEQ && c->trigger_n <= last) {
        ESP_LOGW(TAG, "fault seq=%u already passed (last seq %u), never fires",
                 c->trigger_n, last);
    }

    xSemaphoreTake(lock, portMAX_DELAY);
    slot.armed = true;
    slot.c = *c;
    slot.count = 0;
    xSemaphoreGive(lock);
    ESP_LOGI(TAG,
             "fault armed id=%d name=%s trigger=%d n=%u flags=0x%02x "
             "param=%" PRIu32,
             c->fault_id, names[c->fault_id], c->trigger, c->trigger_n,
             c->flags, c->param);
    return 0;
}

enum fault_id fault_on_send(uint16_t next_seq, uint32_t *param) {
    enum fault_id id = FAULT_NONE;

    xSemaphoreTake(lock, portMAX_DELAY);
    if (slot.armed) {
        if (due(next_seq)) {
            id = slot.c.fault_id;
            *param = slot.c.param;
            if (slot.c.flags & FAULT_FLAG_NO_READV) {
                adv_suppressed = true;
            }
            fired();
        } else {
            slot.count++;
        }
    }
    xSemaphoreGive(lock);

    if (id != FAULT_NONE) {
        log_fired(id, next_seq, *param);
    }
    return id;
}

bool fault_adv_suppressed(void) { return adv_suppressed; }

/* Serial console: fault arm <name|id> [now|after=N|seq=N] [repeat]
 *                       [param=V] [no-readv] | fault clear | fault status */
static int parse_id(const char *s) {
    for (int i = FAULT_DROP; i < FAULT_ID_MAX; i++) {
        if (strcmp(s, names[i]) == 0) {
            return i;
        }
    }
    return atoi(s);
}

static int cmd_fault(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "clear") == 0) {
        clear();
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "status") == 0) {
        xSemaphoreTake(lock, portMAX_DELAY);
        bool armed = slot.armed;
        struct fault_cmd c = slot.c;
        uint32_t count = slot.count;
        xSemaphoreGive(lock);
        if (armed) {
            printf("armed: %s trigger=%d n=%u flags=0x%02x param=%" PRIu32
                   " count=%" PRIu32 "\n",
                   names[c.fault_id], c.trigger, c.trigger_n, c.flags, c.param,
                   count);
        } else {
            printf("no fault armed\n");
        }
        printf("adv_suppressed=%d\n", adv_suppressed);
        return 0;
    }
    if (argc >= 3 && strcmp(argv[1], "arm") == 0) {
        struct fault_cmd c = {.cmd = FAULT_CMD_ARM,
                              .fault_id = (uint8_t)parse_id(argv[2])};
        for (int i = 3; i < argc; i++) {
            const char *a = argv[i];
            if (strcmp(a, "now") == 0) {
                c.trigger = FAULT_TRIG_NOW;
            } else if (strncmp(a, "after=", 6) == 0) {
                c.trigger = FAULT_TRIG_AFTER;
                c.trigger_n = (uint16_t)strtoul(a + 6, NULL, 0);
            } else if (strncmp(a, "seq=", 4) == 0) {
                c.trigger = FAULT_TRIG_SEQ;
                c.trigger_n = (uint16_t)strtoul(a + 4, NULL, 0);
            } else if (strcmp(a, "repeat") == 0) {
                c.flags |= FAULT_FLAG_REPEAT;
            } else if (strcmp(a, "no-readv") == 0) {
                c.flags |= FAULT_FLAG_NO_READV;
            } else if (strncmp(a, "param=", 6) == 0) {
                c.param = (uint32_t)strtoul(a + 6, NULL, 0);
            } else {
                printf("unknown option: %s\n", a);
                return 1;
            }
        }
        if (fault_command(&c) != 0) {
            printf("invalid fault\n");
            return 1;
        }
        return 0;
    }
    printf("usage: fault arm <drop|1> "
           "[now|after=N|seq=N] [repeat] [param=V] [no-readv]\n"
           "       fault clear | fault status\n");
    return 1;
}

void fault_init(void) {
    lock = xSemaphoreCreateMutex();
    assert(lock != NULL);

    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_cfg.prompt = "fault>";
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    esp_console_dev_usb_serial_jtag_config_t hw =
        ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    esp_err_t ret = esp_console_new_repl_usb_serial_jtag(&hw, &repl_cfg, &repl);
#else
    esp_console_dev_uart_config_t hw = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    esp_err_t ret = esp_console_new_repl_uart(&hw, &repl_cfg, &repl);
#endif
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to start serial console, error code: %d", ret);
        return;
    }
    const esp_console_cmd_t cmd = {
        .command = "fault",
        .help = "arm/clear/status a scripted fault (see docs/FAULT_MATRIX.md)",
        .func = cmd_fault};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
