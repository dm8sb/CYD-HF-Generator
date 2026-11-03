#pragma once

// Firmware version and the git state of the build, set by tools/build_info.py.
namespace BuildInfo {
const char* version(); // APP_VERSION
const char* branch();  // "dev"; "unknown" without git
const char* commit();  // "641ebe6+" (+ = uncommitted changes); "unknown" without git
const char* time();    // local build time "2026-10-28 16:42"
}
