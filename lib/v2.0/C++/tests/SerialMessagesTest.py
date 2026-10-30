"""Exercise the example executable through a pseudo-terminal, without a radio."""
import errno
import os
import pty
import select
import subprocess
import sys
import tempfile
import termios
import time
from pathlib import Path


def read_exactly(fd, count):
    data = bytearray()
    deadline = time.monotonic() + 5
    while len(data) < count:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise AssertionError(f"Timed out reading serial TX: {data.hex()}")
        if not select.select([fd], [], [], remaining)[0]:
            continue
        try:
            data.extend(os.read(fd, count - len(data)))
        except OSError as error:
            if error.errno != errno.EIO:  # The child has not opened the slave yet.
                raise
            time.sleep(0.005)
    return bytes(data)


def exchange(executable, messages, transport, expected_tx, incoming, extra=()):
    master, slave = pty.openpty()
    child = None
    try:
        command = [executable, "-messages", str(messages), "-d", os.ttyname(slave),
                   "-transport", transport, "-send", "BYTE 123",
                   "-sender", "42", "-receiver", "7", "-duration", "1", *extra]
        initialize = transport == "xbee" and "-xbee-no-init" not in extra
        if initialize:
            command.extend(["-xbee-guard-ms", "20", "-xbee-timeout-ms", "500"])
        child = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        os.close(slave)
        slave = -1
        if initialize:
            assert read_exactly(master, 3) == b"+++"
            os.write(master, b"O")
            os.write(master, b"K\r\n")
            if "-xbee-no-autobaud" not in extra:
                assert read_exactly(master, 5) == b"ATBD\r"
                os.write(master, b"6\r")
            level = b"1" if "-hfc" in extra else b"0"
            for at in (b"ATMY0100\r", b"ATAP1\r", b"ATD6" + level + b"\r",
                       b"ATD7" + level + b"\r", b"ATD6\r", b"ATD7\r", b"ATCN\r"):
                assert read_exactly(master, len(at)) == at
                assert not termios.tcgetattr(master)[2] & termios.CRTSCTS, "CTS must not block bootstrap commands"
                os.write(master, level + b"\r" if at in (b"ATD6\r", b"ATD7\r") else b"OK\r")
        actual = read_exactly(master, len(expected_tx))
        assert actual == expected_tx, (transport, actual.hex(), expected_tx.hex())
        assert bool(termios.tcgetattr(master)[2] & termios.CRTSCTS) == ("-hfc" in extra)
        for byte in incoming:
            os.write(master, bytes([byte]))
        stdout, stderr = child.communicate(timeout=10)
        assert child.returncode == 0, (stdout, stderr)
        assert not stderr, stderr
        assert "42 BYTE 123" in stdout, stdout
        if transport == "xbee":
            assert "XBee TX status: frame=1 status=0" in stdout, stdout
            assert ("XBee modem ready (AP=1)" in stdout) == initialize, stdout
    finally:
        if child is not None and child.poll() is None:
            child.kill()
            child.communicate()
        if slave >= 0:
            os.close(slave)
        os.close(master)


