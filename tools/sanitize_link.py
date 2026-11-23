# SPDX-FileCopyrightText: 2026 DM8SB
# SPDX-License-Identifier: GPL-3.0-or-later

"""PlatformIO extra script of the env native-asan (AF36, TEST-6): build_flags reach only the
compiler, so the sanitizer runtime must be linked with the same -fsanitize flags."""

Import("env")  # noqa: F821  (provided by SCons)

env.Append(  # noqa: F821
    LINKFLAGS=[
        "-fsanitize=address,undefined",
        "-fno-sanitize-recover=all",
        "-fno-omit-frame-pointer",
    ]
)
