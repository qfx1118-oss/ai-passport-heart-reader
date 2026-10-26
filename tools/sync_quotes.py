#!/usr/bin/env python3
"""Import a reviewed local quote JSON via USB. Does not flash firmware."""
import argparse
import json
import time
from pathlib import Path
from prepare_quotes import validate

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('input', type=Path)
    p.add_argument('--port', required=True, help='Explicit device serial port')
    a = p.parse_args()
    payload = validate(json.loads(a.input.read_text(encoding='utf-8')))
    import serial
    with serial.Serial(port=None, baudrate=115200, timeout=0.2, write_timeout=5) as device:
        device.dtr = False
        device.rts = False
        device.port = a.port
        device.open()
        time.sleep(0.5)
        device.reset_input_buffer()
        # Send bounded chunks; USB CDC baud rate is nominal, but avoid RX-ring overrun.
        packet = ('Q1 ' + payload + '\n').encode()
        for offset in range(0, len(packet), 64):
            device.write(packet[offset:offset+64])
            device.flush()
            time.sleep(0.1)
        deadline = time.monotonic() + 12
        while time.monotonic() < deadline:
            reply = device.readline()
            if b'Q1 OK' in reply:
                print('Imported and saved. The device now displays this date and quote pack.')
                return
            if b'Q1 ERROR' in reply:
                raise SystemExit(reply.decode(errors='replace').strip())
        raise SystemExit('No acknowledgment. Check the port and that quote-reader firmware is running.')

if __name__ == '__main__': main()
