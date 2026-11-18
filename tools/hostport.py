# SPDX-FileCopyrightText: 2026 DM8SB
# SPDX-License-Identifier: GPL-3.0-or-later
"""One parser for "host[:port]" in the host tools (AF52, CQ-19).

    split("cyd-hf-1a2b.local", 80)   -> ("cyd-hf-1a2b.local", 80)
    split("192.168.1.50:5025", 80)   -> ("192.168.1.50", 5025)
    split("[fe80::1]:8080", 80)      -> ("fe80::1", 8080)

An IPv6 address needs brackets when a port follows; a bare one ("fe80::1") is taken whole.
ValueError for an empty host or a port that is not 1..65535.

    python tools/hostport.py         # runs the self-test
"""


def split(text, default_port):
    """(host, port) of "host", "host:port", "[v6]" or "[v6]:port"."""
    text = (text or "").strip()
    if text.startswith("["):
        end = text.find("]")
        if end < 0:
            raise ValueError("missing ] in %r" % text)
        host, rest = text[1:end], text[end + 1:]
        if rest and not rest.startswith(":"):
            raise ValueError("unexpected %r after ] in %r" % (rest, text))
        port = rest[1:] if rest else None
    elif text.count(":") == 1:
        host, port = text.split(":")
    else:
        host, port = text, None  # no port, or a bare IPv6 address
    if not host:
        raise ValueError("no host in %r" % text)
    if port is None:
        return host, default_port
    if not port.isdigit() or not 1 <= int(port) <= 65535:
        raise ValueError("bad port %r in %r" % (port, text))
    return host, int(port)


def host_only(header):
    """Host name of an HTTP Host header ("name:80" -> "name", "[::1]:80" -> "::1"), lower case;
    "" if it cannot be parsed."""
    try:
        return split(header, 0)[0].lower()
    except ValueError:
        return ""


def _self_test():
    ok = [("a", 80, ("a", 80)), ("a:1", 80, ("a", 1)), ("1.2.3.4:65535", 0, ("1.2.3.4", 65535)),
          ("[::1]", 80, ("::1", 80)), ("[::1]:8080", 80, ("::1", 8080)), ("fe80::1", 5025, ("fe80::1", 5025)),
          (" host.local ", 5025, ("host.local", 5025))]
    for text, default, want in ok:
        got = split(text, default)
        assert got == want, (text, got, want)
    for text in ("", ":80", "a:", "a:0", "a:65536", "a:x", "a:-1", "[::1", "[::1]x", "[]:80"):
        try:
            split(text, 80)
        except ValueError:
            continue
        raise AssertionError("accepted %r" % text)
    assert host_only("Name.Local:80") == "name.local" and host_only("[::1]:80") == "::1" and host_only("a:x") == ""
    print("hostport self-test OK")


if __name__ == "__main__":
    _self_test()
