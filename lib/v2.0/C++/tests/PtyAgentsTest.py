#!/usr/bin/env python3
"""Run identical SDK agents with either port assignment and either launch order."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
from LinkParityTest import Process, require
from PtyPeersTest import connected_ports


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--agent', required=True)
    parser.add_argument('--controller')
    args = parser.parse_args()
    dictionary = str(Path(__file__).resolve().parents[1] / 'examples/clients/guide_messages.xml')
    for values in [['--help'], [dictionary, '/dev/null', '-1'],
                   [dictionary, '/dev/null', '256'], [dictionary, '/dev/null', 'invalid']]:
        result = subprocess.run([args.agent, *values], capture_output=True, text=True, timeout=3)
        require(result.returncode == (0 if values == ['--help'] else 2), result.stdout + result.stderr)
    with tempfile.TemporaryDirectory(prefix='pty-agents-test-') as temporary:
        environment = dict(os.environ, XDG_RUNTIME_DIR=temporary)
        for swapped in [False, True]:
            for first_id in [1, 2]:
                with connected_ports(args.controller, environment) as pair:
                    if swapped:
                        pair.reverse()
                    processes = {}
                    try:
                        for id in [first_id, 3 - first_id]:
                            process = Process([args.agent, dictionary, pair[id - 1], str(id)], environment)
                            processes[id] = process
                            process.wait(lambda line, id=id: line.startswith(f'Agent {id} ready on '))
                        for _ in range(2):
                            for id, process in processes.items():
                                peer = 3 - id
                                start = len(process.lines) if _ else 0
                                process.wait(lambda line, id=id, peer=peer:
                                             line == f'Agent {id} RX from {peer}: GUIDE_ALTITUDE {123.5 + peer} m',
                                             start=start)
                                require(process.process.poll() is None, f'Agent {id} stopped instead of continuing')
                        for process in processes.values():
                            process.close()
                            require(process.process.returncode == 0, f'Agent did not stop cleanly: {process.errors}')
                        print(f'PASS: agents use swapped={swapped} ports, agent {first_id} starts first', flush=True)
                    finally:
                        for process in processes.values():
                            if process.process.poll() is None:
                                process.close()


if __name__ == '__main__':
    main()
