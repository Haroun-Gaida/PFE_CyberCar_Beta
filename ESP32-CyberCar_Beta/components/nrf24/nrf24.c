/**
 * @file nrf24.c
 * @brief NRF24L01(+) driver — ESP-IDF SPI master implementation.
 *
 * Two modules share SPI2 (HSPI):
 *   Module 0 : CE=GPIO16  CSN=GPIO4
 *   Module 1 : CE=GPIO5   CSN=GPIO17
 */

#include "nrf24.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"   /* ets_delay_us() */

static const char *TAG = "nrf24";

static bool s_bus_initialized = false;

/* ── SPI bus ─────────────────────────────────────────────────────────── */

esp_err_t nrf24_bus_init(void) {
    if (s_bus_initialized) return ESP_OK;

    spi_bus_config_t bus = {
        .mosi_io_num   = NRF_PIN_MOSI,
        .miso_io_num   = NRF_PIN_MISO,
        .sclk_io_num   = NRF_PIN_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 32,
    };
    esp_err_t ret = spi_bus_initialize(NRF_SPI_HOST, &bus, SPI_DMA_DISABLED);
    if (ret == ESP_ERR_INVALID_STATE) {
        /* Bus already initialised by another driver (e.g. display) */
        ESP_LOGW(TAG, "SPI2 bus already init — sharing.");
        ret = ESP_OK;
    }
    if (ret == ESP_OK) s_bus_initialized = true;
    return ret;
}

void nrf24_bus_deinit(void) {
    if (!s_bus_initialized) return;
    spi_bus_free(NRF_SPI_HOST);
    s_bus_initialized = false;
}

/* ── Device init ─────────────────────────────────────────────────────── */

esp_err_t nrf24_device_init(nrf24_t *dev, gpio_num_t ce, gpio_num_t csn) {
    dev->ce  = ce;
    dev->csn = csn;
    dev->initialized = false;

    /* CE GPIO */
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << ce),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    gpio_set_level(ce, 0);  /* CE low = standby */

    /* SPI device — NRF24L01 is SPI mode 0, max 8 MHz */
    spi_device_interface_config_t devcfg = {
        .command_bits   = 0,
        .address_bits   = 0,
        .dummy_bits     = 0,
        .mode           = 0,              /* CPOL=0, CPHA=0 */
        .clock_speed_hz = 8 * 1000 * 1000,
        .spics_io_num   = csn,
        .queue_size     = 4,
        .flags          = 0,
    };
    esp_err_t ret = spi_bus_add_device(NRF_SPI_HOST, &devcfg, &dev->spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_add_device failed (CE=%d CSN=%d): %s",
                 ce, csn, esp_err_to_name(ret));
        return ret;
    }

    /* Power down, disable auto-ack, set 1 Mbps, 0 dBm */
    vTaskDelay(pdMS_TO_TICKS(5));                    /* power-on reset     */
    nrf24_write_reg(dev, NRF_REG_CONFIG,   0x00);   /* power down         */
    nrf24_write_reg(dev, NRF_REG_EN_AA,    0x00);   /* no auto-ack        */
    nrf24_write_reg(dev, NRF_REG_RF_SETUP, 0x06);   /* 1 Mbps, 0 dBm     */
    nrf24_set_channel(dev, 0);

    dev->initialized = true;
    ESP_LOGI(TAG, "NRF24 init OK (CE=GPIO%d CSN=GPIO%d)", ce, csn);
    return ESP_OK;
}

void nrf24_device_deinit(nrf24_t *dev) {
    if (!dev->initialized) return;
    nrf24_power_down(dev);
    spi_bus_remove_device(dev->spi);
    dev->initialized = false;
}

/* ── Register R/W ────────────────────────────────────────────────────── */

uint8_t nrf24_read_reg(nrf24_t *dev, uint8_t reg) {
    uint8_t tx[2] = { (uint8_t)(reg & 0x1F), 0xFF };
    uint8_t rx[2] = { 0, 0 };
    spi_transaction_t t = {
        .length    = 16,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    spi_device_polling_transmit(dev->spi, &t);
    return rx[1];
}

void nrf24_write_reg(nrf24_t *dev, uint8_t reg, uint8_t val) {
    uint8_t tx[2] = { (uint8_t)(0x20 | (reg & 0x1F)), val };
    spi_transaction_t t = {
        .length    = 16,
        .tx_buffer = tx,
        .rx_buffer = NULL,
    };
    spi_device_polling_transmit(dev->spi, &t);
}

/* ── CE control ──────────────────────────────────────────────────────── */

void nrf24_ce_high(nrf24_t *dev) { gpio_set_level(dev->ce, 1); }
void nrf24_ce_low(nrf24_t *dev)  { gpio_set_level(dev->ce, 0); }

/* ── Power / mode ────────────────────────────────────────────────────── */

void nrf24_power_up_rx(nrf24_t *dev) {
    nrf24_write_reg(dev, NRF_REG_CONFIG,
                    NRF_CFG_EN_CRC | NRF_CFG_PWR_UP | NRF_CFG_PRIM_RX);
    ets_delay_us(1500);  /* Tpd2stby = 1.5 ms */
}

void nrf24_power_down(nrf24_t *dev) {
    nrf24_ce_low(dev);
    nrf24_write_reg(dev, NRF_REG_CONFIG, 0x00);
}

/* ── RF channel ──────────────────────────────────────────────────────── */

void nrf24_set_channel(nrf24_t *dev, uint8_t ch) {
    if (ch > 125) ch = 125;
    nrf24_write_reg(dev, NRF_REG_RF_CH, ch);
}

uint8_t nrf24_get_channel(nrf24_t *dev) {
    return nrf24_read_reg(dev, NRF_REG_RF_CH) & 0x7F;
}

/* ── Carrier / power detection ───────────────────────────────────────── */

bool nrf24_carrier_detect(nrf24_t *dev) {
    /* CE must be high for ≥170 µs before reading RPD/CD */
    nrf24_ce_high(dev);
    ets_delay_us(200);
    bool detected = (nrf24_read_reg(dev, NRF_REG_RPD) & 0x01) != 0;
    nrf24_ce_low(dev);
    return detected;
}

void nrf24_scan_all_channels(nrf24_t *dev,
                              uint8_t  energy_map[NRF_NUM_CHANNELS],
                              uint32_t sweep_us) {
    if (sweep_us < 170) sweep_us = 250;

    nrf24_power_up_rx(dev);

    for (int ch = 0; ch < NRF_NUM_CHANNELS; ch++) {
        nrf24_set_channel(dev, (uint8_t)ch);
        nrf24_ce_high(dev);
        ets_delay_us(sweep_us);
        energy_map[ch] = (nrf24_read_reg(dev, NRF_REG_RPD) & 0x01) ? 1 : 0;
        nrf24_ce_low(dev);
    }

    nrf24_power_down(dev);
}
