#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#include "lb303/lb303.h"

LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
	switch (index) {
		case 0:
			return &lb303_descriptor;
		default:
			return NULL;
	}
}

