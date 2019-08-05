/*	$OpenBSD: arc4random.c,v 1.53 2015/09/10 18:53:50 bcook Exp $	*/

/*
 * Copyright (c) 1996, David Mazieres <dm@uun.org>
 * Copyright (c) 2008, Damien Miller <djm@openbsd.org>
 * Copyright (c) 2013, Markus Friedl <markus@openbsd.org>
 * Copyright (c) 2014, Theo de Raadt <deraadt@openbsd.org>
 * Copyright (c) 2015, Guillem Jover <guillem@hadrons.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * ChaCha based random number generator for OpenBSD.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define KEYSTREAM_ONLY
#include "chacha_private.h"

#define minimum(a, b) ((a) < (b) ? (a) : (b))

#if defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER)
#define inline __inline
#else				/* __GNUC__ || __clang__ || _MSC_VER */
#define inline
#endif				/* !__GNUC__ && !__clang__ && !_MSC_VER */

#define KEYSZ	32
#define IVSZ	8
#define BLOCKSZ	64
#define RSBUFSZ	(16*BLOCKSZ)

/* Marked MAP_INHERIT_ZERO, so zero'd out in fork children. */
typedef struct _rs {
	size_t		rs_have;	/* valid bytes at end of rs_buf */
	size_t		rs_count;	/* bytes till reseed */
} rs_t;

/* Maybe be preserved in fork children, if _rs_allocate() decides. */
typedef struct _rsx {
	chacha_ctx	rs_chacha;	/* chacha context for random keystream */
	unsigned char	rs_buf[RSBUFSZ];	/* keystream blocks */
} rsx_t;

static rs_t rs_s = {0};
static rsx_t rsx_s = {0};
static rs_t *rs = NULL;
static rsx_t *rsx = NULL;

#define _ARC4_LOCK()
#define _ARC4_UNLOCK()
#define _rs_forkdetect()

static void
explicit_bzero(void *v, size_t n) {
    volatile unsigned char *p = v; while( n-- ) *p++ = 0;
}

#if defined(__linux__)

#include <sys/syscall.h>

#ifdef SYS_getrandom

#ifndef GRND_NONBLOCK
#define GRND_NONBLOCK   0x0001
#endif

#endif

static int
getentropy(void *buf, size_t len)
{
	int ret;
	if (len > 256)
		return (-1);
	ret = syscall(SYS_getrandom, buf, len, GRND_NONBLOCK);
	if (ret != (int)len)
		return (-1);
	return (0);
}

#elif defined(_WIN32)

#include <windows.h>
#include <wincrypt.h>

static int
getentropy(void *buf, size_t len)
{
	HCRYPTPROV provider;
	if (len > 256) {
		return (-1);
	}
	if (CryptAcquireContext(&provider, NULL, NULL, PROV_RSA_FULL,
	    CRYPT_VERIFYCONTEXT) == 0)
		goto fail;
	if (CryptGenRandom(provider, len, buf) == 0) {
		CryptReleaseContext(provider, 0);
		goto fail;
	}
	CryptReleaseContext(provider, 0);
	return (0);
fail:
	return (-1);
}

#endif

static int
getentropy_fallback(uint8_t *output, int len) {
    static uint64_t s[2] = {0};
    static int inited = 0;
    if (!inited) {
        uint32_t seed = (uint32_t)time(NULL);
        s[0] = seed | 0x100000000L;
        s[1] = ((uint64_t)seed << 32) | 0x1;
        inited = 1;
    }

    while (len > 0) {
        uint32_t rd;
        const int blen = minimum(len, sizeof(rd));
        uint64_t *state = &s[0];
        uint64_t *inc = &s[1];

        // *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
        // Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)
        uint64_t oldstate = *state;
        // Advance internal state
        *state = oldstate * 6364136223846793005ULL + (*inc|1);
        // Calculate output function (XSH RR), uses old state for max ILP
        uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
        uint32_t rot = oldstate >> 59u;
        rd = (xorshifted >> rot) | (xorshifted << ((-rot) & 31));

        memcpy(output, (uint8_t *)&rd, blen);
        output += blen;
        len    -= blen;
    }
    return 1;
}

static inline void
_rs_init(unsigned char *buf, size_t n)
{
	if (n < KEYSZ + IVSZ)
		return;

	if (rs == NULL) {
		rs = &rs_s;
		rsx = &rsx_s;
		explicit_bzero(rs, sizeof(rs_s));
		explicit_bzero(rsx, sizeof(rsx_s));
	}

	chacha_keysetup(&rsx->rs_chacha, buf, KEYSZ * 8, 0);
	chacha_ivsetup(&rsx->rs_chacha, buf + KEYSZ);
}

