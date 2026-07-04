/**
 * @file nrf24_scanner.h
 * @brief 2.4 GHz RF scanner using dual NRF24L01 modules on one ESP32.
 *
 * Role split (same ESP32, two independent modules):
 *   Module 0 (CE=16, CSN=4 ) → WiFi-channel sentinel: watches ch 1, 6, 11
 *   Module 1 (CE=5,  CSN=17) → Broadband sweeper: cycles all 126 channels
 *
 * Both results are exposed via nrf24_scanner_get_result() and the
 * HTTP endpoint GET /rf-scan (JSON).
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NRF_SCAN_CHANNELS        126
#define WIFI_CH1_NRF             0    /* NRF channel for WiFi ch 1  = 2401 MHz */
#define WIFI_CH6_NRF             25   /* NRF channel for WiFi ch 6  = 2426 MHz */
#define WIFI_CH11_NRF            50   /* NRF channel for WiFi ch 11 = 2451 MHz */

/* ── Result struct exposed to webserver / UI ─────────────────────────── */
typedef struct {
    /* Full 2.4GHz energy map from Module 1 (1 = energy, 0 = clear) */
    uint8_t  channel_map[NRF_SCAN_CHANNELS];

    /* WiFi-channel sentinel from Module 0 */
    bool     wifi_ch1_busy;
    bool     wifi_ch6_busy;
    bool     wifi_ch11_busy;

    /* Derived fields */
    bool     jamming_detected;    /* >80% of channels busy = jamming    */
    uint8_t  busy_count;          /* total channels with energy         */
    uint32_t scan_count;          /* how many full sweeps completed     */
    int64_t  last_scan_ms;        /* timestamp of last completed sweep  */

    bool     running;
} nrf24_scan_result_t;

/* ── Public API ──────────────────────────────────────────────────────── */

/**
 * @brief Start both NRF24 modules and launch the scanner background task.
 * @return ESP_OK on success.
 */
esp_err_t nrf24_scanner_start(void);

/** @brief Stop scanner task and power down both modules. */
void      nrf24_scanner_stop(void);

/** @brief Get a snapshot of the latest scan results (thread-safe copy). */
nrf24_scan_result_t nrf24_scanner_get_result(void);

/** @brief Format scan result as a JSON string into buf (null-terminated). */
int       nrf24_scanner_to_json(char *buf, size_t buf_len);

#ifdef __cplusplus
}
#endif
