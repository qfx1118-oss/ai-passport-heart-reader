#include "quotes_pack.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

static bool field(cJSON *object, const char *key, char *out, size_t capacity, size_t max_chars) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!cJSON_IsString(item) || !item->valuestring) return false;
    size_t n = strlen(item->valuestring), count = 0;
    if (!n || n >= capacity) return false;
    const unsigned char *s = (const unsigned char *)item->valuestring;
    for (size_t i = 0; i < n;) {
        uint32_t cp;
        unsigned width;
        if (s[i] < 0x80) { width = 1; cp = s[i]; }
        else if (s[i] >= 0xC2 && s[i] <= 0xDF) { width = 2; cp = s[i] & 0x1F; }
        else if (s[i] >= 0xE0 && s[i] <= 0xEF) { width = 3; cp = s[i] & 0x0F; }
        else if (s[i] >= 0xF0 && s[i] <= 0xF4) { width = 4; cp = s[i] & 7; }
        else return false;
        if (i + width > n) return false;
        for (unsigned j = 1; j < width; j++) {
            if ((s[i+j] & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (s[i+j] & 0x3F);
        }
        if (cp < 32 || (width == 2 && cp < 0x80) || (width == 3 && cp < 0x800)
            || (width == 4 && cp < 0x10000) || cp > 0x10FFFF
            || (cp >= 0xD800 && cp <= 0xDFFF)) return false;
        count++;
        i += width;
    }
    if (count > max_chars) return false;
    memcpy(out, item->valuestring, n + 1);
    return true;
}
static bool valid_date(const char *s) {
    if (strlen(s) != 10 || s[4] != '-' || s[7] != '-') return false;
    for (int i = 0; i < 10; i++) if (i != 4 && i != 7 && !isdigit((unsigned char)s[i])) return false;
    int y, m, d;
    if (sscanf(s, "%4d-%2d-%2d", &y, &m, &d) != 3 || y < 1 || m < 1 || m > 12) return false;
    static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int max = days[m-1] + (m == 2 && (y % 400 == 0 || (y % 4 == 0 && y % 100 != 0)));
    return d >= 1 && d <= max;
}
esp_err_t quotes_pack_parse(const char *json, quotes_pack_t *pack) {
    if (!json || !pack || strlen(json) > QUOTES_JSON_MAX) return ESP_ERR_INVALID_ARG;
    const char *end = NULL;
    cJSON *root = cJSON_ParseWithOpts(json, &end, true);
    if (!root) return ESP_ERR_INVALID_ARG;
    bool ok = cJSON_IsObject(root) && field(root, "date", pack->date, sizeof(pack->date), 10)
              && valid_date(pack->date);
    pack->weekly = false;
    pack->cumulative=false;
    pack->covered_days=0;
    memcpy(pack->start_date, pack->date, sizeof(pack->date));
    cJSON *period = cJSON_GetObjectItemCaseSensitive(root, "period");
    if (period) {
        ok = ok && cJSON_IsString(period) &&
            (!strcmp(period->valuestring, "day") || !strcmp(period->valuestring, "week") || !strcmp(period->valuestring,"week_to_date"));
        pack->weekly = ok && !strcmp(period->valuestring, "week");
        pack->cumulative=ok && !strcmp(period->valuestring,"week_to_date");
    }
    if (pack->weekly || pack->cumulative) {
        ok = ok && field(root,"start_date",pack->start_date,sizeof(pack->start_date),10)
            && valid_date(pack->start_date) && strcmp(pack->start_date,pack->date) <= 0;
    }
    cJSON *coverage=cJSON_GetObjectItemCaseSensitive(root,"covered_days");
    if(coverage) {
        ok=ok && cJSON_IsNumber(coverage) && coverage->valuedouble==coverage->valueint && coverage->valueint>=1 && coverage->valueint<=7;
        if(ok) pack->covered_days=(unsigned)coverage->valueint;
    }
    if(pack->cumulative && !coverage) ok=false;
    if(pack->weekly && coverage && pack->covered_days!=7) ok=false;
    if(pack->weekly) pack->covered_days=7;
    cJSON *quotes = cJSON_GetObjectItemCaseSensitive(root, "quotes");
    int count = cJSON_GetArraySize(quotes);
    ok = ok && cJSON_IsArray(quotes) && count > 0 && count <= QUOTES_MAX;
    pack->count = 0;
    for (int i = 0; ok && i < count; i++) {
        cJSON *q = cJSON_GetArrayItem(quotes, i);
        quote_t *out = &pack->quotes[i];
        ok = field(q, "text", out->text, sizeof(out->text), 48)
             && field(q, "source", out->source, sizeof(out->source), 10)
             && field(q, "time", out->time, sizeof(out->time), 5);
        if(ok) {
            cJSON *day=cJSON_GetObjectItemCaseSensitive(q,"day");
            if(day) ok=field(q,"day",out->day,sizeof(out->day),10)&&valid_date(out->day)
                && strcmp(out->day,pack->start_date)>=0 && strcmp(out->day,pack->date)<=0;
            else memcpy(out->day,pack->date,sizeof(out->day));
        }
        if (ok) {
            const char *t = out->time;
            ok = strlen(t) == 5 && t[2] == ':' && t[0] >= '0' && t[0] <= '2'
                 && t[1] >= '0' && t[1] <= '9' && t[3] >= '0' && t[3] <= '5'
                 && t[4] >= '0' && t[4] <= '9' && (t[0] != '2' || t[1] <= '3');
        }
    }
    cJSON_Delete(root);
    if (!ok) return ESP_ERR_INVALID_ARG;
    pack->count = (size_t)count;
    return ESP_OK;
}
