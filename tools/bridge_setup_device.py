#!/usr/bin/env python3
"""Receive one local setup payload on stdin, provision only the confirmed Passport."""
import json
import sys
import time
from pathlib import Path
import serial
from serial.tools import list_ports

cfg=json.loads(sys.stdin.read(1500))
expected=(Path(__file__).resolve().parents[1]/'private/confirmed-device-id').read_text().strip().upper()
ports=[p for p in list_ports.comports() if (p.serial_number or '').upper()==expected]
if len(ports)!=1: raise SystemExit('Confirmed AI Passport is not connected')
if not isinstance(cfg,dict): raise SystemExit('Invalid configuration')
for key,maximum in [('ssid',32),('password',64),('url',256),('key',64)]:
    v=cfg.get(key)
    if not isinstance(v,str) or len(v.encode())>maximum or any(ord(c)<32 for c in v): raise SystemExit('Invalid input')
if not cfg['ssid'] or len(cfg['key'])<32: raise SystemExit('Missing input')
packet=('W1 '+json.dumps(cfg,ensure_ascii=False,separators=(',',':'))+'\n').encode()
if len(packet)>643: raise SystemExit('Configuration too long')
with serial.Serial(port=None,baudrate=115200,timeout=0.25,write_timeout=5) as port:
    port.dtr=False;port.rts=False;port.port=ports[0].device;port.open();time.sleep(0.3);port.reset_input_buffer()
    for i in range(0,len(packet),128):
        port.write(packet[i:i+128]);port.flush();time.sleep(0.03)
    end=time.monotonic()+10
    while time.monotonic()<end:
        reply=port.readline()
        if b'W1 OK' in reply:
            print('SETUP_OK');break
        if b'W1 ERROR' in reply: raise SystemExit('Device rejected configuration')
    else: raise SystemExit('No acknowledgment from the confirmed AI Passport')
