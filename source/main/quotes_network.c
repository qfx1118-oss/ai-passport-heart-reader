/* One permanent worker owns Wi-Fi and HTTP. Provisioning takes effect on reboot.
 * Credentials remain in NVS; nothing from this file logs passwords or tokens. */
#include "quotes_network.h"
#include "quotes_pack.h"
#include "cJSON.h"
#include "nvs.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define NET_CONFIG_MAX 640
static EventGroupHandle_t s_events;
static quotes_import_fn s_apply;
static quotes_import_fn s_context;
static const char *TAG = "quote_net";
typedef struct { char ssid[33], password[65], url[257], key[65]; } net_config_t;
static bool get_string(cJSON *root, const char *key, char *out, size_t cap, bool empty) {
    cJSON *v = cJSON_GetObjectItemCaseSensitive(root,key);
    if (!cJSON_IsString(v) || !v->valuestring || strlen(v->valuestring) >= cap
        || (!empty && !v->valuestring[0])) return false;
    for (const char *p=v->valuestring; *p; p++) if ((unsigned char)*p < 32) return false;
    strcpy(out,v->valuestring);
    return true;
}
static bool parse_config(const char *json, net_config_t *cfg) {
    if (!json || strlen(json) > NET_CONFIG_MAX) return false;
    cJSON *r=cJSON_ParseWithOpts(json,NULL,true);
    if (!r) return false;
    bool ok=get_string(r,"ssid",cfg->ssid,sizeof(cfg->ssid),false)
        && get_string(r,"password",cfg->password,sizeof(cfg->password),true)
        && get_string(r,"url",cfg->url,sizeof(cfg->url),false)
        && get_string(r,"key",cfg->key,sizeof(cfg->key),false);
    ok=ok && strlen(cfg->key)>=32 && (!strncmp(cfg->url,"http://",7) || !strncmp(cfg->url,"https://",8));
    cJSON_Delete(r);
    return ok;
}
esp_err_t quotes_network_configure(const char *json) {
    net_config_t cfg;
    if (!parse_config(json,&cfg)) return ESP_ERR_INVALID_ARG;
    memset(&cfg,0,sizeof(cfg));
    nvs_handle_t h;
    esp_err_t e=nvs_open("quote_reader",NVS_READWRITE,&h);
    if (e != ESP_OK) return e;
    e=nvs_set_str(h,"network",json);
    if (e==ESP_OK) e=nvs_commit(h);
    nvs_close(h);
    return e;
}
esp_err_t quotes_network_clear(void) {
    nvs_handle_t h;
    esp_err_t e=nvs_open("quote_reader",NVS_READWRITE,&h);
    if (e!=ESP_OK) return e;
    e=nvs_erase_key(h,"network");
    if (e==ESP_ERR_NVS_NOT_FOUND) e=ESP_OK;
    if (e==ESP_OK) e=nvs_commit(h);
    nvs_close(h);
    return e;
}
static void network_event(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg; (void)data;
    if (base==IP_EVENT && id==IP_EVENT_STA_GOT_IP) xEventGroupSetBits(s_events,1);
    if (base==WIFI_EVENT && id==WIFI_EVENT_STA_DISCONNECTED) xEventGroupClearBits(s_events,1);
}
static esp_err_t wifi_start(const net_config_t *cfg) {
    esp_err_t e=esp_netif_init(); if(e!=ESP_OK) return e;
    e=esp_event_loop_create_default(); if(e!=ESP_OK && e!=ESP_ERR_INVALID_STATE) return e;
    esp_netif_config_t nc=ESP_NETIF_DEFAULT_WIFI_STA();
    esp_netif_t *netif=esp_netif_new(&nc); if(!netif) return ESP_ERR_NO_MEM;
    e=esp_netif_attach_wifi_station(netif);
    if(e==ESP_OK) e=esp_wifi_set_default_wifi_sta_handlers();
    if(e!=ESP_OK) { esp_netif_destroy_default_wifi(netif); return e; }
    wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();
    e=esp_wifi_init(&init);
    if(e!=ESP_OK) { esp_netif_destroy_default_wifi(netif); return e; }
    esp_event_handler_instance_t wh=NULL, ih=NULL;
    e=esp_event_handler_instance_register(WIFI_EVENT,WIFI_EVENT_STA_DISCONNECTED,network_event,NULL,&wh);
    if(e==ESP_OK) e=esp_event_handler_instance_register(IP_EVENT,IP_EVENT_STA_GOT_IP,network_event,NULL,&ih);
    wifi_config_t wc={0};
    memcpy(wc.sta.ssid,cfg->ssid,strlen(cfg->ssid));
    memcpy(wc.sta.password,cfg->password,strlen(cfg->password));
    if(e==ESP_OK) e=esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if(e==ESP_OK) e=esp_wifi_set_mode(WIFI_MODE_STA);
    if(e==ESP_OK) e=esp_wifi_set_config(WIFI_IF_STA,&wc);
    memset(&wc,0,sizeof(wc));
    if(e==ESP_OK) e=esp_wifi_start();
    if(e==ESP_OK) { (void)esp_wifi_set_ps(WIFI_PS_MIN_MODEM); return ESP_OK; }
    if(wh) esp_event_handler_instance_unregister(WIFI_EVENT,WIFI_EVENT_STA_DISCONNECTED,wh);
    if(ih) esp_event_handler_instance_unregister(IP_EVENT,IP_EVENT_STA_GOT_IP,ih);
    (void)esp_wifi_deinit();
    esp_netif_destroy_default_wifi(netif);
    return e;
}
static unsigned fetch(const net_config_t *cfg, char *last_json,bool context) {
    char url[257];strcpy(url,cfg->url);
    if(context) {
        char *path=strstr(url,"/v1/quotes");
        if(!path || strcmp(path,"/v1/quotes") || (size_t)(path-url)+sizeof("/v1/context")>sizeof(url)) return 60;
        strcpy(path,"/v1/context");
    }
    esp_http_client_config_t hc={.url=url,.timeout_ms=8000,.buffer_size=512,
        .disable_auto_redirect=true,.crt_bundle_attach=esp_crt_bundle_attach};
    esp_http_client_handle_t client=esp_http_client_init(&hc);
    if(!client) return 120;
    char auth[80]; snprintf(auth,sizeof(auth),"Bearer %s",cfg->key);
    (void)esp_http_client_set_header(client,"Authorization",auth);
    esp_err_t e=esp_http_client_open(client,0);
    char *json=NULL;
    bool complete=false;
    if(e==ESP_OK) {
        int64_t len=esp_http_client_fetch_headers(client);
        if(esp_http_client_get_status_code(client)==200 && len>0 && len<=QUOTES_JSON_MAX) {
            json=malloc((size_t)len+1);
            int read=0;
            while(json && read<len) {
                int n=esp_http_client_read(client,json+read,(int)len-read);
                if(n<=0) break;
                read+=n;
            }
            if(json) { json[read]=0; complete=read==len; }
        }
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    if(complete && context) { (void)s_context(json); }
    if(complete && !context && strcmp(json,last_json) && s_apply(json)==ESP_OK) {
        strcpy(last_json,json);
        ESP_LOGI(TAG,"New edition saved");
    }
    free(json);
    return 60; /* Local polling: update within a minute after the Mac publishes. */
}
static void network_worker(void *arg) {
    net_config_t *cfg=arg;
    esp_err_t e=wifi_start(cfg);
    char *last_json=e==ESP_OK ? calloc(1,QUOTES_JSON_MAX+1) : NULL;
    if(!last_json) {
        ESP_LOGW(TAG,"Network unavailable; offline reader retained");
        memset(cfg,0,sizeof(*cfg)); free(cfg); vTaskDelete(NULL); return;
    }
    for(;;) {
        if(!(xEventGroupGetBits(s_events)&1)) {
            (void)esp_wifi_connect();
            (void)xEventGroupWaitBits(s_events,1,pdFALSE,pdFALSE,pdMS_TO_TICKS(10000));
        }
        unsigned wait=60;
        if(xEventGroupGetBits(s_events)&1) { fetch(cfg,last_json,true);wait=fetch(cfg,last_json,false); }
        vTaskDelay(pdMS_TO_TICKS(wait*1000));
    }
}
esp_err_t quotes_network_start(quotes_import_fn apply,quotes_import_fn context) {
    nvs_handle_t h;
    esp_err_t e=nvs_open("quote_reader",NVS_READONLY,&h);
    if(e!=ESP_OK) return e;
    char json[NET_CONFIG_MAX+1]; size_t n=sizeof(json);
    e=nvs_get_str(h,"network",json,&n); nvs_close(h);
    if(e!=ESP_OK) return e;
    net_config_t *cfg=calloc(1,sizeof(*cfg));
    if(!cfg) return ESP_ERR_NO_MEM;
    bool valid=parse_config(json,cfg); memset(json,0,sizeof(json));
    if(!valid) { free(cfg); return ESP_ERR_INVALID_ARG; }
    s_events=xEventGroupCreate();
    if(!s_events) { free(cfg); return ESP_ERR_NO_MEM; }
    s_apply=apply;s_context=context;
    if(xTaskCreate(network_worker,"quote_wifi",6144,cfg,3,NULL)!=pdPASS) {
        memset(cfg,0,sizeof(*cfg)); free(cfg); vEventGroupDelete(s_events); s_events=NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
