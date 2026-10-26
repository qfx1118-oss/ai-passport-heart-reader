#!/usr/bin/env python3
"""Validate a private quote pack and optionally embed it (never commit the data)."""
import argparse
import datetime
import json
from pathlib import Path

UI_TEXT = '心力球昨日心力录音提炼暂无内容请用电脑同步上一条下一条确认暂停播放自动手动电量未知按键不可用已暂停倒计时秒来源仅一条'

def inventory():
    chars = set(chr(i) for i in range(32, 127)) | set(UI_TEXT + '，。！？；：、“”‘’（）《》—…·')
    for high in range(0xA1, 0xF8):
        for low in range(0xA1, 0xFF):
            try:
                chars.add(bytes([high, low]).decode('gb2312'))
            except UnicodeDecodeError:
                pass
    return ''.join(sorted(chars))

def validate(pack):
    if not isinstance(pack, dict):
        raise ValueError('Expected an object')
    date = datetime.date.fromisoformat(pack['date'])
    if date.isoformat() != pack['date']:
        raise ValueError('Date must be YYYY-MM-DD')
    period = pack.get('period', 'day')
    if period not in ('day', 'week', 'week_to_date'):
        raise ValueError('Invalid period')
    if period in ('week','week_to_date'):
        start = datetime.date.fromisoformat(pack['start_date'])
        if start.weekday()!=0 or not 0<=(date-start).days<=6 or (period=='week' and (date-start).days!=6):
            raise ValueError('A weekly edition must cover Monday through Sunday')
        if pack.get('covered_days',7 if period=='week' else None) != (date-start).days+1:
            raise ValueError('Progress must match reviewed calendar days')
    quotes = pack['quotes']
    if not isinstance(quotes, list) or not 1 <= len(quotes) <= 48:
        raise ValueError('Expected 1..48 quotes')
    supported = set(inventory())
    for quote in quotes:
        for field, limit in [('text', 36), ('source', 10), ('time', 5)]:
            value = quote[field]
            if not isinstance(value, str) or not value or len(value) > limit:
                raise ValueError(f'{field} is empty or too long')
            if any(ord(c) < 32 or c not in supported for c in value):
                raise ValueError(f'{field} contains unsupported characters')
        if 'day' in quote:
            day=datetime.date.fromisoformat(quote['day'])
            if not pack.get('start_date',pack['date'])<=day.isoformat()<=pack['date']:raise ValueError('Source day outside edition')
        datetime.datetime.strptime(quote['time'], '%H:%M')
        if len(quote['time']) != 5:
            raise ValueError('Time must be HH:MM')
    data = json.dumps(pack, ensure_ascii=False, separators=(',', ':'))
    if len(data.encode()) > 9000:
        raise ValueError('Payload exceeds 9000 bytes')
    return data

if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('input', nargs='?', type=Path)
    p.add_argument('--header', type=Path)
    p.add_argument('--inventory', type=Path)
    a = p.parse_args()
    if a.inventory:
        a.inventory.parent.mkdir(parents=True, exist_ok=True)
        a.inventory.write_text(inventory(), encoding='utf-8')
    if a.input:
        data = validate(json.loads(a.input.read_text(encoding='utf-8')))
        if a.header:
            a.header.write_text('/* Private generated snapshot. Git-ignored. */\n#define QUOTES_BUILTIN_JSON ' + json.dumps(data, ensure_ascii=False) + '\n', encoding='utf-8')
        print(f'Validated {len(json.loads(data)["quotes"])} quotes for {json.loads(data)["date"]}')
