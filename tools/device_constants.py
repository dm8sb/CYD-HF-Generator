# SPDX-FileCopyrightText: 2026 DM8SB
# SPDX-License-Identifier: GPL-3.0-or-later
"""Firmware constants for the host tools, read from include/constants.h (AF48, CQ-7).

web_mock.py and fake_scpi_device.py take frequency limits, step list and correction range from
here instead of keeping their own copies, so a change in the firmware reaches the mocks. Only
plain integer constants and integer arrays are read; a missing name raises KeyError at import.

    python tools/device_constants.py     # prints the values
"""
import ast
import operator
import os
import re

HEADER = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "include", "constants.h")

_OPS = {ast.Add: operator.add, ast.Sub: operator.sub, ast.Mult: operator.mul, ast.FloorDiv: operator.floordiv}


def _int_expr(text, names):
    """Evaluates an integer constant expression of the header (literals, + - * /, known names)."""
    text = re.sub(r"\b(\d+)[uUlL]+\b", r"\1", text.strip()).replace("/", "//")

    def ev(node):
        if isinstance(node, ast.Expression):
            return ev(node.body)
        if isinstance(node, ast.Constant) and isinstance(node.value, int):
            return node.value
        if isinstance(node, ast.Name) and node.id in names:
            return names[node.id]
        if isinstance(node, ast.BinOp) and type(node.op) in _OPS:
            return _OPS[type(node.op)](ev(node.left), ev(node.right))
        if isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.USub):
            return -ev(node.operand)
        if isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and len(node.args) == 1:
            return ev(node.args[0])  # casts like uint8_t(x)
        raise ValueError(text)

    return ev(ast.parse(text, mode="eval"))


def parse(path=HEADER):
    """All integer constants and integer arrays of the header: {name: int or tuple}."""
    with open(path, encoding="utf-8") as f:
        src = re.sub(r"//[^\n]*", "", f.read())
    values = {}
    for m in re.finditer(r"constexpr\s+[\w:]+\s+(\w+)(\[\])?\s*=\s*([^;]+);", src):
        name, array, expr = m.group(1), m.group(2), m.group(3)
        try:
            if array:
                items = expr.strip().strip("{}").split(",")
                values[name] = tuple(_int_expr(x, values) for x in items if x.strip())
            else:
                values[name] = _int_expr(expr, values)
        except (ValueError, SyntaxError):
            pass  # not an integer constant (struct, float, ...)
    return values


_C = parse()
FREQ_MIN = _C["FREQ_MIN_HZ"]
FREQ_MAX = _C["FREQ_MAX_HZ"]
IQ_MIN = _C["IQ_MIN_HZ"]
IQ_MAX = _C["IQ_MAX_HZ"]
STEPS = _C["DELTA_STEPS"]
DEFAULT_STEP = _C["DEFAULT_DELTA_STEP"]
DEFAULT_FREQ = {1: _C["DEFAULT_FREQ_CH1"], 2: _C["DEFAULT_FREQ_CH2"]}
CORRECTION_MAX = _C["CORRECTION_MAX"]
DRIVES = tuple(range(_C["DRIVE_MA_MIN"], _C["DRIVE_MA_MAX"] + 1, _C["DRIVE_MA_STEP"]))
DEFAULT_DRIVE = _C["DEFAULT_DRIVE_MA"]

if __name__ == "__main__":
    for k in ("FREQ_MIN", "FREQ_MAX", "IQ_MIN", "IQ_MAX", "STEPS", "DEFAULT_STEP", "DEFAULT_FREQ",
              "CORRECTION_MAX", "DRIVES", "DEFAULT_DRIVE"):
        print(k, globals()[k])
