#include "quotes_ui.h"
#include <stdio.h>
#include <string.h>
LV_FONT_DECLARE(quote_font_20);
LV_FONT_DECLARE(quote_font_18);
LV_FONT_DECLARE(quote_font_14);
LV_FONT_DECLARE(quote_font_28);
quotes_ui_t quotes_ui;
static char content_start[11],content_end[11];
static bool content_week;
static time_t display_now;
static void update_title(void) {
    if(!content_end[0]) return;
    if(display_now<1577836800) return;
    struct tm local;localtime_r(&display_now,&local);
    local.tm_hour=12;local.tm_min=local.tm_sec=0;
    char yesterday[11],monday[11],previous[11];
    local.tm_mday--;mktime(&local);strftime(yesterday,sizeof(yesterday),"%Y-%m-%d",&local);
    localtime_r(&display_now,&local);local.tm_hour=12;
    local.tm_mday-=(local.tm_wday+6)%7;mktime(&local);strftime(monday,sizeof(monday),"%Y-%m-%d",&local);
    local.tm_mday-=7;mktime(&local);strftime(previous,sizeof(previous),"%Y-%m-%d",&local);
    const char *title=content_week?(!strcmp(content_start,monday)?"本周心力":!strcmp(content_start,previous)?"上周心力":"往期心力")
        :(!strcmp(content_end,yesterday)?"昨日心力":"往期心力");
    lv_label_set_text(quotes_ui.title,title);
}
static lv_obj_t *label(lv_obj_t *screen,int x,int y,int width,const lv_font_t *font,uint32_t color) {
    lv_obj_t *l=lv_label_create(screen);lv_obj_set_pos(l,x,y);lv_obj_set_width(l,width);
    lv_obj_set_style_text_font(l,font,0);lv_obj_set_style_text_color(l,lv_color_hex(color),0);
    lv_label_set_long_mode(l,LV_LABEL_LONG_WRAP);return l;
}
void quotes_ui_context(const quotes_context_t *context,time_t now) {
    display_now=now;
    update_title();
    if(now<1577836800) {
        lv_label_set_text(quotes_ui.date,"日期待同步");lv_label_set_text(quotes_ui.clock,"--:--:--");
        lv_label_set_text(quotes_ui.lunar,"农历待同步");lv_label_set_text(quotes_ui.weather,"锦江区 · 天气待更新");return;
    }
    struct tm local;localtime_r(&now,&local);
    char day[11],date[40],clock[12];strftime(day,sizeof(day),"%Y-%m-%d",&local);
    static const char *week[]={"日","一","二","三","四","五","六"};
    snprintf(date,sizeof(date),"%04d.%02d.%02d  周%s",local.tm_year+1900,local.tm_mon+1,local.tm_mday,week[local.tm_wday]);
    strftime(clock,sizeof(clock),"%H:%M:%S",&local);
    lv_label_set_text(quotes_ui.date,date);lv_label_set_text(quotes_ui.clock,clock);
    const char *lunar="农历待同步";
    for(unsigned i=0;i<context->calendar_count;i++) if(!strcmp(day,context->calendar[i].date)) { lunar=context->calendar[i].lunar;break; }
    lv_label_set_text(quotes_ui.lunar,lunar);
    if(context->weather_valid && now>=context->weather_observed_at && now-context->weather_observed_at<=3600)
        lv_label_set_text_fmt(quotes_ui.weather,"锦江区  %d°C  %s",context->temperature,context->condition);
    else lv_label_set_text(quotes_ui.weather,"锦江区 · 天气待更新");
}
void quotes_ui_render(const quotes_pack_t *pack,const quotes_model_t *model) {
    strcpy(content_start,pack->start_date);strcpy(content_end,pack->date);
    content_week=pack->weekly||pack->cumulative;
    lv_label_set_text(quotes_ui.title,pack->weekly?"上周心力":pack->cumulative?"本周心力":"昨日心力");
    update_title();
    if(pack->covered_days) lv_label_set_text_fmt(quotes_ui.accumulation,"%u/7天 · %u条",pack->covered_days,(unsigned)pack->count);
    else lv_label_set_text(quotes_ui.accumulation,"等待周累积");
    for(unsigned i=0;i<7;i++) lv_obj_set_style_bg_color(quotes_ui.days[i],lv_color_hex(i<pack->covered_days?0x739BC9:0xC4D3E4),0);
    if(pack->count) {
        const quote_t *q=&pack->quotes[model->index];lv_label_set_text(quotes_ui.body,q->text);
        lv_label_set_text(quotes_ui.source,q->source);
        if(q->day[0]) lv_label_set_text_fmt(quotes_ui.time,"%.5s %s",q->day+5,q->time);
        else lv_label_set_text(quotes_ui.time,q->time);
        lv_label_set_text_fmt(quotes_ui.count,"%02u / %02u",(unsigned)model->index+1,(unsigned)pack->count);
    } else {
        lv_label_set_text(quotes_ui.body,"暂无内容\n请用电脑同步");lv_label_set_text(quotes_ui.source,"录音提炼");
        lv_label_set_text(quotes_ui.time,"");lv_label_set_text(quotes_ui.count,"00 / 00");
    }
}
bool quotes_ui_create(void) {
    lv_obj_t *screen=lv_obj_create(NULL);lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_color(screen,lv_color_hex(0xF5F7FA),0);lv_obj_set_style_bg_grad_color(screen,lv_color_hex(0xDCE7F2),0);
    lv_obj_set_style_bg_grad_dir(screen,LV_GRAD_DIR_VER,0);lv_obj_set_style_bg_opa(screen,LV_OPA_COVER,0);
    lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
    quotes_ui.date=label(screen,21,15,160,&quote_font_14,0x5D6978);
    quotes_ui.battery=label(screen,184,15,40,&lv_font_montserrat_14,0x5D6978);lv_label_set_text(quotes_ui.battery,"--%");
    quotes_ui.clock=label(screen,20,36,136,&quote_font_28,0x20262D);
    quotes_ui.lunar=label(screen,151,47,76,&quote_font_14,0x5D6978);
    quotes_ui.weather=label(screen,21,77,202,&quote_font_14,0x345F91);
    quotes_ui.title=label(screen,21,105,99,&quote_font_20,0x20262D);
    quotes_ui.accumulation=label(screen,128,110,99,&quote_font_14,0x5D6978);
    for(unsigned i=0;i<7;i++) {
        quotes_ui.days[i]=lv_obj_create(screen);lv_obj_remove_style_all(quotes_ui.days[i]);
        lv_obj_set_pos(quotes_ui.days[i],21+(int)i*29,134);lv_obj_set_size(quotes_ui.days[i],24,4);
        lv_obj_set_style_radius(quotes_ui.days[i],2,0);lv_obj_set_style_bg_opa(quotes_ui.days[i],LV_OPA_COVER,0);
    }
    lv_obj_t *card=lv_obj_create(screen);lv_obj_remove_style_all(card);lv_obj_set_pos(card,14,149);lv_obj_set_size(card,212,142);
    lv_obj_set_style_radius(card,20,0);lv_obj_set_style_bg_color(card,lv_color_hex(0xFFFFFF),0);lv_obj_set_style_bg_opa(card,LV_OPA_COVER,0);
    lv_obj_set_style_shadow_color(card,lv_color_hex(0xA8B9CC),0);lv_obj_set_style_shadow_width(card,12,0);
    lv_obj_set_style_shadow_ofs_y(card,4,0);lv_obj_set_style_shadow_opa(card,LV_OPA_20,0);lv_obj_remove_flag(card,LV_OBJ_FLAG_SCROLLABLE);
    quotes_ui.body=label(screen,27,159,186,&quote_font_18,0x20262D);lv_obj_set_height(quotes_ui.body,100);
    lv_obj_set_style_text_line_space(quotes_ui.body,2,0);
    quotes_ui.source=label(screen,27,271,86,&quote_font_14,0x5D6978);lv_label_set_long_mode(quotes_ui.source,LV_LABEL_LONG_DOT);lv_obj_set_height(quotes_ui.source,18);
    quotes_ui.time=label(screen,119,272,95,&lv_font_montserrat_14,0x5D6978);
    quotes_ui.count=label(screen,26,299,85,&lv_font_montserrat_14,0x5D6978);
    quotes_ui.status=label(screen,137,298,77,&quote_font_14,0x5D6978);
    quotes_context_t empty={0};quotes_ui_context(&empty,0);lv_screen_load(screen);return true;
}
