/**
 * @file nrf24.h
 * @brief Lightweight NRF24L01(+) driver for ESP-IDF.
 *
 * Two modules share SPI2 (HSPI) bus:
 *   SCK=GPIO18  MOSI=GPIO23  MISO=GPIO19
 *
 * Module 0 (WiFi channel monitor):  CE=GPIO16  CSN=GPIO4
 * Module 1 (Full 2.4GHz scanner):   CE=GPIO5   CSN=GPIO17
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Pin definitions ─────────────────────────────────────────────────── */
#define NRF_SPI_HOST        SPI2_HOST
#define NRF_PIN_SCK         18
#define NRF_PIN_MOSI        23
#define NRF_PIN_MISO        19

/* Module 0 — WiFi-channel monitor */
#define NRF0_PIN_CE         16
#define NRF0_PIN_CSN        4

/* Module 1 — Broadband 2.4GHz scanner */
#define NRF1_PIN_CE         5
#define NRF1_PIN_CSN        17

#define NRF_NUM_CHANNELS    126   /* 2.400–2.525 GHz, 1 MHz steps */

/* ── NRF24L01 register map ───────────────────────────────────────────── */
#define NRF_REG_CONFIG      0x00
#define NRF_REG_EN_AA       0x01
#define NRF_REG_RF_CH       0x05
#define NRF_REG_RF_SETUP    0x06
#define NRF_REG_STATUS      0x07
#define NRF_REG_RPD         0x09  /* Received Power Detector (nRF24L01+)  */
#define NRF_REG_CD          0x09  /* Carrier Detect        (nRF24L01)     */

/* CONFIG bits */
#define NRF_CFG_PRIM_RX     (1 << 0)
#define NRF_CFG_PWR_UP      (1 << 1)
#define NRF_CFG_EN_CRC      (1 << 3)

/* ── Device handle ───────────────────────────────────────────────────── */
typedef struct {
    spi_device_handle_t spi;
    gpio_num_t          ce;
    gpio_num_t          csn;
    bool                initialized;
} nrf24_t;

/* ── Init/deinit ─────────────────────────────────────────────────────── */

/**
 * @brief Init the shared SPI2 bus.  Call once before any nrf24_device_init().
 */
esp_err_t nrf24_bus_init(void);
void      nrf24_bus_deinit(void);

/**
 * @brief Add one NRF24L01 device to the bus.
 * @param dev   Handle to populate.
 * @param ce    CE  GPIO number.
 * @param csn   CSN GPIO number (hardware CS).
 */
esp_err_t nrf24_device_init(nrf24_t *dev, gpio_num_t ce, gpio_num_t csn);
void      nrf24_device_deinit(nrf24_t *dev);

/* ── Register access ─────────────────────────────────────────────────── */
uint8_t   nrf24_read_reg(nrf24_t *dev, uint8_t reg);
void      nrf24_write_reg(nrf24_t *dev, uint8_t reg, uint8_t val);

/* ── CE (chip-enable) ────────────────────────────────────────────────── */
void      nrf24_ce_high(nrf24_t *dev);
void      nrf24_ce_low(nrf24_t *dev);

/* ── Power / mode ────────────────────────────────────────────────────── */
void      nrf24_power_up_rx(nrf24_t *dev);
void      nrf24_power_down(nrf24_t *dev);

/* ── RF channel ──────────────────────────────────────────────────────── */
void      nrf24_set_channel(nrf24_t *dev, uint8_t ch);   /* 0–125         */
uint8_t   nrf24_get_channel(nrf24_t *dev);

/* ── Carrier / power detection ───────────────────────────────────────── */

/**
 * @brief Detect carrier on the current channel.
 *        Requires CE high for ≥170 µs before reading RPD/CD.
 * @return true if received power > -64 dBm detected.
 */
bool      nrf24_carrier_detect(nrf24_t *dev);

/**
 * @brief Sweep all 126 channels and populate energy_map[].
 *        Blocks for ~(126 * sweep_us) µs.
 * @param dev          NRF24 handle.
 * @param energy_map   Output: 126-byte array, 1 = energy detected, 0 = clear.
 * @param sweep_us     Dwell time per channel in µs (min 170, recommend 250).
 */
void      nrf24_scan_all_channels(nrf24_t *dev, uint8_t energy_map[NRF_NUM_CHANNELS],
                                  uint32_t sweep_us);

#ifdef __cplusplus
}
#endif
