#!/usr/bin/env python3
"""Run identical black-box scenarios against link++ and the real OCaml link.

Uses an independent Python frame encoder, PTYs, UDP sockets and an Ivy peer.
Timing-dependent report fields are checked by bounds, not by byte equality.
"""
import argparse
from contextlib import contextmanager
import json
import os
from pathlib import Path
import pty
import re
import select
import shutil
import socket
import struct
import subprocess
import tempfile
import termios
import threading
import time
import tty
import xml.etree.ElementTree as ET


def require(condition, explanation):
    if not condition:
        raise AssertionError(explanation)


allocated_ports = set()


def free_port():
    # The socket must close before the agent can bind. Remember selected ports
    # so consecutive calls cannot assign the same number to Ivy and the radio.
    while True:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.bind(('0.0.0.0', 0))
            port = sock.getsockname()[1]
        if port not in allocated_ports:
            allocated_ports.add(port)
            return port


def pprz(payload):
    data = bytes([len(payload) + 4]) + payload
    first = second = 0
    for byte in data:
        first = (first + byte) & 255
        second = (second + first) & 255
    return b'\x99' + data + bytes([first, second])


def api(payload):
    return b'\x7e' + struct.pack('>H', len(payload)) + payload + bytes([(255 - sum(payload)) & 255])


def telemetry(aircraft=42, value=1.25):
    return bytes([aircraft, 0, 1, 42, 7]) + struct.pack('<f', value)


def setting(aircraft=42, value=2.5):
    return bytes([0, aircraft, 2, 4, 3, aircraft]) + struct.pack('<f', value)


class Process:
    def __init__(self, command, environment):
        self.process = subprocess.Popen(command, env=environment, stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1)
        self.lines = []
        self.errors = []
        self.changed = threading.Condition()
        self.threads = []
        for stream, output in [(self.process.stdout, self.lines), (self.process.stderr, self.errors)]:
            thread = threading.Thread(target=self._read, args=(stream, output), daemon=True)
            thread.start()
            self.threads.append(thread)

    def _read(self, stream, output):
        for line in stream:
            with self.changed:
                output.append(line.rstrip('\n'))
                self.changed.notify_all()

    def send(self, line):
        self.process.stdin.write(line + '\n')
        self.process.stdin.flush()

    def wait(self, predicate, timeout=3, start=0):
        deadline = time.monotonic() + timeout
        with self.changed:
            while True:
                for line in self.lines[start:]:
                    if predicate(line):
                        return line
                remaining = deadline - time.monotonic()
                if remaining <= 0 or self.process.poll() is not None:
                    raise AssertionError(f'Message not received; stdout={self.lines!r}; stderr={self.errors!r}')
                self.changed.wait(min(remaining, .05))

    def close(self):
        if self.process.poll() is None:
            self.process.terminate()
        try:
            self.process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()
        for thread in self.threads:
            thread.join(timeout=1)
        self.process.stdin.close()
        self.process.stdout.close()
        self.process.stderr.close()
        diagnostics = '\n'.join(self.errors)
        require(not any(marker in diagnostics for marker in
                        ['ERROR: AddressSanitizer', 'LeakSanitizer', 'runtime error:', 'SUMMARY:']),
                f'Sanitizer diagnostic from subprocess: {diagnostics}')


