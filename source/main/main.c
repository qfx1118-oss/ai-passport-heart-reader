/* A quiet, offline quote reader. All UI/state access is serialized by the LVGL lock.
 * Button timer callbacks only enqueue. USB parsing, NVS and I2C stay in a worker. */
#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_battery.h"
#include "quotes_model.h"
#include "quotes_pack.h"
#include "quotes_ui.h"
#include "quotes_network.h"
#include "esp_system.h"
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "quotes_text.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/usb_serial_jtag.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#if !defined(QUOTES_NO_BUILTIN) && __has_include("quotes_builtin.h")
#include "quotes_builtin.h"
#else
#define QUOTES_BUILTIN_JSON ""
#endif

LV_FONT_DECLARE(quote_font_20);
LV_FONT_DECLARE(quote_font_18);
LV_FONT_DECLARE(quote_font_14);
static const char *TAG = "quotes";
static QueueHandle_t s_keys;
static quotes_pack_t s_pack;
static quotes_context_t s_context;
static time_t s_wall_second;
static quotes_model_t s_model;
static lv_timer_t *s_timer;
static bool s_keys_ok;
static SemaphoreHandle_t s_import_lock;
static bool s_nvs_ok;
static int s_brightness = -1;
static unsigned s_second = 99;
static uint64_t s_activity;
static uint64_t now_ms(void) { return (uint64_t)esp_timer_get_time() / 1000; }

