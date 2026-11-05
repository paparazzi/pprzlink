#!/usr/bin/env python3
"""Exercise the public-API clients as real external processes, without radio hardware."""
import argparse
import os
from pathlib import Path
import socket
import struct
import subprocess
import tempfile
import xml.etree.ElementTree as ET
from LinkParityTest import Process, SerialWire, free_port, pprz, require
from PtyPeersTest import connected_ports


def serial_aircraft(executable, dictionary, altitude_payload=None):
    wire = SerialWire()
    peer = Process([executable, dictionary, wire.path, 's'], dict(os.environ))
    try:
        peer.wait(lambda line: line.startswith('Server ready on '))
        # A PING for another aircraft must not trigger either reply.
        wire.inject(bytes([7, 41, 2, 8]))
        require(wire.frame(.05) is None, 'Server answered a PING addressed to another aircraft')
        wire.inject(bytes([7, 42, 2, 8]))
        payload = altitude_payload if altitude_payload is not None else struct.pack('<f', 123.5)
        altitude = bytes([42, 7, 1, 42]) + payload
        require(wire.frame(4) == altitude, 'Aircraft example did not send its altitude')
        require(wire.frame(4) == bytes([42, 7, 1, 5]), 'Server did not address PONG to the PING sender')
        peer.wait(lambda line: line == 'Answered PING (8 received bytes)')
        require(peer.process.wait(timeout=4) == 0, f'Server failed: {peer.errors}')
    finally:
        peer.close()
        wire.close()


def serial_client(executable, dictionary, altitude_payload=None):
    wire = SerialWire()
    peer = Process([executable, dictionary, wire.path, 'c'], dict(os.environ))
    try:
        peer.wait(lambda line: line.startswith('Client ready on '))
        ping = bytes([0, 42, 2, 8])
        require(wire.frame(4) == ping, 'Client did not send PING to aircraft 42')
        require(wire.frame(2) == ping, 'Client did not retry while waiting for the server')
        payload = altitude_payload if altitude_payload is not None else struct.pack('<f', 123.5)
        # Ignore another sender's altitude and a PONG for another ground station.
        wire.inject(bytes([43, 0, 1, 42]) + payload)
        wire.inject(bytes([42, 7, 1, 5]))
        # Receiving PONG first must leave the client waiting for the altitude.
        wire.inject(bytes([42, 0, 1, 5]))
        peer.wait(lambda line: line == 'Received PONG (8 received bytes)')
        require(peer.process.poll() is None, 'Client stopped before receiving its altitude')
        wire.inject(bytes([42, 0, 1, 42]) + payload)
        peer.wait(lambda line: line == 'Aircraft 42: 123.5 m (12 received bytes)')
        require(peer.process.wait(timeout=4) == 0, f'Client failed: {peer.errors}')
    finally:
        peer.close()
        wire.close()
    require(sum(line.startswith('Aircraft ') for line in peer.lines) == 1,
            f'Client accepted an altitude from another sender: {peer.lines}')
    require(peer.lines.count('Received PONG (8 received bytes)') == 1,
            f'Client accepted a PONG addressed to another receiver: {peer.lines}')


def serial_peers(executable, dictionary):
    for arguments in [[], [dictionary, '/dev/null'], [dictionary, '/dev/null', 'invalid']]:
        result = subprocess.run([executable, *arguments], capture_output=True, text=True, timeout=3)
        require(result.returncode == 2 and 'serial-port c|s' in result.stderr,
                result.stdout + result.stderr)
    environment = dict(os.environ)
    for client_first in [False, True]:
        with connected_ports(None, environment) as pair:
            processes = {}
            try:
                order = [('s', pair[0], 'Server'), ('c', pair[1], 'Client')]
                if client_first:
                    order.reverse()
                for role, port, name in order:
                    peer = Process([executable, dictionary, port, role], environment)
                    processes[role] = peer
                    peer.wait(lambda line, name=name: line.startswith(name + ' ready on '))
                processes['c'].wait(lambda line: line == 'Aircraft 42: 123.5 m (12 received bytes)')
                processes['c'].wait(lambda line: line == 'Received PONG (8 received bytes)')
                processes['s'].wait(lambda line: line == 'Answered PING (8 received bytes)')
                for peer in processes.values():
                    require(peer.process.wait(timeout=3) == 0, f'Serial peer failed: {peer.errors}')
                print('Serial peers: ' + ('client' if client_first else 'server') + ' started first', flush=True)
            finally:
                for peer in processes.values():
                    peer.close()


