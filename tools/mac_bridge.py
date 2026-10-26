#!/usr/bin/env python3
"""Read-only LAN quote service. No MCP credentials, no LLM key, no external hosting."""
from datetime import date, datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import hashlib
import ipaddress
import json
import os
from pathlib import Path
import secrets
import subprocess
import sys
from prepare_quotes import validate
from quote_window import edition, issue_plan
from office_context import context, start_weather_worker

ROOT=Path(__file__).resolve().parents[1]
PRIVATE=ROOT/'private'
SETUP_TOKEN=secrets.token_urlsafe(32)
SETUP_PYTHON=sys.executable

def bridge_url():
    import socket
    host=subprocess.run(['scutil','--get','LocalHostName'],capture_output=True,text=True).stdout.strip()
    host=host or socket.gethostname().split('.')[0]
    return f'http://{host}.local:8765/v1/quotes'

def device_key():
    PRIVATE.mkdir(parents=True,exist_ok=True)
    path=PRIVATE/'device-key'
    try:
        fd=os.open(path,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
    except FileExistsError:
        return path.read_text().strip()
    key=secrets.token_hex(24)
    with os.fdopen(fd,'w') as out: out.write(key)
    return key

def select_pack(now=None, directory=None):
    folder=directory or PRIVATE/'editions'
    try:issue=edition(now)['issue_date']
    except ValueError:
        # Calendar verification blocks new generation, never serving an old pack.
        from quote_window import ZONE,RELEASE
        from datetime import timedelta
        local=(now or datetime.now(ZONE)).astimezone(ZONE)
        issue=(local.date() if local.time()>=RELEASE else local.date()-timedelta(days=1)).isoformat()
    for path in sorted(folder.glob('*.json'),reverse=True):
        try:
            if path.stem>issue: continue
            date.fromisoformat(path.stem)
            pack=json.loads(path.read_text(encoding='utf-8'))
            if not isinstance(pack,dict): continue
            expected=issue_plan(date.fromisoformat(path.stem))
            if pack.get('issue_date')!=path.stem: continue
            legacy=pack.get('period','day')=='day'
            if not legacy and (pack['date']!=expected['date'] or pack.get('period')!=expected['period']): continue
            return validate(pack).encode()
        except (ValueError,KeyError,TypeError,OSError): continue
    return None

class Handler(BaseHTTPRequestHandler):
    server_version='HeartBridge'
    def setup(self):
        super().setup(); self.connection.settimeout(3)
    def local_setup_request(self):
        return ipaddress.ip_address(self.client_address[0]).is_loopback and self.headers.get('Host') in ('127.0.0.1:8765','localhost:8765')
    def do_POST(self):
        if self.path!='/setup' or not self.local_setup_request(): self.send_error(403); return
        if self.headers.get('Origin') not in ('http://127.0.0.1:8765','http://localhost:8765') or not secrets.compare_digest(self.headers.get('X-Setup-Token',''),SETUP_TOKEN):
            self.send_error(403); return
        try:
            length=int(self.headers.get('Content-Length','0'))
            if length<2 or length>1024: raise ValueError()
            data=json.loads(self.rfile.read(length))
            if not isinstance(data,dict): raise ValueError()
            cfg={'ssid':data.get('ssid'),'password':data.get('password'),'url':bridge_url(),'key':self.server.device_key}
            result=subprocess.run([SETUP_PYTHON,str(ROOT/'tools/bridge_setup_device.py')],input=json.dumps(cfg),text=True,capture_output=True,timeout=18)
            if result.returncode or 'SETUP_OK' not in result.stdout:
                self.respond(400,json.dumps({'message':'未能保存，请确认 AI Passport 已通过 USB 连接，且没有其他程序占用串口。'},ensure_ascii=False).encode());return
            self.respond(200,json.dumps({'message':'已保存，AI Passport 正在重启并连接 Wi-Fi。请保持此页面，等待设备同步。'},ensure_ascii=False).encode())
        except (ValueError,TypeError,subprocess.TimeoutExpired,OSError):
            self.respond(400,json.dumps({'message':'配置未完成，请检查输入和 USB 连接后重试。'},ensure_ascii=False).encode())
    def do_GET(self):
        client=ipaddress.ip_address(self.client_address[0])
        if not (client.is_private or client.is_loopback):
            self.send_error(403); return
        if self.path=='/setup':
            if not self.local_setup_request(): self.send_error(403); return
            html=(ROOT/'tools/wifi_setup.html').read_text().replace('__SETUP_TOKEN__',SETUP_TOKEN).encode()
            self.send_response(200)
            self.send_header('Content-Type','text/html; charset=utf-8')
            self.send_header('Content-Length',str(len(html)))
            self.send_header('Cache-Control','no-store')
            self.send_header('Content-Security-Policy',"default-src 'self'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; frame-ancestors 'none'")
            self.end_headers();self.wfile.write(html);return
        if self.path=='/health':
            self.respond(200,b'{"service":"heart-bridge","ok":true}'); return
        if self.path not in ('/v1/quotes','/v1/context'): self.send_error(404); return
        auth=self.headers.get('Authorization','')
        if not secrets.compare_digest(auth,'Bearer '+self.server.device_key):
            self.send_error(401); return
        if self.path=='/v1/context':
            self.respond(200,json.dumps(context(),ensure_ascii=False,separators=(',',':')).encode());return
        payload=select_pack()
        if payload is None:
            self.respond(503,b'{"error":"no-edition-yet"}'); return
        etag='"'+hashlib.sha256(payload).hexdigest()+'"'
        if self.headers.get('If-None-Match')==etag:
            self.respond(304,b'',etag); return
        self.respond(200,payload,etag)
    def respond(self,status,payload,etag=None):
        self.send_response(status)
        self.send_header('Content-Type','application/json; charset=utf-8')
        self.send_header('Content-Length',str(len(payload)))
        self.send_header('Cache-Control','private, no-store')
        if etag: self.send_header('ETag',etag)
        self.end_headers()
        if payload: self.wfile.write(payload)
    def log_message(self,format,*args):
        # Only response status/path are logged; request headers and content are never logged.
        pass

if __name__=='__main__':
    import argparse
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--bind',default='0.0.0.0')
    p.add_argument('--port',type=int,default=8765)
    a=p.parse_args()
    start_weather_worker()
    server=ThreadingHTTPServer((a.bind,a.port),Handler)
    server.device_key=device_key()
    server.serve_forever()
