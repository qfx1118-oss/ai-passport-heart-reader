#!/usr/bin/env python3
"""Configure the device once via USB; reads Wi-Fi credentials interactively."""
import argparse
import getpass
import json
from pathlib import Path
import time
from mac_bridge import device_key

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port',required=True)
    p.add_argument('--url',required=True,help='Mac LAN URL, e.g. http://MacBook-Pro.local:8765/v1/quotes')
    p.add_argument('--clear',action='store_true',help='Erase only saved reader network settings')
    a=p.parse_args()
    if a.clear: packet=b'W0\n'
    else:
        ssid=input('2.4 GHz Wi-Fi name: ')
        password=getpass.getpass('Wi-Fi password (not displayed): ')
        if not 1<=len(ssid.encode())<=32 or len(password.encode())>64: raise SystemExit('Invalid Wi-Fi credentials length')
        cfg={'ssid':ssid,'password':password,'url':a.url,'key':device_key()}
        packet=('W1 '+json.dumps(cfg,ensure_ascii=False,separators=(',',':'))+'\n').encode()
        if len(packet)>643: raise SystemExit('Configuration too long')
    import serial
    with serial.Serial(port=None,baudrate=115200,timeout=0.2,write_timeout=5) as port:
        port.dtr=False;port.rts=False;port.port=a.port;port.open();time.sleep(0.5)
        port.reset_input_buffer()
        for i in range(0,len(packet),128): port.write(packet[i:i+128]);port.flush();time.sleep(0.03)
        until=time.monotonic()+10
        while time.monotonic()<until:
            reply=port.readline()
            if b'W1 OK' in reply or b'W0 OK' in reply:
                print('Saved. Device is restarting.');return
            if b'W1 ERROR' in reply: raise SystemExit('Device rejected configuration')
        raise SystemExit('No acknowledgment; verify reader firmware and USB port')
if __name__=='__main__': main()