static void render(void) {
    quotes_ui_render(&s_pack, &s_model);
    s_second = 99;
}
static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user) {
    (void)user;
    /* PRESS avoids iot_button merging fast taps into a delayed double-click. */
    if (ev == BSP_BTN_PRESS && s_keys) (void)xQueueSend(s_keys, &btn, 0);
}
static void tick(lv_timer_t *timer) {
    (void)timer;
    uint64_t now = now_ms();
    time_t wall=time(NULL);
    if(wall!=s_wall_second) { quotes_ui_context(&s_context,wall);s_wall_second=wall; }
    bsp_btn_t btn;
    bool dirty = false;
    while (xQueueReceive(s_keys, &btn, 0) == pdTRUE) {
        bool wake_only = s_brightness < 10;
        s_activity = now;
        if (wake_only) continue;
        if (btn == BSP_BTN_OK) quotes_model_toggle(&s_model, now);
        else quotes_model_step(&s_model, btn == BSP_BTN_UP ? -1 : 1, now);
        dirty = true;
    }
    if (quotes_model_tick(&s_model, now)) dirty = true;
    if (dirty) render();
    unsigned second = quotes_model_remaining(&s_model, now);
    if (second != s_second) {
        if (!s_keys_ok) lv_label_set_text(quotes_ui.status, "按键不可用");
        else if (!s_pack.count) lv_label_set_text(quotes_ui.status, "等待同步");
        else if (s_pack.count == 1) lv_label_set_text(quotes_ui.status, "仅一条");
        else if (s_model.automatic) lv_label_set_text(quotes_ui.status, "自动轮播");
        else lv_label_set_text(quotes_ui.status, "已暂停");
        s_second = second;
    }
    int brightness = !s_model.automatic && now - s_activity >= 150000 ? 0
                   : now - s_activity >= 45000 ? 20 : 65;
    if (brightness != s_brightness) { bsp_display_backlight(brightness); s_brightness = brightness; }
}
static bool has_glyphs(const char *text) {
    uint32_t i = 0;
    while (text[i]) {
        uint32_t cp = quotes_utf8_next(text, &i);
        lv_font_glyph_dsc_t glyph = {0};
        if (!lv_font_get_glyph_dsc(&quote_font_18, &glyph, cp, 0) || glyph.is_placeholder) return false;
    }
    return true;
}
static bool pack_supported(const quotes_pack_t *p) {
    for (size_t i = 0; i < p->count; i++) {
        if (!has_glyphs(p->quotes[i].text) || !has_glyphs(p->quotes[i].source)) return false;
        lv_point_t size;
        lv_text_get_size(&size, p->quotes[i].text, &quote_font_18, 0, 2, 186, LV_TEXT_FLAG_NONE);
        if (size.y > 100) return false;
    }
    return true;
}
static esp_err_t store_json(const char *json) {
    if (!s_nvs_ok) return ESP_ERR_INVALID_STATE;
    nvs_handle_t handle;
    esp_err_t err = nvs_open("quote_reader", NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    err = nvs_set_str(handle, "pack", json);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}
static void load_snapshot(void) {
    (void)quotes_pack_parse(QUOTES_BUILTIN_JSON, &s_pack);
    if (!s_nvs_ok) return;
    nvs_handle_t handle;
    if (nvs_open("quote_reader", NVS_READONLY, &handle) != ESP_OK) return;
    size_t size = 0;
    if (nvs_get_str(handle, "pack", NULL, &size) == ESP_OK && size <= QUOTES_JSON_MAX+1) {
        char *json = malloc(size);
        quotes_pack_t *candidate = calloc(1, sizeof(*candidate));
        if (json && candidate && nvs_get_str(handle, "pack", json, &size) == ESP_OK
            && quotes_pack_parse(json, candidate) == ESP_OK) s_pack = *candidate;
        free(candidate);
        free(json);
    }
    nvs_close(handle);
}
static void usb_reply(const char *reply) {
    (void)usb_serial_jtag_write_bytes(reply, strlen(reply), pdMS_TO_TICKS(1000));
}
static esp_err_t import_pack(const char *json) {
    if (!s_import_lock || xSemaphoreTake(s_import_lock,pdMS_TO_TICKS(2000))!=pdTRUE) return ESP_ERR_TIMEOUT;
    quotes_pack_t *candidate = calloc(1, sizeof(*candidate));
    esp_err_t err = candidate ? quotes_pack_parse(json, candidate) : ESP_ERR_NO_MEM;
    bool valid = false;
    if (err == ESP_OK && bsp_lvgl_lock(1000)) {
        valid = pack_supported(candidate);
        bsp_lvgl_unlock();
    }
    if (err == ESP_OK && !valid) err = ESP_ERR_INVALID_ARG;
    if (err == ESP_OK) err = store_json(json);
    if (err == ESP_OK) {
        while (!bsp_lvgl_lock(1000)) vTaskDelay(pdMS_TO_TICKS(10));
        s_pack = *candidate;
        quotes_model_init(&s_model, s_pack.count, now_ms());
        s_activity = now_ms();
        render();
        bsp_lvgl_unlock();
    }
    free(candidate);
    xSemaphoreGive(s_import_lock);
    return err;
}
static esp_err_t import_context(const char *json) {
    quotes_context_t *candidate=calloc(1,sizeof(*candidate));
    if(!candidate) return ESP_ERR_NO_MEM;
    esp_err_t err=quotes_context_parse(json,candidate);
    if(err==ESP_OK) {
        struct timeval tv={.tv_sec=(time_t)candidate->epoch,.tv_usec=0};
        (void)settimeofday(&tv,NULL);
        if(bsp_lvgl_lock(1000)) { s_context=*candidate;quotes_ui_context(&s_context,time(NULL));bsp_lvgl_unlock(); }
        else err=ESP_ERR_TIMEOUT;
    }
    free(candidate);return err;
}
static void worker(void *arg) {
    (void)arg;
    usb_serial_jtag_driver_config_t cfg = {.tx_buffer_size=1024, .rx_buffer_size=1024};
    bool usb_ok = usb_serial_jtag_driver_install(&cfg) == ESP_OK;
    char *line = malloc(QUOTES_JSON_MAX+5);
    size_t used = 0;
    bool overflow = false;
    uint64_t battery_at = 0;
    uint64_t last_byte = now_ms();
    for (;;) {
        char chunk[128];
        int n = usb_ok && line ? usb_serial_jtag_read_bytes(chunk, sizeof(chunk), pdMS_TO_TICKS(50)) : 0;
        if (!usb_ok || !line) vTaskDelay(pdMS_TO_TICKS(50));
        if (used && now_ms() - last_byte > 5000) { used = 0; overflow = false; }
        for (int i = 0; i < n; i++) {
            last_byte = now_ms();
            if (chunk[i] == '\n') {
                line[used] = 0;
                if (overflow) usb_reply("\nQ1 ERROR oversized\n");
                else if (used > 3 && memcmp(line, "Q1 ", 3) == 0) {
                    usb_reply(import_pack(line+3)==ESP_OK ? "\nQ1 OK\n" : "\nQ1 ERROR rejected\n");
                } else if (used > 3 && memcmp(line,"W1 ",3)==0) {
                    if (quotes_network_configure(line+3)==ESP_OK) {
                        usb_reply("\nW1 OK rebooting\n");
                        (void)usb_serial_jtag_wait_tx_done(pdMS_TO_TICKS(1000));
                        esp_restart();
                    } else usb_reply("\nW1 ERROR invalid-config\n");
                } else if (!strcmp(line,"W0")) {
                    if (quotes_network_clear()==ESP_OK) {
                        usb_reply("\nW0 OK rebooting\n");
                        (void)usb_serial_jtag_wait_tx_done(pdMS_TO_TICKS(1000));
                        esp_restart();
                    }
                }
                used = 0;
                overflow = false;
            } else if (chunk[i] != '\r') {
                if (used < QUOTES_JSON_MAX+3) line[used++] = chunk[i];
                else overflow = true;
            }
        }
        if (now_ms() >= battery_at) {
            int soc = bsp_battery_soc();
            if (bsp_lvgl_lock(500)) {
                if (soc >= 0) lv_label_set_text_fmt(quotes_ui.battery, "%d%%", soc);
                else lv_label_set_text(quotes_ui.battery, "--%");
                bsp_lvgl_unlock();
            }
            ESP_LOGI(TAG, "heap=%u largest=%u", (unsigned)esp_get_free_heap_size(),
                     (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
            battery_at = now_ms() + 30000;
        }
    }
}
void app_main(void) {
    setenv("TZ","CST-8",1);tzset();
    ESP_LOGI(TAG, "Quote reader boot; offline, 10-second dwell");
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "Display initialization failed");
        return;
    }
    bsp_display_backlight(0);
    s_nvs_ok = nvs_flash_init() == ESP_OK; /* Never erase existing NVS on failure. */
    load_snapshot();
    (void)bsp_i2c_init();
    (void)bsp_battery_init();
    s_import_lock = xSemaphoreCreateMutex();
    if (!s_import_lock) { ESP_LOGE(TAG,"Import lock allocation failed"); return; }
    s_keys = xQueueCreate(16, sizeof(bsp_btn_t));
    if (!s_keys) { ESP_LOGE(TAG, "Input queue allocation failed"); return; }
    /* Build before registering input so early key events cannot access missing UI. */
    if (!bsp_lvgl_lock(1000)) { ESP_LOGE(TAG, "UI lock failed"); return; }
    if (!pack_supported(&s_pack)) s_pack.count = 0;
    quotes_model_init(&s_model, s_pack.count, now_ms());
    s_activity = now_ms();
    (void)quotes_ui_create();
    render();
    s_timer = lv_timer_create(tick, 100, NULL);
    bsp_lvgl_unlock();
    bool keys_ok = bsp_button_init(on_key, NULL) == ESP_OK;
    if (bsp_lvgl_lock(1000)) { s_keys_ok = keys_ok; s_second = 99; bsp_lvgl_unlock(); }
    if (!s_timer) ESP_LOGE(TAG, "UI timer allocation failed");
    (void)quotes_network_start(import_pack,import_context);
    if (xTaskCreate(worker, "quote_sync", 6144, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "USB/battery worker allocation failed");
    }
}
