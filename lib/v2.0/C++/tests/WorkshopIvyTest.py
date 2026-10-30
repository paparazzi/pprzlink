#!/usr/bin/env python3
"""Run one Ivy workshop executable in emitter and receiver roles on a local bus."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
import xml.etree.ElementTree as ET
from LinkParityTest import Process, free_port, require


def exchange(executable, dictionary, emitter_first):
    domain = f'127.255.255.255:{free_port()}'
    processes = {}
    try:
        order = [('r', 'Récepteur'), ('e', 'Émetteur')]
        if emitter_first:
            order.reverse()
        for role, name in order:
            peer = Process([executable, dictionary, role, domain], dict(os.environ))
            processes[role] = peer
            peer.wait(lambda line, name=name: line.startswith(name + ' prêt sur Ivy ('))
        processes['r'].wait(lambda line: line == 'Altitude de 42 : 123.5 m')
        processes['e'].wait(lambda line: line == 'Altitude publiée sur Ivy.')
        for peer in processes.values():
            require(peer.process.wait(timeout=3) == 0, f'Ivy role failed: {peer.errors}')
    finally:
        for peer in processes.values():
            peer.close()
    print('PASS: ' + ('emitter' if emitter_first else 'receiver') + ' started first', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable')
    args = parser.parse_args()
    dictionary = str(Path(__file__).resolve().parents[1] / 'examples/clients/guide_messages.xml')
    for values in [[], [dictionary], [dictionary, 'invalid'], [dictionary, 'r', 'domain', 'extra']]:
        result = subprocess.run([args.executable, *values], capture_output=True, text=True, timeout=3)
        require(result.returncode == 2 and 'messages.xml e|r' in result.stderr,
                result.stdout + result.stderr)
    for emitter_first in [False, True]:
        exchange(args.executable, dictionary, emitter_first)

    # Both roles use SI metres while the XML selects int32 millimetres on the bus.
    with tempfile.TemporaryDirectory(prefix='workshop-ivy-si-') as temporary:
        xml = ET.parse(dictionary)
        field = xml.find(".//message[@name='GUIDE_ALTITUDE']/field[@name='altitude']")
        require(field is not None, 'Workshop altitude definition is missing')
        field.set('type', 'int32')
        field.set('unit', 'mm')
        millimetres = str(Path(temporary) / 'messages.xml')
        xml.write(millimetres, encoding='utf-8', xml_declaration=True)
        for emitter_first in [False, True]:
            exchange(args.executable, millimetres, emitter_first)

    # Missing peers must release the subscription/timer or the owned loop thread.
    for role, timeout, diagnostic in [('e', 8, 'Abonnement du récepteur introuvable'),
                                      ('r', 13, 'Aucune altitude reçue')]:
        domain = f'127.255.255.255:{free_port()}'
        result = subprocess.run([args.executable, dictionary, role, domain],
                                capture_output=True, text=True, timeout=timeout)
        require(result.returncode == 1 and diagnostic in result.stderr,
                result.stdout + result.stderr)
    print('PASS: invalid roles, XML SI conversion and bounded waits', flush=True)


if __name__ == '__main__':
    main()
