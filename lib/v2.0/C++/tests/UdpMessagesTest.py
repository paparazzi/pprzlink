#!/usr/bin/env python3
"""Send and receive a grouped command with two roles of the same UDP executable."""
import argparse
import os
from pathlib import Path
import socket
import struct
import subprocess
from LinkParityTest import Process, pprz, require


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable')
    args = parser.parse_args()
    dictionary = str(Path(__file__).resolve().parents[1] / 'examples/clients/client_messages.xml')
    for values in [[], [dictionary, '0'], [dictionary, '0', 'invalid']]:
        result = subprocess.run([args.executable, *values], capture_output=True, text=True, timeout=3)
        require(result.returncode == 2 and 'port e|r' in result.stderr, result.stdout + result.stderr)
    for port in ['', '0', '65536', '-1', '42extra']:
        result = subprocess.run([args.executable, dictionary, port, 'e'],
                                capture_output=True, text=True, timeout=3)
        require(result.returncode == 1 and 'Invalid UDP port' in result.stderr,
                result.stdout + result.stderr)

    # Check the exact emitted datagram independently from the receiving role.
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as listener:
        listener.bind(('127.0.0.1', 0))
        listener.settimeout(3)
        result = subprocess.run([args.executable, dictionary, str(listener.getsockname()[1]), 'e'],
                                capture_output=True, text=True, timeout=3)
        require(result.returncode == 0 and '(13 bytes)' in result.stdout, result.stdout + result.stderr)
        datagram, _ = listener.recvfrom(1024)
        require(datagram == pprz(bytes([0, 42, 2, 43, 42]) + struct.pack('<f', 12.5)),
                'The grouped command has incorrect header or field bytes')

    receiver = Process([args.executable, dictionary, '0', 'r'], dict(os.environ))
    try:
        ready = receiver.wait(lambda line: line.startswith('Listening on '))
        port = ready.split()[-1]
        result = subprocess.run([args.executable, dictionary, port, 'e'],
                                capture_output=True, text=True, timeout=3)
        require(result.returncode == 0, result.stdout + result.stderr)
        receiver.wait(lambda line: line.startswith('Received from 127.0.0.1:') and
                      line.endswith('[13 bytes] 0 CLIENT_SETTING 42 12.5'))
        require(receiver.process.wait(timeout=3) == 0, f'Receiver failed: {receiver.errors}')
    finally:
        receiver.close()
    print('PASS: UDP emission/reception, grouped fields, source metadata and port validation', flush=True)


if __name__ == '__main__':
    main()
