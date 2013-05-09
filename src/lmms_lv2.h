#ifndef LMMS_LV2_H__
#define LMMS_LV2_H__

// Pollute our namespace with some "Standard" Headers

#include "config.h"

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"
#include "lv2/lv2plug.in/ns/ext/state/state.h"
#include "lv2/lv2plug.in/ns/ext/atom/util.h"
#include "lv2/lv2plug.in/ns/ext/urid/urid.h"

// Typedefs and Utility functions

#define WHITE_KEYS_PER_OCTAVE (7)
#define BLACK_KEYS_PER_OCTAVE (5)
#define KEYS_PER_OCTAVE       (WHITE_KEYS_PER_OCTAVE + BLACK_KEYS_PER_OCTAVE)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define BEGIN_CONNECT_PORTS(port) switch (port) {
#define CONNECT_PORT(val, buf, type) case val: plugin->buf = (type *)data; break;
#define END_CONNECT_PORTS() default: break; }

typedef float       sample_t;  // standard sample type
typedef uint16_t    fpp_t;     // frames per period (0-16384)

#endif //LMMS_LV2_H__
