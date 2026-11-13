// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
// Host screen renderer (tools/screens): DisplayPanel keeps a TFT_eSPI member; on the host it
// is an empty placeholder, the frames go to tools/screens/host_stubs.cpp instead.
class TFT_eSPI {};
