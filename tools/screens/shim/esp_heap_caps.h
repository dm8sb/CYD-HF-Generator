// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
// Host screen renderer (tools/screens): fixed sample value for the CONFIG info card.
#include <stddef.h>
#include <stdint.h>
#include "host_esp.h"
#define MALLOC_CAP_8BIT 4
size_t heap_caps_get_largest_free_block(uint32_t caps);
