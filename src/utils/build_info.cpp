// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include "build_info.h"
#include <config.h>

// tools/build_info.py defines these for this file only, so a new build time recompiles
// nothing else. __DATE__/__TIME__ would go stale: the file itself never changes.
#ifndef BUILD_INFO_BRANCH
#define BUILD_INFO_BRANCH "unknown"
#endif
#ifndef BUILD_INFO_COMMIT
#define BUILD_INFO_COMMIT "unknown"
#endif
#ifndef BUILD_INFO_TIME
#define BUILD_INFO_TIME "unknown"
#endif
// Only for a build of a release tag v<APP_VERSION>[-suffix], e.g. "2.0.0-rc1" (AF25).
#ifndef BUILD_INFO_VERSION
#define BUILD_INFO_VERSION APP_VERSION
#endif

namespace BuildInfo {
const char* version() { return BUILD_INFO_VERSION; }
const char* branch() { return BUILD_INFO_BRANCH; }
const char* commit() { return BUILD_INFO_COMMIT; }
const char* time() { return BUILD_INFO_TIME; }
}