class SerialWire:
    def __init__(self, xbee=False, extended=False):
        self.master, self.slave = pty.openpty()
        tty.setraw(self.slave)
        os.set_blocking(self.master, False)
        self.path = os.ttyname(self.slave)
        self.buffer = bytearray()
        self.xbee = xbee
        self.extended = extended
        self.commands = []

    def close(self):
        os.close(self.master)
        os.close(self.slave)

    def read(self, timeout):
        if select.select([self.master], [], [], max(0, timeout))[0]:
            self.buffer.extend(os.read(self.master, 65536))

    def initialize_modem(self):
        deadline = time.monotonic() + 9
        while time.monotonic() < deadline:
            self.read(.02)
            if self.buffer.startswith(b'+++'):
                del self.buffer[:3]
                self.commands.append('+++')
                os.write(self.master, b'OK\r')
            while b'\r' in self.buffer:
                command, _, rest = self.buffer.partition(b'\r')
                self.buffer[:] = rest
                self.commands.append(command.decode('ascii'))
                if command in (b'ATD6', b'ATD7'):
                    # Read back the setting written by the paired C++ initializer.
                    pin = command.decode('ascii')
                    setting = next((value[-1] for value in reversed(self.commands)
                                    if value.startswith(pin) and len(value) == 5), '0')
                    os.write(self.master, setting.encode('ascii') + b'\r')
                else:
                    os.write(self.master, b'OK\r')
                if command == b'ATCN':
                    time.sleep(.03)
                    return
        raise AssertionError(f'No ATCN: {self.commands}, pending={self.buffer!r}')

    def inject(self, payload, corrupt=False, fragmented=False):
        if self.xbee:
            radio = (b'\x90' + (42).to_bytes(8, 'big') + b'\xff\xfe\x00') if self.extended else b'\x81\x00\x2a\x46\x00'
            data = api(radio + payload)
        else:
            data = pprz(payload)
        if corrupt:
            data = data[:-1] + bytes([data[-1] ^ 1])
        if fragmented:
            os.write(self.master, data[:3])
            time.sleep(.02)
            os.write(self.master, data[3:])
        else:
            os.write(self.master, data)

    def frame(self, timeout=1):
        deadline = time.monotonic() + timeout
        marker = 0x7e if self.xbee else 0x99
        while True:
            if marker in self.buffer:
                del self.buffer[:self.buffer.index(marker)]
                if len(self.buffer) >= (3 if self.xbee else 2):
                    length = (int.from_bytes(self.buffer[1:3], 'big') + 4) if self.xbee else self.buffer[1]
                    if len(self.buffer) >= length:
                        frame = bytes(self.buffer[:length])
                        del self.buffer[:length]
                        if self.xbee:
                            require(sum(frame[3:]) & 255 == 255, 'Bad XBee checksum')
                            return frame[3:-1]
                        require(pprz(frame[2:-2]) == frame, 'Bad PPRZ checksum')
                        return frame[2:-2]
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return None
            self.read(min(.02, remaining))


class Harness:
    def __init__(self, executable, bus, directory):
        self.executable = executable
        self.bus = bus
        self.directory = directory
        self.environment = dict(os.environ, PPRZLINK_DIR=str(directory))

    @contextmanager
    def session(self, options=(), wire=None, udp=False, allow_exit=False):
        domain = f'127.255.255.255:{free_port()}'
        peer = Process([self.bus, domain], self.environment)
        agent = None
        try:
            peer.wait(lambda line: line == 'TEST_BUS_READY')
            command = [self.executable, '-b', domain, '-ping_period', '10000', '-status_period', '300']
            if wire:
                command += ['-d', wire.path]
            if udp:
                command += ['-udp']
            agent = Process(command + list(options), self.environment)
            peer.wait(lambda line: line == 'READY')
            if wire and wire.xbee:
                wire.initialize_modem()
            yield agent, peer
            if not allow_exit:
                require(agent.process.poll() is None, f'Agent stopped unexpectedly: {agent.errors}')
        except Exception as error:
            if agent:
                error.add_note(f'Agent stderr: {agent.errors}')
            raise
        finally:
            try:
                if agent:
                    agent.close()
            finally:
                peer.close()


def basic(harness):
    wire = SerialWire()
    try:
        with harness.session(['-fg'], wire) as (agent, peer):
            require(termios.tcgetattr(wire.slave)[4] == termios.B9600, 'Default baudrate is not 9600')
            peer.send('ground SETTING 3 42 2.5')
            require(wire.frame(.1) is None, 'Sent to an unknown aircraft')
            wire.inject(telemetry(), fragmented=True)
            line = peer.wait(lambda text: text == '42 LINK_TEST 7 1.25')
            peer.send('ground SETTING 3 42 2.5')
            command = wire.frame()
            require(command == setting(), f'Wrong SETTING: {command!r}')
            peer.send('ground LINK_BROADCAST -123')
            broadcast = wire.frame()
            require(broadcast == bytes([0, 255, 2, 44]) + struct.pack('<h', -123), 'Wrong broadcast')
            peer.send('ground LINK_UNROUTED 42')
            require(wire.frame(.1) is None, 'Untagged message was routed')
            wire.inject(telemetry(value=8.5), corrupt=True)
            wire.inject(telemetry(value=3.75))
            peer.wait(lambda text: text == '42 LINK_TEST 7 3.75')
            report = peer.wait(lambda text: text.startswith('link LINK_REPORT 42 ') and text.split()[7] == '2')
            fields = report.split()
            require(fields[6:9] == ['26', '2', '1'], f'Wrong RX statistics: {report}')
            require(fields[11] == '2', f'Wrong TX count: {report}')
            agent.wait(lambda text: re.fullmatch(r'\d+\.\d{3} 13', text) is not None)
            return {'telemetry': line, 'command': command.hex(), 'broadcast': broadcast.hex(),
                    'statistics': fields[2:4] + fields[6:9] + [fields[11]]}
    finally:
        wire.close()


