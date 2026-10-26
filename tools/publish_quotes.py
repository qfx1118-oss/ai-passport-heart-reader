#!/usr/bin/env python3
"""Append reviewed day buckets and publish an office week's cumulative edition."""
import argparse
from datetime import datetime,timedelta
import json
import os
from pathlib import Path
import tempfile
from prepare_quotes import validate
from quote_window import issue_plan,ZONE

ROOT=Path(__file__).resolve().parents[1]
def atomic(path,payload):
    path.parent.mkdir(parents=True,exist_ok=True)
    fd,tmp=tempfile.mkstemp(prefix='.stage-',dir=path.parent)
    try:
        with os.fdopen(fd,'w',encoding='utf-8') as out:out.write(payload)
        os.replace(tmp,path)
    finally:
        if os.path.exists(tmp):os.unlink(tmp)

def publish(source,issue_date):
    window=issue_plan(datetime.fromisoformat(issue_date).date())
    raw=json.loads(Path(source).read_text(encoding='utf-8'))
    if raw['date']!=window['date']:raise ValueError('Content end date does not match edition')
    start=datetime.fromisoformat(window['start_date']).date()
    days=[(start+timedelta(days=i)).isoformat() for i in range(window['covered_days'])]
    reviewed=raw.get('reviewed_days',[])
    new={day:[] for day in reviewed}
    if any(day not in days for day in new):raise ValueError('Reviewed day outside the edition')
    for q in raw['quotes']:
        day=q.get('day')
        if day not in new:raise ValueError('Every new quote needs its reviewed source day')
        new[day].append(q)
    bucket_dir=ROOT/'private/day-buckets'
    merged=[];to_write={};seen=set()
    for day in days:
        path=bucket_dir/(day+'.json')
        if path.exists():bucket=json.loads(path.read_text())
        elif day in new:
            if len(new[day])>5:raise ValueError('At most five edited extracts per source day')
            bucket={'day':day,'quotes':sorted(new[day],key=lambda q:q['time'])};to_write[path]=bucket
        else:raise ValueError('Unreviewed source day: '+day)
        if bucket.get('day')!=day:raise ValueError('Invalid stored day bucket')
        for q in bucket['quotes']:
            if q.get('day')!=day:raise ValueError('Incorrect stored quote day')
            key=q['text'].strip()
            if key not in seen:merged.append(q);seen.add(key)
    pack={k:window[k] for k in ('period','start_date','issue_date','covered_days','date')}
    pack['quotes']=merged
    payload=validate(pack)
    for path,bucket in to_write.items():atomic(path,json.dumps(bucket,ensure_ascii=False,separators=(',',':')))
    target=ROOT/'private/editions'/(issue_date+'.json');atomic(target,payload)
    runtime=Path.home()/'Library/Application Support/AIPassportHeartBridge/private/editions'
    if runtime.is_dir():atomic(runtime/target.name,payload)
    return target

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('input',type=Path)
    p.add_argument('--issue-date',default=datetime.now(ZONE).date().isoformat());a=p.parse_args()
    print('Published cumulative edition:',publish(a.input,a.issue_date))
