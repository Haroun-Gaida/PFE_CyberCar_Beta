/**
 * @file nrf24_scanner.c
 * @brief Dual NRF24L01 2.4 GHz scanner for Hydra ESP32.
 *
 * Module 0 (CE=16, CSN=4 ): WiFi-channel sentinel, monitors ch 1/6/11
 * Module 1 (CE=5,  CSN=17): Broadband sweeper, sweeps all 126 channels
 */

#include "nrf24_scanner.h"
#include "nrf24.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "nrf24_scan";

/* ── Module handles ──────────────────────────────────────────────────── */
static nrf24_t s_mod0;  /* WiFi sentinel */
static nrf24_t s_mod1;  /* Broadband sweeper */

/* ── Shared result (protected by mutex) ─────────────────────────────── */
static nrf24_scan_result_t s_result;
static SemaphoreHandle_t   s_mutex = NULL;
static TaskHandle_t        s_task  = NULL;

/* WiFi channel → NRF channel mapping (2400 + nrf_ch = freq MHz) */
/* WiFi ch 1 = 2412 MHz → NRF ch 12; ch 6 = 2437 → NRF 37; ch 11 = 2462 → NRF 62 */
#define NRF_CH_WIFI1   12
#define NRF_CH_WIFI6   37
#define NRF_CH_WIFI11  62

/* ── Scanner task ────────────────────────────────────────────────────── */

static void scanner_task(void *arg) {
    uint8_t sweep_map[NRF_SCAN_CHANNELS];

    while (s_result.running) {

        /* ---- Module 0: WiFi sentinel (3 channels) ---- */
        nrf24_power_up_rx(&s_mod0);

        bool ch1  = false, ch6  = false, ch11 = false;
        uint8_t sentinel_chs[3] = { NRF_CH_WIFI1, NRF_CH_WIFI6, NRF_CH_WIFI11 };
        bool   *sentinel_res[3] = { &ch1, &ch6, &ch11 };

        for (int i = 0; i < 3; i++) {
            nrf24_set_channel(&s_mod0, sentinel_chs[i]);
            bool detected = nrf24_carrier_detect(&s_mod0);
            *sentinel_res[i] = detected;
        }
        nrf24_power_down(&s_mod0);

        /* ---- Module 1: full 126-channel sweep ---- */
        memset(sweep_map, 0, sizeof(sweep_map));
        nrf24_scan_all_channels(&s_mod1, sweep_map, 250);

        /* ---- Compute derived stats ---- */
        uint8_t busy = 0;
        for (int i = 0; i < NRF_SCAN_CHANNELS; i++) {
            if (sweep_map[i]) busy++;
        }
        bool jamming = (busy > (NRF_SCAN_CHANNELS * 80 / 100));

        /* ---- Update shared result ---- */
        if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
            memcpy(s_result.channel_map, sweep_map, NRF_SCAN_CHANNELS);
            s_result.wifi_ch1_busy     = ch1;
            s_result.wifi_ch6_busy     = ch6;
            s_result.wifi_ch11_busy    = ch11;
            s_result.jamming_detected  = jamming;
            s_result.busy_count        = busy;
            s_result.scan_count++;
            s_result.last_scan_ms      = esp_timer_get_time() / 1000;
            xSemaphoreGive(s_mutex);
        }

        if (jamming) {
            ESP_LOGW(TAG, "JAMMING DETECTED: %d/126 channels busy!", busy);
        }
        if (ch1 || ch6 || ch11) {
            ESP_LOGI(TAG, "WiFi interference: ch1=%d ch6=%d ch11=%d",
                     ch1, ch6, ch11);
        }

        /* Sleep between sweeps — full sweep takes ~32ms, add breathing room */
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    vTaskDelete(NULL);
}

/* ── Public API ──────────────────────────────────────────────────────── */

esp_err_t nrf24_scanner_start(void) {
    esp_err_t ret;

    /* Init shared SPI bus */
    ret = nrf24_bus_init();
    if (ret != ESP_OK) return ret;

    /* Init Module 0 — WiFi sentinel */
    ret = nrf24_device_init(&s_mod0, NRF0_PIN_CE, NRF0_PIN_CSN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Module 0 init failed");
        return ret;
    }

    /* Init Module 1 — broadband sweeper */
    ret = nrf24_device_init(&s_mod1, NRF1_PIN_CE, NRF1_PIN_CSN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Module 1 init failed");
        nrf24_device_deinit(&s_mod0);
        return ret;
    }

    /* Create mutex and reset state */
    if (s_mutex == NULL) s_mutex = xSemaphoreCreateMutex();
    memset(&s_result, 0, sizeof(s_result));
    s_result.running = true;

    xTaskCreate(scanner_task, "nrf_scan", 4096, NULL, 4, &s_task);
    ESP_LOGI(TAG, "NRF24 scanner started. Mod0=WiFi sentinel Mod1=Broadband sweep.");
    return ESP_OK;
}

void nrf24_scanner_stop(void) {
    s_result.running = false;
    vTaskDelay(pdMS_TO_TICKS(600));  /* let task exit naturally */
    nrf24_device_deinit(&s_mod0);
    nrf24_device_deinit(&s_mod1);
    s_task = NULL;
    ESP_LOGI(TAG, "NRF24 scanner stopped.");
}

nrf24_scan_result_t nrf24_scanner_get_result(void) {
    nrf24_scan_result_t copy;
    if (s_mutex && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        copy = s_result;
        xSemaphoreGive(s_mutex);
    } else {
        memset(&copy, 0, sizeof(copy));
    }
    return copy;
}

int nrf24_scanner_to_json(char *buf, size_t buf_len) {
    nrf24_scan_result_t r = nrf24_scanner_get_result();

    int pos = 0;
    pos += snprintf(buf + pos, buf_len - pos,
        "{\"running\":%s,\"scan_count\":%lu,\"busy_count\":%d,"
        "\"jamming\":%s,\"wifi_ch1\":%s,\"wifi_ch6\":%s,\"wifi_ch11\":%s,"
        "\"channel_map\":[",
        r.running ? "true" : "false",
        (unsigned long)r.scan_count,
        r.busy_count,
        r.jamming_detected ? "true" : "false",
        r.wifi_ch1_busy    ? "true" : "false",
        r.wifi_ch6_busy    ? "true" : "false",
        r.wifi_ch11_busy   ? "true" : "false");

    for (int i = 0; i < NRF_SCAN_CHANNELS; i++) {
        if (pos >= (int)buf_len - 4) break;
        pos += snprintf(buf + pos, buf_len - pos,
                        "%d%s", r.channel_map[i],
                        (i < NRF_SCAN_CHANNELS - 1) ? "," : "");
    }
    pos += snprintf(buf + pos, buf_len - pos, "]}");
    return pos;
}
