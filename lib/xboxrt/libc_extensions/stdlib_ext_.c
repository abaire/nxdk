// SPDX-License-Identifier: MIT

// SPDX-FileCopyrightText: 2019-2021 Stefan Schmidt

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <math.h>
#include <stdlib.h>
#include <threads.h>
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>

once_flag init_flag = ONCE_FLAG_INIT;
mtx_t rand_mutex;
unsigned char rand_buffer[116];

static void update_rand_buffer (void)
{
    LARGE_INTEGER performanceCounter;
    QueryPerformanceCounter(&performanceCounter);
    XcSHAUpdate(rand_buffer, (PUCHAR)&performanceCounter, sizeof(performanceCounter));
}

void init_rand_s (void)
{
    mtx_init(&rand_mutex, mtx_plain);

    XcSHAInit(rand_buffer);

    // Hash the EEPROM
    unsigned char eeprom_buffer[256];
    ULONG bytes_read;
    ULONG eeprom_type;
    ExQueryNonVolatileSetting(0xFFFF, &eeprom_type, eeprom_buffer, 256, &bytes_read);
    assert(bytes_read == 256);
    XcSHAUpdate(rand_buffer, eeprom_buffer, 256);

    update_rand_buffer();
}

// While rand_s is supposed to be cryptographically secure, this is not a very secure
// implementation. The choice of SHA-1 as the algorithm is not ideal, but convenient,
// as the kernel provides that for us. As the kernel doesn't help in collecting
// entropy, though, we're very limited in that regard.
int rand_s (unsigned int *randomValue)
{
    call_once(&init_flag, init_rand_s);

    // Microsoft's CRT has special handling for invalid parameters, which
    // nxdk doesn't have. We just act as if the invalid parameter handler
    // allowed continuing execution and make sure to assert in debug builds.
    assert(randomValue != NULL);
    if (!randomValue) {
        errno = EINVAL;
        return EINVAL;
    }

    mtx_lock(&rand_mutex);

    update_rand_buffer();

    // Extract four bytes from the hash as our random value, and feed it back
    // into the SHA algorithm
    unsigned char output_buffer[20];
    XcSHAFinal(rand_buffer, output_buffer);
    *randomValue = *((unsigned int *)output_buffer);
    XcSHAUpdate(rand_buffer, (PUCHAR)randomValue, sizeof(unsigned int));

    mtx_unlock(&rand_mutex);

    return 0;
}

unsigned short _byteswap_ushort (unsigned short val)
{
    return __builtin_bswap16(val);
}

unsigned long _byteswap_ulong (unsigned long val)
{
    return __builtin_bswap32(val);
}

unsigned __int64 _byteswap_uint64 (unsigned __int64 val)
{
    return __builtin_bswap64(val);
}

// Simulates a special case of strcasecmp, `target` must be lowercase.
// Returns the number of characters in `str` that match `target` after being
// converted via `tolower`.
static inline int matching_chars(const char *str, const char *target) {
  int ret = 0;

  for (; *str && *target && tolower(*str) == *target; ++str, ++target, ++ret) {
  }

  return ret;
}

double strtod( const char * _PDCLIB_restrict nptr, char * * _PDCLIB_restrict endptr )
{
    struct lconv * locale = localeconv();
    double sign = 1.0;
    const char * digit_end = NULL;
    BOOL is_hex = FALSE;
    int matching_len;

    while(*nptr && isspace(*nptr)) {
      ++nptr;
    }
    if (!*nptr) {
      return 0.0;
    }

    //  A valid floating point number for strtod using the "C" locale is formed by an optional sign character (+ or -), followed by one of:
    if (*nptr == '+') {
      ++nptr;
    } else if (*nptr == '-') {
      sign = -1.0;
      ++nptr;
    }

    if (!*nptr) {
      return 0.0;
    }

    matching_len = matching_chars(nptr, "infinity");
    if (matching_len == 3 || matching_len == 8) {
      if (endptr) {
        *endptr = (char *)nptr + matching_len;
      }
      return sign * INFINITY;
    }

    matching_len = matching_chars(nptr, "nan");
    if (matching_len == 3) {
      if (endptr) {
        *endptr = (char *)nptr + matching_len;
      }
      return NAN;
    }

    if (*nptr == '0' && (nptr[1] == 'x' || nptr[1] == 'X')) {
      // Hex
      is_hex = TRUE;
      nptr += 2;
    }

    digit_end = nptr;
    for (; *digit_end && (isdigit(*digit_end) || *digit_end == *locale->decimal_point)
    while (*digit_end &&
           )
    if (!isdigit(*nptr) && *nptr == '.') {
      return 0.0;
    }

//  - A sequence of digits, optionally containing a decimal-point character (.), optionally followed by an exponent part (an e or E character followed by an optional sign and a sequence of digits).
//  - A 0x or 0X prefix, then a sequence of hexadecimal digits (as in isxdigit) optionally containing a period which separates the whole and fractional number parts. Optionally followed by a power of 2 exponent (a p or P character followed by an optional sign and a sequence of hexadecimal digits).
//  - INF or INFINITY (ignoring case).
//  - NAN or NANsequence (ignoring case), where sequence is a sequence of characters, where each character is either an alphanumeric character (as in isalnum) or the underscore character (_).
    return 0.0;
}

float strtof( const char * _PDCLIB_restrict nptr, char * * _PDCLIB_restrict endptr )
{
    assert(0);
    return 0.0;
}

long double strtold( const char * _PDCLIB_restrict nptr, char * * _PDCLIB_restrict endptr )
{
    assert(0);
    return 0.0;
}

int mbtowc (wchar_t *pwc, const char *string, size_t n)
{
    assert(0);
    return 0;
}

