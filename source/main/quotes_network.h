#pragma once
#include "esp_err.h"
typedef esp_err_t (*quotes_import_fn)(const char *json);
esp_err_t quotes_network_start(quotes_import_fn apply,quotes_import_fn context);
esp_err_t quotes_network_configure(const char *json);
esp_err_t quotes_network_clear(void);
