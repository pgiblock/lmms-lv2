#ifndef VOICE_H__
#define VOICE_H__

#include "envelope.h"

typedef struct {
	// Standard voice state
	uint8_t    midi_note;
	uint32_t   frame;

	// Generator
	// TODO: Reverse this relationship?? TripOscVoice contain some VoiceState struct?
	void *generator;

	// Volume Envelope TODO: Remove from generic Voice?
	Envelope  *env_vol;

	// TODO: Function pointers for processing the voice
	
} Voice;

#endif 
