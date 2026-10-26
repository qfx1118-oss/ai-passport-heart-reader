#!/usr/bin/env python3
"""Office editions: release 09:35, weekdays excluding published holidays."""
from datetime import datetime, timedelta, time, date
from pathlib import Path
from zoneinfo import ZoneInfo
import json

ZONE=ZoneInfo('Asia/Shanghai')
RELEASE=time(9,35)
CALENDAR_PATH=Path(__file__).resolve().parents[1]/'data/work-calendar.json'

def is_office_day(day):
    if day.weekday()>=5: return False
    years=json.loads(CALENDAR_PATH.read_text())['years']
    if str(day.year) not in years: raise ValueError(f'Holiday calendar for {day.year} must be verified before updating')
    return not any(a<=day.isoformat()<=b for a,b in years[str(day.year)]['holiday_ranges'])

def issue_plan(issue):
    if not is_office_day(issue): raise ValueError('No quote update on weekends or statutory holidays')
    monday=issue-timedelta(days=issue.weekday())
    first=not any(is_office_day(monday+timedelta(days=i)) for i in range(issue.weekday()))
    start=monday-timedelta(days=7) if first else monday
    end=monday-timedelta(days=1) if first else issue-timedelta(days=1)
    return {'issue_date':issue.isoformat(),'period':'week' if first else 'week_to_date',
        'start_date':start.isoformat(),'date':end.isoformat(),'covered_days':(end-start).days+1,
        'start_time':int(datetime.combine(start,time.min,ZONE).timestamp()*1000)-1,
        'end_time':int(datetime.combine(end+timedelta(days=1),time.min,ZONE).timestamp()*1000)-1,
        'title':'上周心力' if first else '本周心力'}

def edition(now=None):
    now=(now or datetime.now(ZONE)).astimezone(ZONE)
    issue=now.date() if now.time()>=RELEASE else now.date()-timedelta(days=1)
    for _ in range(370):
        if is_office_day(issue): return issue_plan(issue)
        issue-=timedelta(days=1)
    raise ValueError('No verified office edition available')

if __name__=='__main__':
    import argparse
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--prepare',action='store_true');a=p.parse_args()
    now=datetime.now(ZONE)
    if a.prepare:
        if not is_office_day(now.date()):
            print(json.dumps({'update':False,'reason':'weekend-or-holiday'}));raise SystemExit(0)
        plan=issue_plan(now.date());plan['update']=True
        start=date.fromisoformat(plan['start_date'])
        plan['missing_days']=[(start+timedelta(days=i)).isoformat() for i in range(plan['covered_days']) if not (CALENDAR_PATH.parents[1]/'private/day-buckets'/((start+timedelta(days=i)).isoformat()+'.json')).exists()]
    else: plan=edition(now)
    print(json.dumps(plan,ensure_ascii=False,indent=2))