def arrays(harness):
    wire = SerialWire()
    try:
        with harness.session(wire=wire) as (_, peer):
            payload = bytes([42, 0, 1, 43, 3]) + struct.pack('<hhh', -2, 0, 32767)
            payload += b'Hello' + bytes([11]) + b'hello world' + struct.pack('<d', 48.12345678)
            wire.inject(payload)
            expected = '42 LINK_ARRAYS -2,0,32767 "Hello" "hello world" 48.1234568'
            return peer.wait(lambda text: text == expected)
    finally:
        wire.close()


def integers(harness):
    wire = SerialWire()
    try:
        with harness.session(wire=wire) as (_, peer):
            payload = bytes([42, 0, 1, 44]) + struct.pack('<qQ', -(1 << 63), (1 << 64) - 1)
            payload += bytes([2]) + struct.pack('<qq', -1, (1 << 63) - 1)
            wire.inject(payload)
            line = peer.wait(lambda text: text.startswith('42 LINK_WIDE '))
            require(line == '42 LINK_WIDE -9223372036854775808 18446744073709551615 -1,9223372036854775807', line)
            peer.send('ground   SETTING    3  0x2a  +2.5  ')
            require(wire.frame() == setting(), 'Legacy numeric syntax or whitespace differs')
            return line
    finally:
        wire.close()


def recovery(harness):
    wire = SerialWire()
    try:
        with harness.session(wire=wire) as (_, peer):
            wire.inject(bytes([42, 0, 1, 255]))  # Unknown message ID.
            wire.inject(bytes([42, 0, 1, 42, 7]))  # Valid frame checksum, missing float.
            wire.inject(telemetry())
            line = peer.wait(lambda text: text == '42 LINK_TEST 7 1.25')
            peer.send('ground SETTING 3 42 2.5')
            require(wire.frame() == setting(), 'Malformed message stopped further processing')
            return line
    finally:
        wire.close()


def command_error(harness):
    wire = SerialWire()
    try:
        with harness.session(wire=wire, allow_exit=True) as (_, peer):
            wire.inject(telemetry())
            peer.wait(lambda text: text == '42 LINK_TEST 7 1.25')
            peer.send('ground SETTING 3 42 invalid')
            require(wire.frame(.1) is None, 'Invalid command was transmitted')
            peer.send('ground SETTING 3 42 2.5')
            return {'recovers': wire.frame(.4) == setting()}
    finally:
        wire.close()


def file_device(harness):
    wire = SerialWire()
    try:
        # A non-/dev path exercises the file/FIFO branch without applying termios.
        path = harness.directory / 'serial-file'
        path.symlink_to(wire.path)
        wire.path = str(path)
        with harness.session(wire=wire) as (_, peer):
            wire.inject(telemetry())
            line = peer.wait(lambda text: text == '42 LINK_TEST 7 1.25')
            peer.send('ground SETTING 3 42 2.5')
            require(wire.frame() == setting(), 'File-descriptor uplink differs')
            return line
    finally:
        (harness.directory / 'serial-file').unlink(missing_ok=True)
        wire.close()


def fifo(harness):
    path = harness.directory / 'telemetry.fifo'
    os.mkfifo(path)
    try:
        with harness.session(['-d', str(path), '-nouplink']) as (_, peer):
            descriptor = os.open(path, os.O_WRONLY | os.O_NONBLOCK)
            try:
                os.write(descriptor, pprz(telemetry()))
                return peer.wait(lambda line: line == '42 LINK_TEST 7 1.25')
            finally:
                os.close(descriptor)
    finally:
        path.unlink()


