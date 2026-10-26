#!/usr/bin/env python3
"""Install/uninstall a user-level launch agent for the local quote bridge."""
import argparse
import os
from pathlib import Path
import plistlib
import subprocess
import sys
import shutil

ROOT=Path(__file__).resolve().parents[1]
LABEL='local.ai-passport.heart-bridge'
RUNTIME=Path.home()/'Library/Application Support/AIPassportHeartBridge'
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--uninstall',action='store_true')
    a=p.parse_args()
    target=Path.home()/'Library/LaunchAgents'/f'{LABEL}.plist'
    domain=f'gui/{os.getuid()}'
    if a.uninstall:
        subprocess.run(['launchctl','bootout',domain+'/'+LABEL],capture_output=True)
        if target.exists(): target.unlink()
        print('Removed the quote bridge login service. Content files retained.');return
    if target.exists():
        prior=plistlib.loads(target.read_bytes())
        if prior.get('Label')!=LABEL or not any(str(path) in prior.get('ProgramArguments',[]) for path in (ROOT/'tools/mac_bridge.py',RUNTIME/'tools/mac_bridge.py')):
            raise SystemExit('Existing unrelated launch agent preserved; inspect before replacing')
    (ROOT/'private').mkdir(exist_ok=True)
    target.parent.mkdir(parents=True,exist_ok=True)
    (RUNTIME/'tools').mkdir(parents=True,exist_ok=True)
    (RUNTIME/'private/editions').mkdir(parents=True,exist_ok=True)
    for name in ('mac_bridge.py','prepare_quotes.py','quote_window.py','bridge_setup_device.py','wifi_setup.html','office_context.py'):
        shutil.copy2(ROOT/'tools'/name,RUNTIME/'tools'/name)
    (RUNTIME/'data').mkdir(exist_ok=True)
    shutil.copy2(ROOT/'data/work-calendar.json',RUNTIME/'data/work-calendar.json')
    if (ROOT/'private/runtime-deps').is_dir():
        shutil.copytree(ROOT/'private/runtime-deps',RUNTIME/'private/runtime-deps',dirs_exist_ok=True)
    confirmed=ROOT/'private/confirmed-device-id'
    if confirmed.exists(): shutil.copy2(confirmed,RUNTIME/'private/confirmed-device-id')
    # Preserve the same per-device credential; never create an incompatible second key.
    sys.path.insert(0,str(ROOT/'tools'))
    from mac_bridge import device_key
    key=device_key()
    runtime_key=RUNTIME/'private/device-key'
    if runtime_key.exists() and runtime_key.read_text().strip()!=key:
        raise SystemExit('Runtime device key conflicts with the project key; preserve both and inspect')
    runtime_key.write_text(key);runtime_key.chmod(0o600)
    for pack in (ROOT/'private/editions').glob('*.json'):
        shutil.copy2(pack,RUNTIME/'private/editions'/pack.name)

    plist={'Label':LABEL,'ProgramArguments':[sys.executable,str(RUNTIME/'tools/mac_bridge.py')],
           'WorkingDirectory':str(RUNTIME),'RunAtLoad':True,'KeepAlive':True,'ThrottleInterval':15,
           'StandardOutPath':str(RUNTIME/'private/bridge.log'),'StandardErrorPath':str(RUNTIME/'private/bridge-error.log')}
    target.write_bytes(plistlib.dumps(plist)); target.chmod(0o600)
    subprocess.run(['launchctl','bootout',domain+'/'+LABEL],capture_output=True)
    subprocess.run(['launchctl','bootstrap',domain,str(target)],check=True)
    print('Quote bridge installed for this user and started; no admin access or power settings changed.')
if __name__=='__main__': main()
