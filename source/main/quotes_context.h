#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#define QUOTES_CALENDAR_DAYS 32
typedef struct { char date[11]; char lunar[32]; } quotes_lunar_day_t;
typedef struct {
    int64_t epoch;
    quotes_lunar_day_t calendar[QUOTES_CALENDAR_DAYS];
    unsigned calendar_count;
    int temperature;
    int64_t weather_observed_at;
    bool weather_valid;
    char condition[25];
} quotes_context_t;
esp_err_t quotes_context_parse(const char *json,quotes_context_t *out);
