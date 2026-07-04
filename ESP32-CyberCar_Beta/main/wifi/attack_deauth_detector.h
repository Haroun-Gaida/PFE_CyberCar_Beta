/**
 * @file attack_deauth_detector.h
 * @brief 802.11 Deauth / Disassoc / Broadcast-kick detector.
 *        Runs a channel-hopper (ch 1→6→11) so it sees attacks on all
 *        main 2.4 GHz WiFi channels, not just the one the radio is on.
 */

#ifndef ATTACK_DEAUTH_DETECTOR_H
#define ATTACK_DEAUTH_DETECTOR_H

#include <stdint.h>
#include <stdbool.h>

/* Tuning knobs ----------------------------------------------------------- */
#define DEAUTH_THRESHOLD       10     /* frames per window to trigger alert  */
#define DEAUTH_WINDOW_MS     1000     /* sliding window width (ms)           */
#define MAX_TRACKED_BSSIDS     16     /* max APs tracked simultaneously      */
#define ALERT_HOLD_MS        5000     /* how long an alert stays "active"    */
#define CHANNEL_HOP_INTERVAL  200     /* ms between channel hops             */

/* Alert type flags ------------------------------------------------------- */
#define DEAUTH_FLAG_DEAUTH       (1 << 0)   /* subtype 0xC0 frames seen      */
#define DEAUTH_FLAG_DISASSOC     (1 << 1)   /* subtype 0xA0 frames seen      */
#define DEAUTH_FLAG_BROADCAST    (1 << 2)   /* addr1 = FF:FF:FF:FF:FF:FF     */

/* Per-BSSID tracking entry ----------------------------------------------- */
typedef struct {
    uint8_t  bssid[6];
    uint16_t count;               /* frames in current window                */
    uint16_t deauth_count;        /* subtype 0xC0                            */
    uint16_t disassoc_count;      /* subtype 0xA0                            */
    uint16_t broadcast_count;     /* broadcast target (mass-kick)            */
    uint8_t  flags;               /* DEAUTH_FLAG_* bitmask                   */
    int64_t  window_start_ms;
    bool     alerting;
    int64_t  last_alert_ms;
} deauth_track_entry_t;

/* Global detector state -------------------------------------------------- */
typedef struct {
    deauth_track_entry_t entries[MAX_TRACKED_BSSIDS];
    uint8_t              count;
    bool                 running;
    uint8_t              current_channel;  /* channel hopper state           */
} deauth_detector_status_t;

/* Public API ------------------------------------------------------------- */
void                            deauth_detector_start(void);
void                            deauth_detector_stop(void);
const deauth_detector_status_t *deauth_detector_get_status(void);

#endif /* ATTACK_DEAUTH_DETECTOR_H */
