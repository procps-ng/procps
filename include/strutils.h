/*
 * This header was copied from util-linux at fall 2011.
 */

#ifndef PROCPS_NG_STRUTILS
#define PROCPS_NG_STRUTILS

extern long strtol_or_err(const char *str, const char *errmesg);
extern double strtod_or_err(const char *str, const char *errmesg);
extern double strtod_nol_or_err(char *str, const char *errmesg);

/* Like stpncpy, but do ensure that the result is NUL-terminated,
   and do not NUL-pad out to LEN.  I.e., when strnlen (src, len) == len,
   this function writes a NUL byte into dest[len].  Thus, the length
   of the destination buffer must be at least LEN + 1.
   The DEST and SRC buffers must not overlap.  */
static inline char *
stzncpy (char *restrict dest, char const *restrict src, size_t len)
{
  char const *src_end = src + len;
  while (src < src_end && *src)
    *dest++ = *src++;
  *dest = 0;
  return dest;
}

#endif
