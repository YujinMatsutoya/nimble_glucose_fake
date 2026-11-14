/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Includes */
#include "gatt_svc.h"
#include "common.h"
#include "glucose.h"
#include <inttypes.h>

/* Private function declarations */
static int glucose_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg);

/* Private variables */
/* Glucose service (custom 128-bit UUIDs, see docs/PROTOCOL.md) */
const ble_uuid128_t glucose_svc_uuid =
    BLE_UUID128_INIT(0x41, 0x0b, 0x28, 0x6f, 0xf9, 0x4d, 0x7d, 0x9d, 0x43,
                     0x42, 0x64, 0x92, 0xab, 0xd7, 0x9c, 0xe6);

static uint16_t glucose_chr_val_handle;
static const ble_uuid128_t glucose_chr_uuid =
    BLE_UUID128_INIT(0x47, 0x47, 0x4d, 0x54, 0x87, 0xc8, 0x2b, 0xa0, 0xd0,
                     0x47, 0x84, 0x21, 0x3d, 0x4a, 0xa4, 0xeb);

#if CONFIG_GLUCOSE_DELIVERY_INDICATE
#define GLUCOSE_SEND_FLAG BLE_GATT_CHR_F_INDICATE
#else
#define GLUCOSE_SEND_FLAG BLE_GATT_CHR_F_NOTIFY
#endif

static uint16_t glucose_chr_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static bool glucose_sub_status = false;

/* GATT services table */
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    /* Glucose service */
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = &glucose_svc_uuid.u,
     .characteristics =
         (struct ble_gatt_chr_def[]){
             {/* Glucose reading characteristic */
              .uuid = &glucose_chr_uuid.u,
              .access_cb = glucose_chr_access,
              .flags = BLE_GATT_CHR_F_READ | GLUCOSE_SEND_FLAG,
              .val_handle = &glucose_chr_val_handle},
             {
                 0, /* No more characteristics in this service. */
             }}},

    {
        0, /* No more services. */
    },
};

/* Private functions */
static int glucose_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg) {
    /* Note: glucose characteristic is read only */
    switch (ctxt->op) {

    /* Read characteristic event */
    case BLE_GATT_ACCESS_OP_READ_CHR:
        if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
            ESP_LOGI(TAG, "characteristic read; conn_handle=%d attr_handle=%d",
                     conn_handle, attr_handle);
        } else {
            ESP_LOGI(TAG, "characteristic read by nimble stack; attr_handle=%d",
                     attr_handle);
        }

        if (attr_handle == glucose_chr_val_handle) {
            /* Return the latest reading without advancing the sequence */
            struct glucose_reading r = glucose_last_reading();
            uint8_t buf[GLUCOSE_PACKET_LEN];
            glucose_encode(&r, buf);
            int rc = os_mbuf_append(ctxt->om, buf, sizeof(buf));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        goto error;

    /* Unknown event */
    default:
        goto error;
    }

error:
    ESP_LOGE(TAG,
             "unexpected access operation to glucose characteristic, opcode: %d",
             ctxt->op);
    return BLE_ATT_ERR_UNLIKELY;
}

/* Public functions */
void send_glucose_reading(void) {
    if (!glucose_sub_status) {
        return;
    }

    struct glucose_reading r = glucose_next_reading();
    uint8_t buf[GLUCOSE_PACKET_LEN];
    glucose_encode(&r, buf);

    struct os_mbuf *om = ble_hs_mbuf_from_flat(buf, sizeof(buf));
    if (om == NULL) {
        ESP_LOGE(TAG, "failed to allocate mbuf for reading seq=%u", r.seq);
        return;
    }

    /* ble_gatts_notify_custom / ble_gatts_indicate_custom take ownership of om */
#if CONFIG_GLUCOSE_DELIVERY_INDICATE
    int rc = ble_gatts_indicate_custom(glucose_chr_conn_handle,
                                       glucose_chr_val_handle, om);
#else
    int rc = ble_gatts_notify_custom(glucose_chr_conn_handle,
                                     glucose_chr_val_handle, om);
#endif
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to send reading seq=%u, error code: %d", r.seq,
                 rc);
        return;
    }
    ESP_LOGI(TAG, "sent reading seq=%u t=%" PRIu32 "s glucose=%u.%u mmol/L",
             r.seq, r.timestamp_s, r.glucose / 10, r.glucose % 10);
}

/*
 *  Handle GATT attribute register events
 *      - Service register event
 *      - Characteristic register event
 *      - Descriptor register event
 */
void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg) {
    /* Local variables */
    char buf[BLE_UUID_STR_LEN];

    /* Handle GATT attributes register events */
    switch (ctxt->op) {

    /* Service register event */
    case BLE_GATT_REGISTER_OP_SVC:
        ESP_LOGD(TAG, "registered service %s with handle=%d",
                 ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                 ctxt->svc.handle);
        break;

    /* Characteristic register event */
    case BLE_GATT_REGISTER_OP_CHR:
        ESP_LOGD(TAG,
                 "registering characteristic %s with "
                 "def_handle=%d val_handle=%d",
                 ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                 ctxt->chr.def_handle, ctxt->chr.val_handle);
        break;

    /* Descriptor register event */
    case BLE_GATT_REGISTER_OP_DSC:
        ESP_LOGD(TAG, "registering descriptor %s with handle=%d",
                 ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                 ctxt->dsc.handle);
        break;

    /* Unknown event */
    default:
        assert(0);
        break;
    }
}

/*
 *  GATT server subscribe event callback
 *      1. Update glucose subscription status
 */
void gatt_svr_subscribe_cb(struct ble_gap_event *event) {
    if (event->subscribe.conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        ESP_LOGI(TAG, "subscribe event; conn_handle=%d attr_handle=%d",
                 event->subscribe.conn_handle, event->subscribe.attr_handle);
    } else {
        ESP_LOGI(TAG, "subscribe by nimble stack; attr_handle=%d",
                 event->subscribe.attr_handle);
    }

    if (event->subscribe.attr_handle == glucose_chr_val_handle) {
        if (event->subscribe.conn_handle == BLE_HS_CONN_HANDLE_NONE) {
            return;
        }

#if CONFIG_GLUCOSE_DELIVERY_INDICATE
        bool enabled = event->subscribe.cur_indicate;
#else
        bool enabled = event->subscribe.cur_notify;
#endif
        glucose_chr_conn_handle = event->subscribe.conn_handle;
        glucose_sub_status = enabled;
        ESP_LOGI(TAG, "glucose readings %s",
                 enabled ? "subscribed" : "unsubscribed");
    }
}

void gatt_svr_reset_glucose_subscription(void) {
    glucose_chr_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    glucose_sub_status = false;
}

/*
 *  GATT server initialization
 *      1. Initialize GATT service
 *      2. Update NimBLE host GATT services counter
 *      3. Add GATT services to server
 */
int gatt_svc_init(void) {
    /* Local variables */
    int rc = 0;

    /* 1. GATT service initialization */
    ble_svc_gatt_init();

    /* 2. Update GATT services counter */
    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    /* 3. Add GATT services */
    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    return 0;
}
