#include "libm.h"

/* tanh(x) = (exp(x) - exp(-x))/(exp(x) + exp(-x))
 *         = (exp(2*x) - 1)/(exp(2*x) - 1 + 2)
 *         = (1 - exp(-2*x))/(exp(-2*x) - 1 + 2)
 */
double tanh(double x)
{
	uint32_t w;
	int sign;
	double_t t;

	/* x = |x| */
	/// Original implementation
	// union {double f; uint64_t i;} u = {.f = x};
	// sign = u.i >> 63;
	// u.i &= (uint64_t)-1/2;
	// x = u.f;
	// w = u.i >> 32;
	/// Safe implementation in Snitch
	uint32_t upper_32b_x = safe_extract_upper_32b_from_double(x);
	sign = upper_32b_x >> 31;
	uint32_t sign_mask = (~(1 << 31));
	w = upper_32b_x & sign_mask;
	safe_inject_into_upper_32b_double(w, &x);

	if (w > 0x3fe193ea) {
		/* |x| > log(3)/2 ~= 0.5493 or nan */
		if (w > 0x40340000) {
			/* |x| > 20 or nan */
			/* note: this branch avoids raising overflow */
			t = 1 - 0/x;
		} else {
			t = expm1(2*x);
			t = 1 - 2/(t+2);
		}
	} else if (w > 0x3fd058ae) {
		/* |x| > log(5/3)/2 ~= 0.2554 */
		t = expm1(2*x);
		t = t/(t+2);
	} else if (w >= 0x00100000) {
		/* |x| >= 0x1p-1022, up to 2ulp error in [0.1,0.2554] */
		t = expm1(-2*x);
		t = -t/(t+2);
	} else {
		/* |x| is subnormal */
		/* note: the branch above would not raise underflow in [0x1p-1023,0x1p-1022) */
		FORCE_EVAL((float)x);
		t = x;
	}
	return sign ? -t : t;
}
