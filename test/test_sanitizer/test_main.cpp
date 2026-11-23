// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

// Probe of the env native-asan (AF36, TEST-6). Normally a trivial pass in every native env. CI
// builds it once more with -DSANITIZER_PROBE=1 in native-asan: the deliberate heap overflow and
// the signed overflow below must then be reported and fail the run, which shows that the
// sanitizers are really linked in and abort on the first report.

#include <unity.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

void setUp() {}
void tearDown() {}

#ifndef SANITIZER_PROBE
#define SANITIZER_PROBE 0
#endif

static volatile int g_index = 8;

static void test_sanitizer_probe() {
    char* p = static_cast<char*>(malloc(8));
    TEST_ASSERT_NOT_NULL(p);
    memset(p, 0, 8);
    if (SANITIZER_PROBE) {
        p[g_index] = 1; // one byte past the block: AddressSanitizer heap-buffer-overflow
        volatile int big = INT_MAX;
        TEST_ASSERT_TRUE(big + g_index > 0); // signed overflow: UBSan
    }
    free(p);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_sanitizer_probe);
    return UNITY_END();
}
