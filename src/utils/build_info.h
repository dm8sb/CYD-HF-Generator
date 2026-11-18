// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Firmware version and the git state of the build, set by tools/build_info.py.
namespace BuildInfo {
const char* version(); // APP_VERSION; the tag without "v" for a tag build ("2.0.0-rc1")
const char* branch();  // "dev"; the tag at a detached HEAD; "unknown" without git
const char* commit();  // "641ebe6+" (+ = uncommitted changes); "unknown" without git
const char* time();    // commit time in UTC "2026-10-28 15:42 UTC" (build time if "+")
}