def udp_recorder(executable, dictionary):
    process = subprocess.Popen([executable, dictionary, '0'], stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, text=True)
    try:
        line = process.stdout.readline().strip()
        require(line.startswith('Listening on '), f'UDP example did not expose its bound port: {line}')
        destination = ('127.0.0.1', int(line.split()[-1]))
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as first, socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as second:
            first.bind(('127.0.0.1', 0))
            second.bind(('127.0.0.1', 0))
            def message(altitude):
                return pprz(bytes([42, 0, 1, 42]) + struct.pack('<f', altitude))
            first.sendto(message(10) + message(20), destination)
            second.sendto(message(30), destination)
            output, errors = process.communicate(timeout=4)
            expected = [f'127.0.0.1:{first.getsockname()[1]} [12 bytes] 42 CLIENT_ALTITUDE {value}'
                        for value in [10, 20]]
            expected.append(f'127.0.0.1:{second.getsockname()[1]} [12 bytes] 42 CLIENT_ALTITUDE 30')
            require(process.returncode == 0 and sorted(output.splitlines()) == sorted(expected), output + errors)
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()


def udp_setting_sender(executable, dictionary):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as receiver:
        receiver.bind(('127.0.0.1', 0))
        receiver.settimeout(4)
        process = subprocess.run([executable, dictionary, str(receiver.getsockname()[1])],
                                 capture_output=True, text=True, timeout=4)
        require(process.returncode == 0, process.stdout + process.stderr)
        datagram, _ = receiver.recvfrom(1024)
        expected = pprz(bytes([0, 42, 2, 43, 42]) + struct.pack('<f', 12.5))
        require(datagram == expected, 'Grouped setting fields changed the expected PPRZ frame')
        require('(13 bytes)' in process.stdout, process.stdout)


def ivy_receiver(executable, bus, dictionary, encoded_altitude='123.5'):
    domain = f'127.255.255.255:{free_port()}'
    peer = Process([bus, domain], dict(os.environ))
    process = None
    try:
        peer.wait(lambda text: text == 'TEST_BUS_READY')
        process = subprocess.Popen([executable, dictionary, domain], stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, text=True)
        peer.wait(lambda text: text == 'ivy-receiver-example ready')
        peer.send(f'42 CLIENT_ALTITUDE {encoded_altitude}')
        output, errors = process.communicate(timeout=4)
        require(process.returncode == 0 and 'Aircraft 42: 123.5 m' in output, output + errors)
    finally:
        if process and process.poll() is None:
            process.kill()
            process.wait()
        peer.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial', required=True)
    parser.add_argument('--udp', required=True)
    parser.add_argument('--udp-setting')
    parser.add_argument('--ivy')
    parser.add_argument('--bus')
    args = parser.parse_args()
    dictionary = str(Path(__file__).resolve().parents[1] / 'examples/clients/client_messages.xml')
    serial_aircraft(args.serial, dictionary)
    serial_client(args.serial, dictionary)
    serial_peers(args.serial, dictionary)
    print('Serial client/server: altitude, PING/PONG, filters and retry passed', flush=True)
    udp_recorder(args.udp, dictionary)
    print('UDP recorder: concatenated frames and distinct peers passed', flush=True)
    if args.udp_setting:
        udp_setting_sender(args.udp_setting, dictionary)
        print('UDP setting sender: grouped command fields passed', flush=True)
    if args.ivy:
        require(args.bus is not None, 'Ivy testing requires the test peer')
        ivy_receiver(args.ivy, args.bus, dictionary)
        print('Ivy receiver: common binding and SI field read passed', flush=True)

    # Run the same applications with another wire representation. SI call sites
    # still provide/display metres while XML selects int32 millimetres.
    with tempfile.TemporaryDirectory(prefix='pprzlink-client-si-') as directory:
        xml = ET.parse(dictionary)
        field = xml.find(".//message[@name='CLIENT_ALTITUDE']/field[@name='altitude']")
        require(field is not None, 'Altitude definition missing from example XML')
        field.set('type', 'int32')
        field.set('unit', 'mm')
        millimetres = str(Path(directory) / 'messages.xml')
        xml.write(millimetres, encoding='utf-8', xml_declaration=True)
        serial_aircraft(args.serial, millimetres, struct.pack('<i', 123500))
        serial_client(args.serial, millimetres, struct.pack('<i', 123500))
        if args.ivy:
            ivy_receiver(args.ivy, args.bus, millimetres, '123500')
        print('SI clients: unchanged metre inputs/output with XML int32 millimetres passed', flush=True)


if __name__ == '__main__':
    main()
