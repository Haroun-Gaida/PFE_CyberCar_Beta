/**
 * @file attack_deauth_detector.c
 * @brief 802.11 Deauth / Disassoc / Broadcast-kick detector.
 *
 * Improvements over v1:
 *  - Channel hopper: cycles 1 → 6 → 11 every CHANNEL_HOP_INTERVAL ms
 *    so the detector sees attacks on all primary 2.4 GHz channels.
 *  - Detects disassociation frames (subtype 0xA0) in addition to deauth (0xC0).
 *  - Flags broadcast deauth attacks (addr1 = FF:FF:FF:FF:FF:FF).
 */

#include "attack_deauth_detector.h"
#include <string.h>
#include <stdbool.h>
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "deauth_detector";

/* Channel hop sequence: primary 2.4 GHz channels */
static const uint8_t s_hop_channels[] = {1, 6, 11};
#define NUM_HOP_CHANNELS (sizeof(s_hop_channels) / sizeof(s_hop_channels[0]))

static deauth_detector_status_t s_status = {0};
static TaskHandle_t              s_hop_task = NULL;

/* ── Helpers ──────────────────────────────────────────────────────────── */

static bool is_broadcast_mac(const uint8_t *mac) {
    for (int i = 0; i < 6; i++) {
        if (mac[i] != 0xFF) return false;
    }
    return true;
}

static bool is_zero_mac(const uint8_t *mac) {
    for (int i = 0; i < 6; i++) {
        if (mac[i] != 0x00) return false;
    }
    return true;
}

static deauth_track_entry_t *get_or_create_entry(const uint8_t *bssid) {
    for (int i = 0; i < s_status.count; i++) {
        if (memcmp(s_status.entries[i].bssid, bssid, 6) == 0) {
            return &s_status.entries[i];
        }
    }
    if (s_status.count >= MAX_TRACKED_BSSIDS) {
        /* Evict oldest entry (slot 0) */
        memmove(&s_status.entries[0], &s_status.entries[1],
                sizeof(deauth_track_entry_t) * (MAX_TRACKED_BSSIDS - 1));
        s_status.count = MAX_TRACKED_BSSIDS - 1;
    }
    deauth_track_entry_t *e = &s_status.entries[s_status.count++];
    memset(e, 0, sizeof(*e));
    memcpy(e->bssid, bssid, 6);
    e->window_start_ms = esp_timer_get_time() / 1000;
    return e;
}

/* ── Promiscuous callback ─────────────────────────────────────────────── */

typedef struct {
    uint8_t  frame_ctrl[2];
    uint16_t duration;
    uint8_t  addr1[6];   /* receiver / destination (DA)   */
    uint8_t  addr2[6];   /* transmitter / source   (SA)   */
    uint8_t  addr3[6];   /* BSSID                         */
} __attribute__((packed)) wifi_80211_hdr_t;

static void promiscuous_rx_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT) return;

    const wifi_promiscuous_pkt_t *pkt = (const wifi_promiscuous_pkt_t *)buf;
    const wifi_80211_hdr_t       *hdr = (const wifi_80211_hdr_t *)pkt->payload;

    if (is_zero_mac(hdr->addr3)) return;

    uint8_t frame_type    = hdr->frame_ctrl[0] & 0x0C;
    uint8_t frame_subtype = hdr->frame_ctrl[0] & 0xF0;

    /* Only management frames: deauth (0xC0) or disassoc (0xA0) */
    if (frame_type != 0x00) return;
    bool is_deauth   = (frame_subtype == 0xC0);
    bool is_disassoc = (frame_subtype == 0xA0);
    if (!is_deauth && !is_disassoc) return;

    bool is_broadcast_target = is_broadcast_mac(hdr->addr1);

    int64_t now_ms = esp_timer_get_time() / 1000;
    deauth_track_entry_t *entry = get_or_create_entry(hdr->addr3);

    /* Reset window if expired */
    if ((now_ms - entry->window_start_ms) > DEAUTH_WINDOW_MS) {
        entry->count           = 0;
        entry->deauth_count    = 0;
        entry->disassoc_count  = 0;
        entry->broadcast_count = 0;
        entry->flags           = 0;
        entry->window_start_ms = now_ms;
    }

    /* Accumulate counters and flags */
    entry->count++;
    if (is_deauth)           { entry->deauth_count++;   entry->flags |= DEAUTH_FLAG_DEAUTH;    }
    if (is_disassoc)         { entry->disassoc_count++; entry->flags |= DEAUTH_FLAG_DISASSOC;  }
    if (is_broadcast_target) { entry->broadcast_count++; entry->flags |= DEAUTH_FLAG_BROADCAST;}

    /* Threshold check */
    if (entry->count >= DEAUTH_THRESHOLD) {
        entry->alerting      = true;
        entry->last_alert_ms = now_ms;

        ESP_LOGW(TAG,
                 "ATTACK DETECTED BSSID %02X:%02X:%02X:%02X:%02X:%02X "
                 "ch=%d total=%d deauth=%d disassoc=%d bcast=%d flags=0x%02X",
                 entry->bssid[0], entry->bssid[1], entry->bssid[2],
                 entry->bssid[3], entry->bssid[4], entry->bssid[5],
                 s_status.current_channel,
                 entry->count, entry->deauth_count,
                 entry->disassoc_count, entry->broadcast_count,
                 entry->flags);
    }
}

/* ── Channel hopper task ──────────────────────────────────────────────── */

static void channel_hop_task(void *arg) {
    uint8_t idx = 0;
    while (s_status.running) {
        uint8_t ch = s_hop_channels[idx];
        esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
        s_status.current_channel = ch;
        idx = (idx + 1) % NUM_HOP_CHANNELS;
        vTaskDelay(pdMS_TO_TICKS(CHANNEL_HOP_INTERVAL));
    }
    s_hop_task = NULL;
    vTaskDelete(NULL);
}

/* ── Public API ───────────────────────────────────────────────────────── */

void deauth_detector_start(void) {
    memset(&s_status, 0, sizeof(s_status));
    s_status.running         = true;
    s_status.current_channel = s_hop_channels[0];

    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&promiscuous_rx_cb);

    wifi_promiscuous_filter_t filter = {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT
    };
    esp_wifi_set_promiscuous_filter(&filter);

    /* Start channel hopper */
    xTaskCreate(channel_hop_task, "det_hop", 2048, NULL, 5, &s_hop_task);

    ESP_LOGI(TAG, "Deauth detector started (hopping ch 1/6/11 every %d ms).",
             CHANNEL_HOP_INTERVAL);
}

void deauth_detector_stop(void) {
    s_status.running = false;
    /* hopper task exits on its own; give it time */
    vTaskDelay(pdMS_TO_TICKS(CHANNEL_HOP_INTERVAL + 50));

    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(NULL);
    s_hop_task = NULL;

    ESP_LOGI(TAG, "Deauth detector stopped.");
}

const deauth_detector_status_t *deauth_detector_get_status(void) {
    int64_t now_ms = esp_timer_get_time() / 1000;
    for (int i = 0; i < s_status.count; i++) {
        deauth_track_entry_t *e = &s_status.entries[i];
        if (e->alerting && (now_ms - e->last_alert_ms > ALERT_HOLD_MS)) {
            e->alerting = false;
        }
    }
    return &s_status;
}
