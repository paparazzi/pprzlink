#!/usr/bin/env python3
"""Check existing XML link routing and paired RTS/CTS through Ivy and a simulated XBee."""
import argparse
import os
from pathlib import Path
import tempfile
import termios
import xml.etree.ElementTree as ET

from LinkParityTest import Harness, SerialWire, api, require, setting, telemetry


class FlowWire(SerialWire):
    def __init__(self, extended):
        super().__init__(xbee=True, extended=extended)
        self.flow_during_at = []

    def read(self, timeout):
        super().read(timeout)
        self.flow_during_at.append(bool(termios.tcgetattr(self.slave)[2] & termios.CRTSCTS))


def scenario(executable, bus, link_mode, flow_control, extended):
    with tempfile.TemporaryDirectory(prefix='link-xbee-config-') as temporary:
        directory = Path(temporary)
        tree = ET.parse(Path(__file__).with_name('link_messages.xml'))
        command = tree.getroot().find("msg_class[@name='datalink']/message[@name='SETTING']")
        command.set('link', link_mode)
        broadcast = link_mode == 'broadcasted'

        def expected_setting(aircraft=42, value=2.5):
            payload = bytearray(setting(aircraft, value))
            if broadcast:
                payload[1] = 255
            return bytes(payload)

        tree.write(directory / 'messages.xml')
        harness = Harness(executable, bus, directory)
        wire = FlowWire(extended)
        try:
            options = ['-transport', 'xbee', '-s', '57600', '-xbee_retries', '2']
            if flow_control:
                options.append('-hfc')
            if extended:
                options.append('-xbee_868')
            with harness.session(options, wire) as (_, peer):
                enabled = '1' if flow_control else '0'
                require(wire.commands == ['+++', 'ATMY0100', 'ATAP1', f'ATD6{enabled}',
                                          f'ATD7{enabled}', 'ATD6', 'ATD7', 'ATCN'],
                        f'Paired XBee flow-control commands differ: {wire.commands}')
                require(not any(wire.flow_during_at), 'Host flow control must stay off during AT setup')
                require(bool(termios.tcgetattr(wire.slave)[2] & termios.CRTSCTS) == flow_control,
                        'The host RTS/CTS setting must match the modem after ATCN')
                if not broadcast:
                    wire.inject(telemetry())
                    peer.wait(lambda text: text == '42 LINK_TEST 7 1.25')
                peer.send('ground SETTING 3 42 2.5')
                frame = wire.frame()
                require(frame is not None, 'No XBee uplink after initialization')
                offset = 13 if extended else 4
                destination = 0xffff if broadcast else 42
                address_end = 10 if extended else 4
                require(frame[2:address_end] == destination.to_bytes(address_end - 2, 'big'),
                        f'XML link routing selected the wrong radio destination: {frame.hex()}')
                require(frame[offset] == 0 and frame[offset + 1:] == expected_setting(),
                        f'Legacy radio options or application ac_id changed: {frame.hex()}')
                require(frame[1] != 0, 'Broadcast must still receive a local transmit status')
                if broadcast:
                    # A broadcast can precede aircraft discovery; reception still works.
                    wire.inject(telemetry())
                    peer.wait(lambda text: text == '42 LINK_TEST 7 1.25')
                failure = (bytes([0x8b, frame[1], 0xff, 0xfe, 0, 0x21, 0]) if extended
                           else bytes([0x89, frame[1], 1]))
                os.write(wire.master, api(failure))
                if not broadcast:
                    require(wire.frame(1) == frame, 'Unicast retry must preserve the frame ID and payload')
                    os.write(wire.master, api(failure))
                require(wire.frame(.4) is None,
                        'Broadcast forbids host resends; unicast must obey -xbee_retries')
                following_aircraft = 43 if broadcast else 42
                peer.send(f'ground SETTING 3 {following_aircraft} 3.5')
                following = wire.frame()
                require(following is not None and following[offset + 1:] == expected_setting(following_aircraft, 3.5),
                        'The agent must preserve ac_id and continue after a final transmit failure')
        finally:
            wire.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--agent', required=True)
    parser.add_argument('--bus', required=True)
    args = parser.parse_args()
    for link_mode, flow_control, extended in [
            ('forwarded', False, False), ('forwarded', True, False), ('broadcasted', True, False),
            ('broadcasted', False, True), ('forwarded', True, True)]:
        print(f'link={link_mode}, RTS/CTS={flow_control}, 868={extended}', flush=True)
        scenario(args.agent, args.bus, link_mode, flow_control, extended)
    print('PASS: XML link routing, ac_id preservation, unicast retries, broadcasts and paired flow control', flush=True)


if __name__ == '__main__':
    main()
