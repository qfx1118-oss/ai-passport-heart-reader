#!/usr/bin/env python3
"""Portable macOS setup. Preparation never flashes or replaces a running service."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import tempfile
import webbrowser

ROOT=Path(__file__).resolve().parent
PYTHON=ROOT/'.venv/bin/python'

def run(args,**kwargs):
    return subprocess.run([str(x) for x in args],check=True,**kwargs)

def verify():
    manifest=json.loads((ROOT/'SHA256.json').read_text())
    for name,digest in manifest.items():
        p=(ROOT/name).resolve()
        if not p.is_relative_to(ROOT) or not p.is_file():raise SystemExit('Invalid package member: '+name)
        if hashlib.sha256(p.read_bytes()).hexdigest()!=digest:raise SystemExit('Checksum mismatch: '+name)
    print('Package file checksums: PASS')

def devices():
    code='from serial.tools.list_ports import comports; import json; print(json.dumps([dict(port=p.device,serial=p.serial_number,description=p.description) for p in comports()]))'
    return json.loads(run([PYTHON,'-c',code],capture_output=True,text=True).stdout)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--verify',action='store_true')
    p.add_argument('--prepare',action='store_true')
    p.add_argument('--devices',action='store_true')
    p.add_argument('--install',action='store_true')
    p.add_argument('--serial')
    p.add_argument('--approve-flash',action='store_true',help='Explicit permission for this package and the selected device')
    p.add_argument('--fresh-install',action='store_true',help='Explicitly replace partition layout/settings using the merged image')
    a=p.parse_args();verify()
    if not any((a.prepare,a.devices,a.install)):return
    if platform.system()!='Darwin':raise SystemExit('This package installer supports macOS only')
    if a.prepare:
        if not PYTHON.exists():run([sys.executable,'-m','venv',ROOT/'.venv'])
        run([PYTHON,'-m','pip','install','-r',ROOT/'requirements.txt'])
        private=ROOT/'private';private.mkdir(exist_ok=True);private.chmod(0o700)
        print('Prepared. Next: --devices, confirm the Passport by unplug/replug, then --install.')
    if a.devices:
        if not PYTHON.exists():raise SystemExit('Run --prepare first')
        print(json.dumps(devices(),ensure_ascii=False,indent=2))
    if not a.install:return
    if not a.serial or not a.approve_flash:raise SystemExit('Installation needs --serial and explicit --approve-flash consent')
    if not PYTHON.exists():raise SystemExit('Run --prepare first')
    # Refuse to replace an existing reader installation or occupy its port.
    runtime=Path.home()/'Library/Application Support/AIPassportHeartBridge'
    agent=Path.home()/'Library/LaunchAgents/local.ai-passport.heart-bridge.plist'
    if runtime.exists() or agent.exists():raise SystemExit('Existing reader installation preserved. Ask the Agent to inspect it instead of reinstalling.')
    import socket
    try:
        with socket.socket() as probe:probe.bind(('127.0.0.1',8765))
    except OSError:raise SystemExit('Port 8765 is occupied; preserve the existing service')
    matches=[d for d in devices() if (d['serial'] or '').upper()==a.serial.upper()]
    if len(matches)!=1:raise SystemExit('Requested serial is not uniquely connected')
    command=[PYTHON,'-m','esptool','--chip','esp32c3','--port',matches[0]['port']]
    info=run(command+['flash_id'],capture_output=True,text=True).stdout
    print('\n'.join(line for line in info.splitlines() if 'Chip is' in line or 'Detected flash size' in line))
    if 'Detected flash size: 8MB' not in info:raise SystemExit('Only the verified 8MB ESP32-C3 Passport is supported')
    firmware=ROOT/'firmware'
    if not a.fresh_install:
        with tempfile.TemporaryDirectory() as temp:
            table=Path(temp)/'partition.bin'
            run(command+['read_flash','0x8000','0x1000',table],stdout=subprocess.DEVNULL)
            expected=(firmware/'partition-table.bin').read_bytes()
            if table.read_bytes()[:len(expected)]!=expected:
                raise SystemExit('Partition layout differs: settings-preserving installation refused. Fresh installation requires separate --fresh-install consent and replaces settings.')
        images=['0x0',firmware/'bootloader.bin','0x8000',firmware/'partition-table.bin','0x10000',firmware/'application.bin']
    else:images=['0x0',firmware/'full.bin']
    run(command+['--baud','460800','--before','default_reset','--after','hard_reset','write_flash','--flash_mode','dio','--flash_freq','80m','--flash_size','8MB']+images)
    private=ROOT/'private';private.mkdir(exist_ok=True);private.chmod(0o700)
    (private/'confirmed-device-id').write_text(a.serial.upper())
    run([PYTHON,ROOT/'tools/install_mac_bridge.py'])
    print('Firmware and bridge installed. Enter Wi-Fi locally at http://127.0.0.1:8765/setup')
    webbrowser.open('http://127.0.0.1:8765/setup')

if __name__=='__main__':main()
