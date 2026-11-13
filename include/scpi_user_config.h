// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// scpi-parser configuration (-DSCPI_USER_CONFIG): the same on the ESP32 and in the host tests,
// whose compilers the library would otherwise treat differently (bare metal / full-blown OS).

// Answers end in LF only, so PyVISA with read_termination="\n" gets no trailing CR.
#define SCPI_LINE_ENDING "\n"
// All standard error texts (-114, -221, -222, -224, -240, -363 ...).
#define USE_FULL_ERROR_LIST 1
// "<code>,"<text>;<info>"" (e.g. the undefined header); the info is copied into a fixed
// buffer per session (SCPI_InitHeap). USE_MEMORY_ALLOCATION_FREE 1 would mean malloc/free.
#define USE_DEVICE_DEPENDENT_ERROR_INFORMATION 1
#define USE_MEMORY_ALLOCATION_FREE 0
#define USE_COMMAND_TAGS 1
#define USE_DEPRECATED_FUNCTIONS 0
// Numbers and units are parsed by remote/scpi.cpp itself (exact integers, W-S19); the library's
// unit table stays empty.
#define USE_UNITS_IMPERIAL 0
#define USE_UNITS_ANGLE 0
#define USE_UNITS_PARTICLES 0
#define USE_UNITS_DISTANCE 0
#define USE_UNITS_MAGNETIC 0
#define USE_UNITS_LIGHT 0
#define USE_UNITS_ENERGY_FORCE_MASS 0
#define USE_UNITS_TIME 0
#define USE_UNITS_TEMPERATURE 0
#define USE_UNITS_RATIO 0
#define USE_UNITS_POWER 0
#define USE_UNITS_FREQUENCY 0
#define USE_UNITS_ELECTRIC 0
#define USE_UNITS_ELECTRIC_CHARGE_CONDUCTANCE 0