def initialization_failure(executable, messages, timeout):
    master, slave = pty.openpty()
    child = None
    try:
        child = subprocess.Popen(
            [executable, "-messages", str(messages), "-d", os.ttyname(slave),
             "-transport", "xbee", "-send", "BYTE 123", "-xbee-guard-ms", "20",
             "-xbee-timeout-ms", "100", "-xbee_addr", "0x234", "-ch", "0x15"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        os.close(slave)
        slave = -1
        for command, reply in ((b"+++", b"OK\r"), (b"ATBD\r", b"6\r"),
                               (b"ATMY0234\r", b"OK\r"), (b"ATCH15\r", b"OK\r")):
            assert read_exactly(master, len(command)) == command
            os.write(master, reply)
        assert read_exactly(master, 6) == b"ATAP1\r"
        if not timeout:
            os.write(master, b"ERROR\r")
        stdout, stderr = child.communicate(timeout=5)
        assert child.returncode != 0, (stdout, stderr)
        assert "ATAP1" in stderr and ("timeout" if timeout else "ERROR") in stderr, stderr
        assert "Wrote" not in stdout and "Listening" not in stdout, stdout
        # With the child closed, any remaining bytes would be an erroneous send/ATCN.
        try:
            remainder = os.read(master, 1024)
        except OSError as error:
            assert error.errno == errno.EIO
            remainder = b""
        assert not remainder, remainder
    finally:
        if child is not None and child.poll() is None:
            child.kill()
            child.communicate()
        if slave >= 0:
            os.close(slave)
        os.close(master)


def baud_transition(executable, messages, target=57600, failure=None):
    """Emulate BD/WR while checking the actual baud options applied to the PTY."""
    stored_baud = 9600
    code = {57600: b"6", 115200: b"7"}[target]
    speeds = {9600: termios.B9600, 57600: termios.B57600, 115200: termios.B115200}
    expected_frame = bytes.fromhex("7e 00 0a 01 01 00 07 00 2a 07 01 01 7b 48")
    # Second process sees the state retained by the simulated modem after ATWR.
    for startup in range(1 if failure else 2):
        master, slave = pty.openpty()
        child = None
        try:
            arguments = [executable, "-messages", str(messages), "-d", os.ttyname(slave),
                         "-transport", "xbee", "-send", "BYTE 123", "-sender", "42",
                         "-receiver", "7", "-duration", "1", "-xbeesan",
                         "-xbee-guard-ms", "20", "-xbee-timeout-ms", "100"]
            if target != 57600:
                arguments.extend(["-s", str(target)])
            child = subprocess.Popen(arguments, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            os.close(slave)
            slave = -1

            def expect(command, baud, reply):
                assert read_exactly(master, len(command)) == command, command
                attributes = termios.tcgetattr(master)
                assert attributes[4:6] == [speeds[baud], speeds[baud]], (command, baud, attributes)
                if reply:
                    os.write(master, reply)

            expect(b"+++", target, b"OK\r" if stored_baud == target else None)
            if stored_baud != target:
                expect(b"+++", 9600, b"OK\r")  # Default fallback, without a command-line opt-in.
                expect(b"ATBD\r", 9600, b"3\r")
                expect(b"ATBD" + code + b"\r", 9600, b"OK\r")
                expect(b"ATCN\r", 9600, b"OK\r")
                expect(b"+++", target, b"OK\r")
                expect(b"ATBD\r", target, code + b"\r")
                reply = None if failure == "timeout" else b"ERROR\r" if failure else b"OK\r"
                expect(b"ATWR\r", target, reply)
                if failure:
                    stdout, stderr = child.communicate(timeout=5)
                    assert child.returncode != 0 and "ATWR" in stderr and failure in stderr, (stdout, stderr)
                    assert "Wrote" not in stdout and "Listening" not in stdout, stdout
                    try:
                        remainder = os.read(master, 1024)
                    except OSError as error:
                        assert error.errno == errno.EIO
                        remainder = b""
                    assert not remainder, remainder
                    continue
                stored_baud = target
            else:
                expect(b"ATBD\r", target, code + b"\r")
            for command in (b"ATMY0100\r", b"ATAP1\r", b"ATD60\r", b"ATD70\r",
                            b"ATD6\r", b"ATD7\r", b"ATCN\r"):
                expect(command, target, b"0\r" if command in (b"ATD6\r", b"ATD7\r") else b"OK\r")
            assert read_exactly(master, len(expected_frame)) == expected_frame
            stdout, stderr = child.communicate(timeout=5)
            assert child.returncode == 0 and not stderr, (stdout, stderr)
            assert f"configured {target}" in stdout, stdout
            assert ("saved to flash" if startup == 0 else "unchanged") in stdout, stdout
        finally:
            if child is not None and child.poll() is None:
                child.kill()
                child.communicate()
            if slave >= 0:
                os.close(slave)
            os.close(master)


def blocked_send(executable, messages):
    master, slave = pty.openpty()
    try:
        # Structurally valid XML values, but 4 header + 1 count + 96 data > 100 RF bytes.
        body = "BLOB " + ",".join(["1"] * 96)
        result = subprocess.run(
            [executable, "-messages", str(messages), "-d", os.ttyname(slave),
             "-transport", "xbee", "-xbee-no-init", "-xbeesan", "-send", body],
            capture_output=True, text=True, timeout=5)
        assert result.returncode != 0 and "payload limit" in result.stderr, result
        assert "Wrote" not in result.stdout and "Listening" not in result.stdout, result
        # Keep the slave open so poll is not confused by hangup; no serial bytes may arrive.
        assert not select.select([master], [], [], 0.05)[0], "Rejected frame reached serial port"
    finally:
        os.close(slave)
        os.close(master)


def main():
    executable = sys.argv[1]
    with tempfile.TemporaryDirectory(prefix="pprzlink-xbee-") as directory:
        messages = Path(directory) / "messages.xml"
        messages.write_text('''<protocol><msg_class name="test" id="1">
          <message name="BYTE" id="1"><field name="value" type="uint8"/></message>
          <message name="BLOB" id="2"><field name="data" type="uint8[]"/></message>
        </msg_class></protocol>''')
        # Same dictionary, sender/receiver and field values, different serial envelopes.
        pprz = bytes.fromhex("99 09 2a 07 01 01 7b b7 a4")
        exchange(executable, messages, "pprz", pprz, pprz)
        status_and_rx = bytes.fromhex("7e 00 03 89 01 00 75  7e 00 0a 81 12 34 40 00 2a 07 01 01 7b 4a")
        exchange(executable, messages, "xbee",
                 bytes.fromhex("7e 00 0a 01 01 00 07 00 2a 07 01 01 7b 48"), status_and_rx)
        exchange(executable, messages, "xbee",
                 bytes.fromhex("7e 00 10 00 01 00 13 a2 00 40 52 91 ab 00 2a 07 01 01 7b cd"),
                 status_and_rx, ("-xbee-dest64", "0x0013a200405291ab", "-xbeesan"))
        exchange(executable, messages, "xbee",
                 bytes.fromhex("7e 00 0a 01 01 00 07 00 2a 07 01 01 7b 48"),
                 status_and_rx, ("-xbee-no-init", "-xbeesan"))
        exchange(executable, messages, "xbee",
                 bytes.fromhex("7e 00 0a 01 01 00 07 00 2a 07 01 01 7b 48"),
                 status_and_rx, ("-xbee-no-autobaud",))
        exchange(executable, messages, "xbee",
                 bytes.fromhex("7e 00 0a 01 01 00 07 00 2a 07 01 01 7b 48"),
                 status_and_rx, ("-hfc",))
        baud_transition(executable, messages)
        baud_transition(executable, messages, target=115200)
        baud_transition(executable, messages, failure="ERROR")
        baud_transition(executable, messages, failure="timeout")
        initialization_failure(executable, messages, timeout=False)
        initialization_failure(executable, messages, timeout=True)
        blocked_send(executable, messages)

        result = subprocess.run([executable, "--help"], capture_output=True, text=True, timeout=5)
        assert result.returncode == 0 and "-transport pprz|xbee" in result.stdout, result
        for arguments in (("-transport", "invalid"), ("-sender", "256"),
                          ("-transport", "pprz", "-xbee-dest", "42"),
                          ("-transport", "pprz", "-xbeesan"),
                          ("-transport", "pprz", "-xbee-no-autobaud"),
                          ("-transport", "xbee", "-s", "230400"),
                          ("-transport", "xbee", "-xbee-dest64", "0x10000000000000000"),
                          ("-transport", "xbee", "-ch", "0x18"),
                          ("-transport", "xbee", "-xbee-guard-ms", "0"),
                          ("-transport", "xbee", "-xbee-no-init", "-xbee_addr", "42"),
                          ("-transport", "xbee", "-xbee-no-init", "-hfc"),
                          ("-transport", "xbee", "-xbee-dest", "1", "-xbee-dest64", "2")):
            result = subprocess.run([executable, "-messages", str(messages), "-d", "/no-such-port",
                                     *arguments], capture_output=True, text=True, timeout=5)
            assert result.returncode != 0 and "No such file" not in result.stderr, result
    print("CLI selection, serial TX/RX, default autobaud, persistence and failure handling passed")


if __name__ == "__main__":
    main()
