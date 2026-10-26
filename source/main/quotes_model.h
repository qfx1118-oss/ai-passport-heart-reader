#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define QUOTE_INTERVAL_MS UINT64_C(10000)
typedef struct {
    size_t index;
    size_t count;
    bool automatic;
    uint64_t changed_ms;
} quotes_model_t;
void quotes_model_init(quotes_model_t *m, size_t count, uint64_t now);
void quotes_model_step(quotes_model_t *m, int direction, uint64_t now);
void quotes_model_toggle(quotes_model_t *m, uint64_t now);
bool quotes_model_tick(quotes_model_t *m, uint64_t now);
unsigned quotes_model_remaining(const quotes_model_t *m, uint64_t now);
