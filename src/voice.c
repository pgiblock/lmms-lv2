
void
voice_steal(Voice* triposc, uint8_t midi_note) {
	// We are just going round-robin for now
	// TODO: Prioritize on vol-envelope state
	TripOscVoice* v = &triposc->voices[triposc->victim_idx];

	// Stealing
	v->midi_note = midi_note;
	// Init note
	float freq  = powf(2.0f, ((float)midi_note-69.0f) / 12.0f) * 440.0f;

	// Reset oscillators backwards, wee...
	for (int i=2; i>=0; --i) {
		OscillatorUnit* u = &triposc->units[i];
		// FIXME: This check won't be needed if we fix oscillator to just hold float* members bound straight to ports
		float mod = (i==2)? 0 : *u->modulation_port;
		// FIXME: Detuning needs to happen occationally even after note-on
		float detune_l = powf( 2.0f, (*u->detune_coarse_port * 100.0f + *u->detune_fine_l_port) / 1200.0f) / triposc->srate;
		float detune_r = powf( 2.0f, (*u->detune_coarse_port * 100.0f + *u->detune_fine_r_port) / 1200.0f) / triposc->srate;
		osc_reset(&(v->osc_l[i]), *u->wave_shape_port, mod,
							freq, detune_l, *u->vol_port*0.01f,
							i==2?NULL:&(v->osc_l[i+1]), *u->phase_offset_port,
							triposc->srate);
		osc_reset(&(v->osc_r[i]), *u->wave_shape_port, mod,
							freq, detune_r, *u->vol_port*0.01f,
							i==2?NULL:&(v->osc_r[i+1]), *u->phase_offset_port,
							triposc->srate);
	}

	// Trigger envelopes
	envelope_trigger(v->env_vol);

	// Pick next victim
	triposc->victim_idx = (triposc->victim_idx+1) % NUM_VOICES;
}
	

void
voice_release(TripleOscillator* triposc, uint8_t midi_note) {
	for (int i=0; i<NUM_VOICES; ++i) {
		TripOscVoice* v = &triposc->voices[i];
		if (v->midi_note == midi_note) {
			envelope_release(v->env_vol);
		}
	}
}

