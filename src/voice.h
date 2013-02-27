#ifndef VOICE_H__
#define VOICE_H__

#include "basic_filters.h"
#include "envelope.h"
#include "lfo.h"

typedef struct {
	// Standard voice state
	uint8_t    midi_note;
	uint32_t   frame;

	// Generator
	// TODO: Reverse this relationship?? TripOscVoice contain some VoiceState struct?
	void *generator;

	// Volume Envelope TODO: Remove from generic Voice?
	Envelope  *env_vol;
	Lfo       *lfo_vol;

	// Filter envelopes and state
	Envelope  *env_cut;
	Lfo       *lfo_cut;
	Envelope  *env_res;
	Lfo       *lfo_res;
	Filter    *filter;

	// TODO: Function pointers for processing the voice
	
} Voice;

#endif 
