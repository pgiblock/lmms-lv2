#ifndef LMMS_LV2_H__
#define LMMS_LV2_H__

// "Standard" Headers

#include <math.h>
#include "config.h"

// "Standard" LV2 Headers

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"
#include "lv2/lv2plug.in/ns/ext/state/state.h"

// Switch "Experimental" LV2

#ifdef USE_LV2_ATOM
#include "lv2/lv2plug.in/ns/ext/atom/atom-buffer.h"
#include "lv2/lv2plug.in/ns/ext/atom/atom-helpers.h"
typedef LV2_Atom_Buffer      Event_Buffer_t;
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

#ifndef M_PI
#define M_PI (3.14159265358979323846264338327)
#endif

#define WHITE_KEYS_PER_OCTAVE (7)
#define BLACK_KEYS_PER_OCTAVE (5)
#define KEYS_PER_OCTAVE       (WHITE_KEYS_PER_OCTAVE + BLACK_KEYS_PER_OCTAVE)


typedef float       sample_t;  // standard sample type
typedef uint16_t    fpp_t;     // frames per period (0-16384)

static inline float absFraction( const float _x ) { 
	return( _x - ( _x >= 0.0f ? floorf( _x ) : floorf( _x ) - 1 ) ); 
}


// TODO: Avoid this in triple-osc
static inline float fraction(float _x) {
	return( _x - floorf( _x ) );
}


#define FAST_RAND_MAX 32767
static inline int fast_rand() {
	static unsigned long next = 1;
	next = next * 1103515245 + 12345;
	return( (unsigned)( next / 65536 ) % 32768 );
}


#endif //LMMS_LV2_H__
