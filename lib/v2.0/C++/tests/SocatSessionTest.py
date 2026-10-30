#!/usr/bin/env python3
"""Exercise the detached socat lifecycle and binary traffic without a messages XML."""
import concurrent.futures
import os
from pathlib import Path
import re
import select
import shutil
import signal
import subprocess
import sys
import tempfile
import time


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def ports(result):
    values = []
    for label in ['A', 'B']:
        match = re.search(rf'^Port {label}: (/dev/pts/\d+)$', result.stdout, re.M)
        require(match, f'Missing {label} port: {result.stdout!r}')
        values.append(Path(match.group(1)))
    require(values[0] != values[1], 'Both ends refer to the same PTY')
    require(all(port.exists() for port in values), 'Printed PTYs do not exist')
    return values


def exchange(sender, receiver, payload):
    os.write(sender, payload)
    received = bytearray()
    deadline = time.monotonic() + 2
    while time.monotonic() < deadline and len(received) < len(payload):
        if select.select([receiver], [], [], .05)[0]:
            received.extend(os.read(receiver, 4096))
    require(received == payload, f'Binary transfer differs: {received.hex()} != {payload.hex()}')


def main():
    executable = str(Path(sys.argv[1]).resolve())
    socat = shutil.which('socat')
    with tempfile.TemporaryDirectory(prefix='link-socat-test-') as temporary:
        root = Path(temporary)
        runtime = root / 'runtime with spaces'
        runtime.mkdir(mode=0o700)
        environment = dict(os.environ, XDG_RUNTIME_DIR=str(runtime))
        for name in ['PPRZLINK_DIR', 'PAPARAZZI_HOME', 'PAPARAZZI_SRC']:
            environment.pop(name, None)
        directory = runtime / 'linkpp-socat'

        def run(*arguments, env=None, expected=0):
            result = subprocess.run([executable, *arguments], env=env or environment,
                                    capture_output=True, text=True, timeout=8)
            require(result.returncode == expected,
                    f'{arguments}: exit {result.returncode}, stdout={result.stdout!r}, stderr={result.stderr!r}')
            return result

        def pid():
            return int((directory / 'session').read_text().splitlines()[0])

        def stopped(old_ports):
            require(not any(port.exists() for port in old_ports), 'Stopped socat left its PTYs alive')
            require(not (directory / 'session').exists(), 'Stopped socat left session state')
            for name in ['port-a', 'port-b', 'link', 'simulator']:
                require(not os.path.lexists(directory / name), 'Stopped socat left a port symlink')

        try:
            run('-socat', expected=2)
            run('-socat', 'restart', expected=2)
            require('-socat' in run('--help').stdout, 'Management command is missing from help')
            require('No socat session' in run('-socat', 'stop').stdout, 'Stop must work before start')

            empty = root / 'empty-path'
            empty.mkdir()
            missing = run('-socat', 'start', env=dict(environment, PATH=str(empty)), expected=1)
            require('socat is not installed' in missing.stderr and '--help' in missing.stderr,
                    f'Missing executable diagnostic: {missing.stderr}')
            stopped([])

            delayed = root / 'delayed-path'
            delayed.mkdir()
            child_pid = root / 'delayed-child.pid'
            stub = delayed / 'socat'
            stub.write_text(f'#!{sys.executable}\nimport os, time\n'
                            f'with open({str(child_pid)!r}, "w") as state: state.write(str(os.getpid()))\n'
                            'time.sleep(30)\n')
            stub.chmod(0o700)
            failed = run('-socat', 'start', env=dict(environment, PATH=str(delayed)), expected=1)
            require('Could not create' in failed.stderr, f'Startup timeout diagnostic: {failed.stderr}')
            require(not Path(f'/proc/{int(child_pid.read_text())}').exists(), 'Failed startup left its child alive')
            stopped([])

            if not socat:
                print('SKIP: socat is not installed')
                return 77

            first = ports(run('-socat', 'start'))
            original_pid = pid()
            existing = run('-socat', 'start')
            require('already running' in existing.stdout, 'Repeated start spawned a new session')
            require(ports(existing) == first and pid() == original_pid, 'Repeated start changed the session')
            descriptors = [os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK) for port in first]
            try:
                exchange(descriptors[0], descriptors[1], bytes(range(256)))
                exchange(descriptors[1], descriptors[0], b'\x99\x08\x2a\x00\x01\x03\x36\xd5')
            finally:
                for descriptor in descriptors:
                    os.close(descriptor)
            run('-socat', 'stop')
            stopped(first)
            run('-socat', 'stop')

            with concurrent.futures.ThreadPoolExecutor(max_workers=2) as executor:
                starts = list(executor.map(lambda _: run('-socat', 'start'), range(2)))
            first = ports(starts[0])
            require(ports(starts[1]) == first, 'Concurrent starts created separate sessions')
            run('-socat', 'stop')
            stopped(first)

            first = ports(run('-socat', 'start'))
            os.kill(pid(), signal.SIGTERM)
            deadline = time.monotonic() + 2
            while any(port.exists() for port in first) and time.monotonic() < deadline:
                time.sleep(.02)
            restarted = run('-socat', 'start')
            require('already running' not in restarted.stdout, 'Exited socat was mistaken for a running session')
            first = ports(restarted)
            os.kill(pid(), signal.SIGSTOP)
            run('-socat', 'stop')  # Must also stop a process unable to handle SIGTERM.
            stopped(first)

            unrelated = subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(30)'])
            try:
                (directory / 'session').write_text(f'{unrelated.pid}\n1\n/dev/pts/missing\n/dev/pts/missing\n')
                run('-socat', 'stop')
                require(unrelated.poll() is None, 'Stale session state killed an unrelated process')
            finally:
                unrelated.terminate()
                unrelated.wait(timeout=3)

            print('PASS: startup, detached lifetime, repeated/concurrent commands, raw duplex traffic, cleanup and stale PID protection')
        finally:
            run('-socat', 'stop')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
