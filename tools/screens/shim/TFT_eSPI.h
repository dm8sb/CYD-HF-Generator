#pragma once
// Host screen renderer (tools/screens): DisplayPanel keeps a TFT_eSPI member; on the host it
// is an empty placeholder, the frames go to tools/screens/host_stubs.cpp instead.
class TFT_eSPI {};