def flags(harness):
    results = []
    for options, enabled in [(['-nouplink'], False), (['-nouplink', '-uplink'], True),
                             (['-uplink', '-nouplink'], False)]:
        wire = SerialWire()
        try:
            with harness.session(options + ['-noac_info', '-hfc', '-s', '57600'], wire) as (_, peer):
                require(termios.tcgetattr(wire.slave)[4] == termios.B57600, 'Explicit baudrate ignored')
                require(termios.tcgetattr(wire.slave)[2] & termios.CRTSCTS, 'RTS/CTS ignored')
                wire.inject(telemetry())
                peer.wait(lambda text: text == '42 LINK_TEST 7 1.25')
                peer.send('ground ACINFO 42')
                peer.send('ground ACINFO_LLA 42')
                require(wire.frame(.12) is None, 'ACINFO was not disabled')
                peer.send('ground SETTING 3 42 2.5')
                frame = wire.frame(.15)
                require(frame == (setting() if enabled else None), 'Uplink flag order ignored')
                results.append(enabled)
        finally:
            wire.close()
    return results


def expiry(harness):
    results = []
    for timeout in [300, 0, -1]:
        wire = SerialWire()
        try:
            with harness.session(['-ac_timeout', str(timeout), '-status_period', '90'], wire) as (_, peer):
                wire.inject(telemetry())
                peer.wait(lambda text: text == '42 LINK_TEST 7 1.25')
                time.sleep(.42)
                peer.send('ground SETTING 3 42 2.5')
                frame = wire.frame(.12)
                require(frame == (None if timeout > 0 else setting()), 'Expiry policy differs')
                wire.inject(telemetry(value=9))
                peer.wait(lambda text: text == '42 LINK_TEST 7 9.00')
                peer.send('ground SETTING 3 42 2.5')
                require(wire.frame() == setting(), 'Aircraft did not revive')
                results.append(timeout)
        finally:
            wire.close()
    return results


def ping(harness):
    wire = SerialWire()
    try:
        with harness.session(['-ping_period', '400', '-status_period', '100', '-ac_timeout', '2000'], wire) as (_, peer):
            wire.inject(telemetry())
            peer.wait(lambda text: text == '42 LINK_TEST 7 1.25')
            require(wire.frame(1.2) == bytes([0, 42, 2, 8]), 'No PING')
            wire.inject(bytes([42, 0, 1, 5]))
            peer.wait(lambda text: text == '42 PONG')
            report = peer.wait(lambda text: text.startswith('link LINK_REPORT') and
                               text.split()[7] == '2' and 0 <= float(text.split()[12]) < 200)
            return {'pong': '42 PONG', 'rx': report.split()[6:8]}
    finally:
        wire.close()


def redundant(harness):
    wire = SerialWire()
    try:
        with harness.session(['-redlink', '-id=0x7', '-local_timestamp', '-nouplink'], wire) as (_, peer):
            wire.inject(telemetry())
            line = peer.wait(lambda text: text.startswith('redlink TELEMETRY_MESSAGE'))
            require(re.fullmatch(r'redlink TELEMETRY_MESSAGE 42 7 \d+\.\d{6};42;LINK_TEST;7;1.25', line), line)
            report = peer.wait(lambda text: 'link LINK_REPORT 42 7 ' in text)
            require(re.match(r'^\d+\.\d{6} link LINK_REPORT ', report), report)
            return re.sub(r'\d+\.\d{6};', '<time>;', line, count=1)
    finally:
        wire.close()


def udp(harness):
    downlink = free_port()
    uplink = free_port()
    sockets = []
    try:
        for address in ['127.0.0.1', '127.0.0.2']:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.bind((address, uplink))
            sock.settimeout(1)
            sockets.append(sock)
        with harness.session(['-udp_port', str(downlink), '-udp_uplink_port', str(uplink)], udp=True) as (_, peer):
            for aircraft, sock in zip([42, 43], sockets):
                sock.sendto(pprz(telemetry(aircraft)), ('127.0.0.1', downlink))
                peer.wait(lambda text: text == f'{aircraft} LINK_TEST 7 1.25')
            peer.send('ground SETTING 3 43 2.5')
            require(sockets[1].recv(65536) == pprz(setting(43)), 'UDP destination or uplink port differs')
            require(not select.select([sockets[0]], [], [], .05)[0], 'UDP command reached the wrong aircraft')
            peer.send('ground LINK_BROADCAST -123')
            expected = pprz(bytes([0, 255, 2, 44]) + struct.pack('<h', -123))
            for sock in sockets:
                require(sock.recv(65536) == expected, 'UDP broadcast missing an aircraft')
            return {'target': setting(43).hex(), 'broadcast': expected.hex(), 'aircraft': [42, 43]}
    finally:
        for sock in sockets:
            sock.close()


