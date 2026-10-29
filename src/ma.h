#pragma once

// miniaudio con lo justo: solo WASAPI (Windows), sin motor ni grabación a archivo.
// Estos defines cambian las estructuras, así que todo archivo que use miniaudio debe incluir este, no miniaudio.h.

#define MA_ENABLE_ONLY_SPECIFIC_BACKENDS
#define MA_ENABLE_WASAPI
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE

#include <miniaudio.h>
