#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#include "lb303/lb303.h"
#include "triple_oscillator/triple_oscillator.h"
#include "envelope_generator/envelope_generator.h"

const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
	switch (index) {
	case 0:
		return &lb303_descriptor;
	case 1:
		return &triple_oscillator_descriptor;
	case 2:
		return &envelope_generator_descriptor;
	default:
		return NULL;
	}
}