def udp_broadcast(harness):
    downlink, uplink = free_port(), free_port()
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as receiver:
        receiver.bind(('0.0.0.0', uplink))
        receiver.settimeout(1)
        options = ['-udp_port', str(downlink), '-udp_uplink_port', str(uplink),
                   '-udp_broadcast', '-udp_broadcast_addr', '127.255.255.255']
        with harness.session(options, udp=True) as (_, peer):
            receiver.sendto(pprz(telemetry()), ('127.0.0.1', downlink))
            peer.wait(lambda text: text == '42 LINK_TEST 7 1.25')
            peer.send('ground SETTING 3 42 2.5')
            received = receiver.recv(65536)
            require(received == pprz(setting()), 'UDP broadcast address was ignored')
            return received.hex()


def radio(harness, extended=False):
    wire = SerialWire(xbee=True, extended=extended)
    try:
        options = ['-transport', 'xbee'] + (['-xbee_868'] if extended else [])
        with harness.session(options, wire) as (_, peer):
            require('ATWR' not in wire.commands and not any(c.startswith('ATBD') for c in wire.commands),
                    'Compatible launch changed or persisted the baudrate')
            wire.inject(telemetry())
            peer.wait(lambda text: text == '42 LINK_TEST 7 1.25')
            if not extended:
                os.write(wire.master, api(b'\x01\x00\x2a\x46\x00' + telemetry(value=4)))
                peer.wait(lambda text: text == '42 LINK_TEST 7 4.00')
            peer.send('ground SETTING 3 42 2.5')
            frame = wire.frame()
            require(frame is not None, 'No XBee transmission')
            header = 14 if extended else 5
            require(frame[header:] == setting(), f'Wrong RF payload: {frame.hex()}')
            if not extended:
                require(frame[0] == 1 and frame[2:5] == b'\x00\x2a\x00', 'Wrong TX16 header')
            else:
                require(frame[2:10] == (42).to_bytes(8, 'big') and frame[10:14] == b'\xff\xfe\x00\x00',
                        'Wrong 868 address/reserved fields')
            failure = (bytes([0x8b, frame[1], 0xff, 0xfe, 0, 1, 0]) if extended
                       else bytes([0x89, frame[1], 1]))
            os.write(wire.master, api(failure))
            require(wire.frame(.8) == frame, 'Retry did not preserve frame ID and payload')
            success = bytearray(failure)
            success[5 if extended else 2] = 0
            os.write(wire.master, api(success))
            require(wire.frame(.25) is None, 'Retry continued after success')
            return {'payload': frame[header:].hex(), 'retry': 'same frame',
                    'format': frame[0] if extended else 1}
    finally:
        wire.close()


def serial_open_errors(harness):
    missing_device = f'/dev/{harness.directory.name}/missing-serial'
    require(not Path(missing_device).exists(), 'Missing-device fixture already exists')
    for device in [missing_device, '/dev/null']:
        result = subprocess.run([harness.executable, '-d', device], env=harness.environment,
                                text=True, capture_output=True, timeout=3)
        require(result.returncode == 1, f'Serial failure exit code: {result.returncode}')
        require(not result.stdout, f'Serial failure wrote to stdout: {result.stdout}')
        require(device in result.stderr and 'Cannot open serial port' in result.stderr,
                f'Serial diagnostic must identify the port: {result.stderr}')
        require('-d <port>' in result.stderr and f'{harness.executable} --help' in result.stderr,
                f'Serial diagnostic must explain how to select a port and get help: {result.stderr}')
        require('boost/asio' not in result.stderr and '[system:' not in result.stderr,
                f'Serial diagnostic exposed Boost implementation details: {result.stderr}')


