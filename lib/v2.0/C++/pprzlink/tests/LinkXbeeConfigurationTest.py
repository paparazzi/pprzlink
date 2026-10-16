#!/usr/bin/env python3
"""Check link++ XML retries and paired RTS/CTS through Ivy and a simulated XBee."""
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


def scenario(executable, bus, retry_attribute, flow_control, extended):
    with tempfile.TemporaryDirectory(prefix='link-xbee-config-') as temporary:
        directory = Path(temporary)
        tree = ET.parse(Path(__file__).with_name('link_messages.xml'))
        command = tree.getroot().find("msg_class[@name='datalink']/message[@name='SETTING']")
        if retry_attribute is not None:
            command.set('radio_retry', retry_attribute)
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
                wire.inject(telemetry())
                peer.wait(lambda text: text == '42 LINK_TEST 7 1.25')
                peer.send('ground SETTING 3 42 2.5')
                frame = wire.frame()
                require(frame is not None, 'No XBee uplink after initialization')
                retry_enabled = retry_attribute != 'false'
                offset = 13 if extended else 4
                require(frame[offset] == (0 if retry_enabled else 1) and frame[offset + 1:] == setting(),
                        f'XML retry metadata or PPRZ payload lost through Ivy: {frame.hex()}')
                failure = (bytes([0x8b, frame[1], 0xff, 0xfe, 0, 0x21, 0]) if extended
                           else bytes([0x89, frame[1], 1]))
                os.write(wire.master, api(failure))
                if retry_enabled:
                    require(wire.frame(1) == frame, 'Enabled/legacy retry must preserve the frame ID and payload')
                    os.write(wire.master, api(failure))
                require(wire.frame(.4) is None,
                        'XML false forbids host resends; enabled retries must obey -xbee_retries')
                peer.send('ground SETTING 3 42 3.5')
                following = wire.frame()
                require(following is not None and following[offset + 1:] == setting(value=3.5),
                        'The agent must continue handling commands after a final transmit failure')
        finally:
            wire.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--agent', required=True)
    parser.add_argument('--bus', required=True)
    args = parser.parse_args()
    for retry_attribute, flow_control, extended in [
            (None, False, False), ('true', True, False), ('false', True, False),
            ('false', False, True), ('true', True, True)]:
        print(f'XML={retry_attribute}, RTS/CTS={flow_control}, 868={extended}', flush=True)
        scenario(args.agent, args.bus, retry_attribute, flow_control, extended)
    print('PASS: XML radio and host retries, CLI limit, paired flow control and continued reception', flush=True)


if __name__ == '__main__':
    main()