static inline void
_rs_rekey(unsigned char *dat, size_t datlen)
{
#ifndef KEYSTREAM_ONLY
	memset(rsx->rs_buf, 0, sizeof(rsx->rs_buf));
#endif
	/* fill rs_buf with the keystream */
	chacha_encrypt_bytes(&rsx->rs_chacha, rsx->rs_buf,
	    rsx->rs_buf, sizeof(rsx->rs_buf));
	/* mix in optional user provided data */
	if (dat) {
		size_t i, m;

		m = minimum(datlen, KEYSZ + IVSZ);
		for (i = 0; i < m; i++)
			rsx->rs_buf[i] ^= dat[i];
	}
	/* immediately reinit for backtracking resistance */
	_rs_init(rsx->rs_buf, KEYSZ + IVSZ);
	memset(rsx->rs_buf, 0, KEYSZ + IVSZ);
	rs->rs_have = sizeof(rsx->rs_buf) - KEYSZ - IVSZ;
}

static void
_rs_stir(void)
{
	unsigned char rnd[KEYSZ + IVSZ];

	if (getentropy(rnd, sizeof rnd) == -1) {
		getentropy_fallback(rnd, sizeof rnd);
	}

	if (!rs)
		_rs_init(rnd, sizeof(rnd));
	else
		_rs_rekey(rnd, sizeof(rnd));
	explicit_bzero(rnd, sizeof(rnd));	/* discard source seed */

	/* invalidate rs_buf */
	rs->rs_have = 0;
	memset(rsx->rs_buf, 0, sizeof(rsx->rs_buf));

	rs->rs_count = 1600000;
}

static inline void
_rs_stir_if_needed(size_t len)
{
	_rs_forkdetect();
	if (!rs || rs->rs_count <= len)
		_rs_stir();
	if (rs->rs_count <= len)
		rs->rs_count = 0;
	else
		rs->rs_count -= len;
}

static inline void
_rs_random_buf(void *_buf, size_t n)
{
	unsigned char *buf = (unsigned char *)_buf;
	unsigned char *keystream;
	size_t m;

	_rs_stir_if_needed(n);
	while (n > 0) {
		if (rs->rs_have > 0) {
			m = minimum(n, rs->rs_have);
			keystream = rsx->rs_buf + sizeof(rsx->rs_buf)
			    - rs->rs_have;
			memcpy(buf, keystream, m);
			memset(keystream, 0, m);
			buf += m;
			n -= m;
			rs->rs_have -= m;
		}
		if (rs->rs_have == 0)
			_rs_rekey(NULL, 0);
	}
}

static inline void
_rs_random_u32(uint32_t *val)
{
	unsigned char *keystream;

	_rs_stir_if_needed(sizeof(*val));
	if (rs->rs_have < sizeof(*val))
		_rs_rekey(NULL, 0);
	keystream = rsx->rs_buf + sizeof(rsx->rs_buf) - rs->rs_have;
	memcpy(val, keystream, sizeof(*val));
	memset(keystream, 0, sizeof(*val));
	rs->rs_have -= sizeof(*val);
}

void
arc4random_stir(void)
{
	_ARC4_LOCK();
	_rs_stir();
	_ARC4_UNLOCK();
}

void
arc4random_addrandom(unsigned char *dat, int datlen)
{
	_ARC4_LOCK();
	_rs_stir_if_needed(datlen);
	_rs_rekey(dat, datlen);
	_ARC4_UNLOCK();
}

uint32_t
arc4random(void)
{
	uint32_t val;

	_ARC4_LOCK();
	_rs_random_u32(&val);
	_ARC4_UNLOCK();
	return val;
}

void
arc4random_buf(void *buf, size_t n)
{
	_ARC4_LOCK();
	_rs_random_buf(buf, n);
	_ARC4_UNLOCK();
}

/*
 * Calculate a uniformly distributed random number less than upper_bound
 * avoiding "modulo bias".
 *
 * Uniformity is achieved by generating new random numbers until the one
 * returned is outside the range [0, 2**32 % upper_bound).  This
 * guarantees the selected random number will be inside
 * [2**32 % upper_bound, 2**32) which maps back to [0, upper_bound)
 * after reduction modulo upper_bound.
 */
uint32_t
arc4random_uniform(uint32_t upper_bound)
{
	uint32_t r, min;

	if (upper_bound < 2)
		return 0;

	/* 2**32 % x == (2**32 - x) % x */
	min = -upper_bound % upper_bound;

	/*
	 * This could theoretically loop forever but each retry has
	 * p > 0.5 (worst case, usually far better) of selecting a
	 * number inside the range we need, so it should rarely need
	 * to re-roll.
	 */
	for (;;) {
		r = arc4random();
		if (r >= min)
			break;
	}

	return r % upper_bound;
}
