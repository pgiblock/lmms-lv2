#ifndef LMMS_LV2_H__
#define LMMS_LV2_H__

// "Standard" Headers

#include "config.h"

// "Standard" LV2 Headers

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"
#include "lv2/lv2plug.in/ns/ext/state/state.h"

// Switch "Experimental" LV2

#ifdef USE_LV2_ATOM
#include "lv2/lv2plug.in/ns/ext/atom/util.h"
typedef LV2_Atom_Sequence      Event_Buffer_t;
#else
#include "lv2/lv2plug.in/ns/ext/event/event.h"
#include "lv2/lv2plug.in/ns/ext/event/event-helpers.h"
typedef LV2_Event_Buffer     Event_Buffer_t;
#endif

#ifdef USE_LV2_URID
#include "lv2/lv2plug.in/ns/ext/urid/urid.h"
typedef LV2_URID    URI_t;
#else
#include "lv2/lv2plug.in/ns/ext/uri-map/uri-map.h"
typedef uint32_t    URI_t;
#endif // USE_LV2_URID

// Typedefs and Utility functions

#define WHITE_KEYS_PER_OCTAVE (7)
#define BLACK_KEYS_PER_OCTAVE (5)
#define KEYS_PER_OCTAVE       (WHITE_KEYS_PER_OCTAVE + BLACK_KEYS_PER_OCTAVE)

typedef float       sample_t;  // standard sample type
typedef uint16_t    fpp_t;     // frames per period (0-16384)

#endif //LMMS_LV2_H__
