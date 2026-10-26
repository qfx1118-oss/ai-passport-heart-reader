#!/usr/bin/env python3
"""Local time/lunar calendar and free model weather near Jinjiang district center."""
from datetime import datetime,timedelta
from pathlib import Path
import json
import sys
import threading
import time
import urllib.request
from quote_window import ZONE

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'private/runtime-deps'))
LAT,LON=30.599,104.117
WEATHER_URL=f'https://api.open-meteo.com/v1/forecast?latitude={LAT}&longitude={LON}&current=temperature_2m,weather_code&timezone=Asia%2FShanghai'
_lock=threading.Lock()
_cache=None
_next_attempt=0

def weather_name(code):
    if code==0:return '晴'
    if code in (1,2):return '多云'
    if code==3:return '阴'
    if code in (45,48):return '雾'
    if code in (51,53,55,56,57):return '小雨'
    if code in (61,63,65,66,67,80,81,82):return '雨'
    if code in (71,73,75,77,85,86):return '雪'
    if code in (95,96,99):return '雷雨'
    return '天气未知'

def refresh_weather(now=None):
    global _cache,_next_attempt
    now=time.time() if now is None else now
    with _lock:
        if now<_next_attempt:return
        _next_attempt=now+300
    try:
        with urllib.request.urlopen(WEATHER_URL,timeout=8) as response:
            raw=json.loads(response.read(100000))
        current=raw['current'];temp=float(current['temperature_2m']);code=int(current['weather_code'])
        observed=int(datetime.fromisoformat(current['time']).replace(tzinfo=ZONE).timestamp())
        if not -60<=temp<=60 or observed>now+300 or now-observed>7200: raise ValueError('Invalid weather observation')
        value={'temperature':round(temp),'condition':weather_name(code),'observed_at':observed,'provider':'Open-Meteo','area':'锦江区','approximate':True}
        with _lock:_cache=value;_next_attempt=now+900
        path=ROOT/'private/weather-cache.json';path.parent.mkdir(parents=True,exist_ok=True)
        path.write_text(json.dumps(value,ensure_ascii=False))
    except (OSError,ValueError,KeyError,TypeError):pass

def context(now=None):
    now=(now or datetime.now(ZONE)).astimezone(ZONE)
    try:
        from lunar_python import Solar
        days=[]
        for i in range(32):
            day=now.date()+timedelta(days=i)
            lunar=Solar.fromYmd(day.year,day.month,day.day).getLunar()
            days.append({'date':day.isoformat(),'lunar':lunar.getMonthInChinese()+'月'+lunar.getDayInChinese()})
    except ImportError:days=[]
    with _lock:weather=dict(_cache) if _cache else None
    if weather is None:
        try:weather=json.loads((ROOT/'private/weather-cache.json').read_text())
        except (OSError,ValueError):pass
    return {'epoch':int(now.timestamp()),'calendar':days,'weather':weather}

def start_weather_worker():
    def worker():
        while True:refresh_weather();time.sleep(60)
    threading.Thread(target=worker,name='jinjiang-weather',daemon=True).start()
