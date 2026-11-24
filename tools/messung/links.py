# SPDX-FileCopyrightText: 2026 DM8SB
# SPDX-License-Identifier: GPL-3.0-or-later

"""Transports for the automated measurements: one small interface for every instrument.

A link knows write(cmd), query(cmd) -> str and query_block(cmd) -> bytes (IEEE 488.2
definite-length block). Specs as given on the command line:

    visa:TCPIP::192.168.1.20::INSTR     PyVISA with pyvisa-py (VXI-11), the Siglent instruments
    tcp:192.168.1.50[:5025]             raw SCPI socket (the generator)
    usb:COM13                           serial port, 115200 Bd (the generator; pyserial)
    sim:<name>                          simulated instrument (sim.py), for dry runs and tests
"""
import os
import socket
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import hostport  # noqa: E402  tools/hostport.py


class LinkError(IOError):
    pass


def parse_block(data):
    """Payload of an IEEE 488.2 definite-length block somewhere in data (#<n><len><payload>)."""
    i = data.find(b"#")
    if i < 0 or i + 2 > len(data):
        raise LinkError("no block header")
    nd = int(data[i + 1:i + 2])
    if nd == 0:
        raise LinkError("indefinite blocks are not supported")
    length = int(data[i + 2:i + 2 + nd])
    start = i + 2 + nd
    payload = data[start:start + length]
    if len(payload) != length:
        raise LinkError("block truncated ({} of {} bytes)".format(len(payload), length))
    return payload


class Link:
    name = "link"

    def write(self, cmd):
        raise NotImplementedError

    def query(self, cmd):
        raise NotImplementedError

    def query_block(self, cmd):
        raise NotImplementedError

    def close(self):
        pass

    def pulse_reset(self):
        raise LinkError("{}: reset over the link is only possible over USB".format(self.name))


class VisaLink(Link):
    def __init__(self, resource, timeout_s=10.0):
        import pyvisa  # late import: dry runs need no PyVISA

        self.name = resource
        self.rm = pyvisa.ResourceManager("@py")
        self.inst = self.rm.open_resource(resource)
        self.inst.timeout = int(timeout_s * 1000)
        self.inst.read_termination = "\n"
        self.inst.write_termination = "\n"
        self.inst.chunk_size = 1024 * 1024

    def write(self, cmd):
        self.inst.write(cmd)

    def query(self, cmd):
        return self.inst.query(cmd).strip()

    def query_block(self, cmd):
        self.inst.write(cmd)
        return parse_block(self.inst.read_raw())

    def close(self):
        self.inst.close()


class _LineLink(Link):
    """Common part of the serial and TCP links of the generator (lines end with LF)."""

    def _send(self, data):
        raise NotImplementedError

    def _recv(self):
        raise NotImplementedError

    def write(self, cmd):
        self._send((cmd + "\n").encode("ascii"))

    def readline(self, timeout_s=5.0):
        end = time.monotonic() + timeout_s
        while b"\n" not in self.buf:
            if time.monotonic() > end:
                raise LinkError("{}: no answer".format(self.name))
            self.buf += self._recv()
        line, self.buf = self.buf.split(b"\n", 1)
        return line.decode("ascii", "replace").strip()

    def query(self, cmd):
        self.write(cmd)
        return self.readline()

    def query_block(self, cmd):
        raise LinkError("{}: no block transfer".format(self.name))


class SerialLink(_LineLink):
    def __init__(self, port):
        import serial  # pyserial

        ser = serial.Serial()
        ser.port = port
        ser.baudrate = 115200
        ser.timeout = 0.2
        # DTR and RTS off before open(): otherwise the auto-reset circuit restarts the board.
        ser.dtr = False
        ser.rts = False
        ser.open()
        self.ser = ser
        self.name = port
        self.buf = b""

    def _send(self, data):
        self.ser.write(data)

    def _recv(self):
        return self.ser.read(256)

    def pulse_reset(self, low_s=0.1):
        """EN low for low_s over RTS (auto-reset circuit of the CYD), DTR stays off (no boot mode)."""
        self.ser.dtr = False
        self.ser.rts = True
        time.sleep(low_s)
        self.ser.rts = False
        self.buf = b""

    def close(self):
        self.ser.close()


class TcpLink(_LineLink):
    def __init__(self, target, timeout_s=5.0):
        host, port = hostport.split(target, 5025)
        self.sock = socket.create_connection((host, port), timeout=timeout_s)
        self.sock.settimeout(0.2)
        self.name = target
        self.buf = b""

    def _send(self, data):
        self.sock.sendall(data)

    def _recv(self):
        try:
            data = self.sock.recv(4096)
        except socket.timeout:
            return b""
        if not data:
            raise LinkError("{}: connection closed".format(self.name))
        return data

    def close(self):
        self.sock.close()


def open_link(spec, sim_bench=None):
    kind, _, rest = spec.partition(":")
    if kind == "visa":
        return VisaLink(rest)
    if kind == "tcp":
        return TcpLink(rest)
    if kind == "usb":
        return SerialLink(rest)
    if kind == "sim":
        if sim_bench is None:
            raise LinkError("sim: link without a simulated bench")
        return sim_bench.link(rest)
    raise LinkError("unknown link '{}' (visa:, tcp:, usb:, sim:)".format(spec))
