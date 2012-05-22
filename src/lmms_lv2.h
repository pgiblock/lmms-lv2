#ifndef LMMS_LV2_H__
#define LMMS_LV2_H__

// "Standard" Headers

#include "config.h"

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"
#include "lv2/lv2plug.in/ns/ext/state/state.h"
#include "lv2/lv2plug.in/ns/ext/atom/util.h"
#include "lv2/lv2plug.in/ns/ext/urid/urid.h"

// Typedefs and Utility functions

#define WHITE_KEYS_PER_OCTAVE (7)
#define BLACK_KEYS_PER_OCTAVE (5)
#define KEYS_PER_OCTAVE       (WHITE_KEYS_PER_OCTAVE + BLACK_KEYS_PER_OCTAVE)

typedef float       sample_t;  // standard sample type
typedef uint16_t    fpp_t;     // frames per period (0-16384)

#endif //LMMS_LV2_H__
