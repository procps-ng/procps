#include <stdio.h>

static double power(unsigned int base, unsigned int expo);

static double power(unsigned int base, unsigned int expo)
{
	return (expo == 0) ? 1 : base * power(base, expo - 1);
}

/* idea of this function is copied from top size scaling */
const char *scale_size(unsigned long size, unsigned int exponent, int si, int humanreadable)
{
	static char up[] = { 'B', 'K', 'M', 'G', 'T', 'P', 0 };
	static char buf[BUFSIZ];
	int i;
	unsigned int base = si ? 1000 : 1024;
	long long bytes = size * 1024LL;

	if (!humanreadable) {
		switch (exponent) {
		case 0:
			/* default output */
			snprintf(buf, sizeof(buf), "%ld", (long int)(bytes / (long long int)base));
			return buf;
		case 1:
			/* in bytes, which can not be in SI */
			snprintf(buf, sizeof(buf), "%lld", bytes);
			return buf;
		default:
			/* In desired scale. */
			snprintf(buf, sizeof(buf), "%ld",
			        (long)(bytes / power(base, exponent-1)));
			return buf;
		}
	}

	/* human readable output */
	if (4 >= snprintf(buf, sizeof(buf), "%lld%c", bytes, up[0]))
		return buf;

	double power = base;
	if (si) {
		for (i = 1; up[i] != 0; i++) {
			if (4 >= snprintf(buf, sizeof(buf), "%.1f%c",
			                  (float)(bytes / power), up[i]))
				return buf;
			if (4 >= snprintf(buf, sizeof(buf), "%ld%c",
			                  (long)(bytes / power), up[i]))
				return buf;
			power *= base;
		}
	} else {
		for (i = 1; up[i] != 0; i++) {
			if (5 >= snprintf(buf, sizeof(buf), "%.1f%ci",
			                  (float)(bytes / power), up[i]))
				return buf;
			if (5 >= snprintf(buf, sizeof(buf), "%ld%ci",
			                  (long)(bytes / power), up[i]))
				return buf;
			power *= base;
		}
	}
	/*
	 * On system where there is more than exbibyte of memory or swap the
	 * output does not fit to column. For incoming few years this should
	 * not be a big problem (wrote at Apr, 2015).
	 */
	return buf;
}
