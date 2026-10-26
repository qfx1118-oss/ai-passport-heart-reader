#include "quotes_context.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
static bool text(cJSON *root,const char *name,char *out,size_t cap) {
    cJSON *v=cJSON_GetObjectItemCaseSensitive(root,name);
    if(!cJSON_IsString(v)||!v->valuestring||strlen(v->valuestring)>=cap) return false;
    for(const unsigned char *s=(const unsigned char *)v->valuestring;*s;s++) if(*s<32) return false;
    strcpy(out,v->valuestring);return true;
}
esp_err_t quotes_context_parse(const char *json,quotes_context_t *out) {
    if(!json || !out || strlen(json)>9000) return ESP_ERR_INVALID_ARG;
    cJSON *r=cJSON_ParseWithOpts(json,NULL,true);if(!r) return ESP_ERR_INVALID_ARG;
    cJSON *epoch=cJSON_GetObjectItemCaseSensitive(r,"epoch");
    bool ok=cJSON_IsObject(r)&&cJSON_IsNumber(epoch)&&epoch->valuedouble>=1577836800&&epoch->valuedouble<4102444800.;
    if(!ok) { cJSON_Delete(r);return ESP_ERR_INVALID_ARG; }
    memset(out,0,sizeof(*out));out->epoch=(int64_t)epoch->valuedouble;
    cJSON *days=cJSON_GetObjectItemCaseSensitive(r,"calendar");
    int n=cJSON_GetArraySize(days);
    if(!cJSON_IsArray(days)||n>QUOTES_CALENDAR_DAYS) ok=false;
    for(int i=0;ok&&i<n;i++) {
        cJSON *d=cJSON_GetArrayItem(days,i);
        ok=text(d,"date",out->calendar[i].date,sizeof(out->calendar[i].date))
            && strlen(out->calendar[i].date)==10
            && text(d,"lunar",out->calendar[i].lunar,sizeof(out->calendar[i].lunar));
        if(ok) out->calendar_count++;
    }
    cJSON *w=cJSON_GetObjectItemCaseSensitive(r,"weather");
    if(cJSON_IsObject(w)) {
        cJSON *t=cJSON_GetObjectItemCaseSensitive(w,"temperature");
        cJSON *at=cJSON_GetObjectItemCaseSensitive(w,"observed_at");
        if(cJSON_IsNumber(t)&&cJSON_IsNumber(at)&&t->valuedouble>=-60&&t->valuedouble<=60
            && at->valuedouble>=1577836800&&at->valuedouble<=out->epoch+300
            && text(w,"condition",out->condition,sizeof(out->condition))) {
            out->temperature=t->valueint;out->weather_observed_at=(int64_t)at->valuedouble;out->weather_valid=true;
        }
    }
    cJSON_Delete(r);return ok?ESP_OK:ESP_ERR_INVALID_ARG;
}
