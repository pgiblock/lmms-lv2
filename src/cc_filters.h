#ifndef CC_FILTERS_H__
#define CC_FILTERS_H__

#include <assert.h>

float
cc_lag (float *value, float in, float coef)
{
	assert(coef >= 0.0f);

	return (*value) = ((1.0f - coef) * in) + (coef * (*value));
}

#endif // CC_FILTERS_H__
