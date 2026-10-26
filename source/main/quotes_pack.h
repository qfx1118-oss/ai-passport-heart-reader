#pragma once
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#define QUOTES_MAX 48
#define QUOTES_JSON_MAX 9000

typedef struct {
    char text[193]; /* 48 UTF-8 characters, up to four bytes each. */
    char source[41];
    char time[6];
    char day[11];
} quote_t;
typedef struct {
    char date[11];
    char start_date[11];
    bool weekly;
    bool cumulative;
    unsigned covered_days;
    size_t count;
    quote_t quotes[QUOTES_MAX];
} quotes_pack_t;
esp_err_t quotes_pack_parse(const char *json, quotes_pack_t *pack);
