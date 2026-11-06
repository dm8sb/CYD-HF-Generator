#!/usr/bin/env python3
"""Minimal SCPI example for the CYD HF generator.

Connects over USB (pyserial) or TCP (plain socket, or PyVISA if you pass
--visa), then:

  1. asks *IDN?
  2. sets CH1 to 7.1 MHz and switches it on
  3. reads the actual frequency (FREQ:ACT?)
  4. reads the error queue until it is empty
  5. switches CH1 off again

WARNING: the generator outputs RF. Step 2 really switches CH1 on (briefly).
Do not connect an antenna without a filter.

Usage:
    python scpi_example.py --usb COM5
    python scpi_example.py --tcp cyd-hf-1a2b.local          # port 5025
    python scpi_example.py --tcp 192.168.1.50:5025
    python scpi_example.py --visa "TCPIP0::cyd-hf-1a2b.local::5025::SOCKET"

Requirements: Python 3.8+, "pip install pyserial" for --usb,
"pip install pyvisa pyvisa-py" for --visa (both optional otherwise).

The TCP port is not in the firmware yet; USB is available now.
"""
import argparse
import socket
import sys


class UsbLink:
    """SCPI over the USB serial port (115200 baud, lines end with LF)."""

    def __init__(self, port):
        import serial  # imported late so --tcp works without pyserial

        ser = serial.Serial()
        ser.port = port
        ser.baudrate = 115200
        ser.timeout = 2
        # IMPORTANT: DTR and RTS must be off *before* open(). Otherwise the
        # board's auto-reset circuit restarts the generator on open.
        ser.dtr = False
        ser.rts = False
        ser.open()
        self.ser = ser

    def write(self, cmd):
        self.ser.write((cmd + "\n").encode("ascii"))

    def readline(self):
        return self.ser.readline().decode("ascii", "replace").strip()

    def close(self):
        self.ser.close()


class TcpLink:
    """SCPI over a plain TCP socket (default port 5025, lines end with LF)."""

    def __init__(self, target):
        host, _, port = target.partition(":")
        self.sock = socket.create_connection((host, int(port or 5025)), timeout=2)
        self.buf = b""

    def write(self, cmd):
        self.sock.sendall((cmd + "\n").encode("ascii"))

    def readline(self):
        while b"\n" not in self.buf:
            data = self.sock.recv(1024)  # raises socket.timeout after 2 s
            if not data:
                raise ConnectionError("connection closed by the device")
            self.buf += data
        line, _, self.buf = self.buf.partition(b"\n")
        return line.decode("ascii", "replace").strip()

    def close(self):
        self.sock.close()


class VisaLink:
    """SCPI through PyVISA, e.g. TCPIP0::host::5025::SOCKET or ASRL5::INSTR."""

    def __init__(self, resource):
        import pyvisa

        self.inst = pyvisa.ResourceManager().open_resource(
            resource, read_termination="\n", write_termination="\n")
        self.inst.timeout = 2000

    def write(self, cmd):
        self.inst.write(cmd)

    def readline(self):
        return self.inst.read().strip()

    def close(self):
        self.inst.close()


def query(link, cmd):
    """Send a query and return the one-line answer."""
    link.write(cmd)
    return link.readline()


def main():
    ap = argparse.ArgumentParser(description="Minimal SCPI example for the CYD HF generator.")
    grp = ap.add_mutually_exclusive_group(required=True)
    grp.add_argument("--usb", metavar="PORT", help="serial port, e.g. COM5 or /dev/ttyUSB0")
    grp.add_argument("--tcp", metavar="HOST[:PORT]", help="TCP address, port defaults to 5025")
    grp.add_argument("--visa", metavar="RESOURCE", help="PyVISA resource string (needs pyvisa)")
    args = ap.parse_args()

    if args.usb:
        link = UsbLink(args.usb)
    elif args.tcp:
        link = TcpLink(args.tcp)
    else:
        link = VisaLink(args.visa)

    try:
        print("IDN:", query(link, "*IDN?"))  # CYD,HF-Generator,<MAC>,<version>

        link.write("FREQ 7.1 MHZ")           # CH1 is the default channel
        link.write("OUTP1 ON")
        print("Actual frequency [Hz]:", query(link, "FREQ:ACT?"))

        # Setting commands answer nothing; errors wait in a queue (16 entries).
        # Read until the device says 0,"No error".
        for _ in range(20):
            err = query(link, "SYST:ERR?")
            print("Error queue:", err)
            if err.startswith("0,"):
                break
    finally:
        link.write("OUTP1 OFF")              # always leave the output off
        link.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
