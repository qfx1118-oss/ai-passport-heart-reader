#pragma once
#include "lvgl.h"
#include "quotes_pack.h"
#include "quotes_model.h"
#include "quotes_context.h"
#include <time.h>
typedef struct {
    lv_obj_t *date, *clock, *lunar, *weather, *title, *body, *source, *time, *count, *status, *battery, *accumulation;
    lv_obj_t *days[7];
} quotes_ui_t;
extern quotes_ui_t quotes_ui;
bool quotes_ui_create(void);
void quotes_ui_context(const quotes_context_t *context,time_t now);
void quotes_ui_render(const quotes_pack_t *pack, const quotes_model_t *model);