def cli(harness):
    result = subprocess.run([harness.executable, '-help'], env=harness.environment,
                            text=True, capture_output=True, timeout=3)
    require(result.returncode == 0, result.stderr)
    options = sorted(re.findall(r'^\s+(-[a-z0-9_-]+)\b', result.stdout, re.M))
    # The virtual-port management command is an addition to the OCaml-compatible CLI.
    options = [option for option in options if option != '-socat']
    require(len(options) == 26, f'Missing CLI options: {options}')
    codes = []
    for args in [['--help'], ['-unknown'], ['-udp_port'], ['-id', 'invalid'],
                 ['-id=7', '--help'], ['-nouplink=1'], ['-help=1']]:
        run = subprocess.run([harness.executable] + args, env=harness.environment,
                             text=True, capture_output=True, timeout=3)
        codes.append(run.returncode)
    require(codes == [0, 2, 2, 2, 0, 2, 2], f'CLI exit codes: {codes}')
    return {'options': options, 'exit_codes': codes}


def production_xml(harness):
    directory = Path(__file__).resolve().parents[5] / 'message_definitions/v1.0'
    root = ET.parse(directory / 'messages.xml').getroot()
    telemetry_class = root.find("msg_class[@name='telemetry']")
    datalink_class = root.find("msg_class[@name='datalink']")
    pong = telemetry_class.find("message[@name='PONG']")
    command = datalink_class.find("message[@name='SETTING']")
    require(not list(pong.findall('field')), 'PONG schema changed; update this reference scenario')
    fields = command.findall('field')
    require([f.get('name') for f in fields] == ['index', 'ac_id', 'value'], 'SETTING schema changed')
    payload = bytes([42, 0, int(telemetry_class.get('id')), int(pong.get('id'))])
    expected = bytes([0, 42, int(datalink_class.get('id')), int(command.get('id')), 3, 42]) + struct.pack('<f', 2.5)
    production = Harness(harness.executable, harness.bus, directory)
    wire = SerialWire()
    try:
        with production.session(wire=wire) as (_, peer):
            wire.inject(payload)
            peer.wait(lambda line: line == '42 PONG')
            peer.send('ground SETTING 3 42 2.5')
            require(wire.frame() == expected, 'Real messages.xml routing differs')
            return expected.hex()
    finally:
        wire.close()


SCENARIOS = {'cli': cli, 'production_xml': production_xml, 'serial': basic, 'arrays': arrays, 'integers': integers,
             'recovery': recovery, 'command_error': command_error, 'file': file_device, 'fifo': fifo,
             'flags': flags, 'expiry': expiry,
             'ping': ping, 'redundant': redundant, 'udp': udp, 'udp_broadcast': udp_broadcast,
             'xbee': radio, 'xbee868': lambda harness: radio(harness, extended=True)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cpp', required=True)
    parser.add_argument('--ocaml')
    parser.add_argument('--bus', required=True)
    parser.add_argument('--only', choices=SCENARIOS, action='append')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    results = {}
    with tempfile.TemporaryDirectory(prefix='link-parity-') as temporary:
        directory = Path(temporary)
        shutil.copyfile(Path(__file__).with_name('link_messages.xml'), directory / 'messages.xml')
        executables = [('ocaml', args.ocaml), ('cpp', args.cpp)] if args.ocaml else [('cpp', args.cpp)]
        for label, executable in executables:
            harness = Harness(str(Path(executable).resolve()), str(Path(args.bus).resolve()), directory)
            results[label] = {}
            for name in args.only or SCENARIOS:
                print(f'{label}: {name}', flush=True)
                results[label][name] = SCENARIOS[name](harness)
                if label == 'cpp' and name == 'cli':
                    serial_open_errors(harness)
                if label == 'cpp' and name == 'command_error':
                    require(results[label][name]['recovers'], 'Invalid input stopped the C++ Ivy loop')
        if args.ocaml:
            for name, actual in results['cpp'].items():
                expected = results['ocaml'][name]
                if name == 'xbee868':
                    require(expected['format'] == 0 and actual['format'] == 0x10,
                            '868 divergence no longer matches the documented OCaml TX-type bug')
                    expected = dict(expected, format=0x10)
                if name == 'command_error' and expected != actual:
                    require(expected == {'recovers': False} and actual == {'recovers': True},
                            'Unexpected malformed-command difference')
                    continue  # Documented improvement: keep Ivy alive after bad input.
                require(actual == expected, f'{name} differs:\nOCaml {expected!r}\nC++ {actual!r}')
    if args.output:
        args.output.write_text(json.dumps(results, indent=2) + '\n')
    print(f'PASS: {len(results["cpp"])} scenarios' + (' compared with OCaml' if args.ocaml else ''), flush=True)


if __name__ == '__main__':
    main()
