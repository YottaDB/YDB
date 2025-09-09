/****************************************************************
 *								*
* Copyright (c) 2025 YottaDB LLC and/or its subsidiaries.	*
* All rights reserved.						*
*								*
*	This source code contains the intellectual property	*
*	of its copyright holder(s), and is made available	*
*	under a license.  If you do not know the terms of	*
*	the license, please stop and do not read further.	*
*								*
****************************************************************/

/*
 * Since this code is largely third-party, its form is somewhat different than
 * other code in YottaDB. This is intentional, at least for the initial version,
 * in order to allow for comparison with the original. This may be cleaned up
 * in future versions, as no revisions to the original public domain code are
 * expected. The original code is hosted at https://prng.di.unimi.it/xoshiro256plusplus.c
 */

/*  Written in 2019 by David Blackman and Sebastiano Vigna (vigna@acm.org)

To the extent possible under law, the author has dedicated all copyright
and related and neighboring rights to this software to the public domain
worldwide.

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#include "xoshiro.h"

/* This is xoshiro256++ 1.0, one of our all-purpose, rock-solid generators.
   It has excellent (sub-ns) speed, a state (256 bits) that is large
   enough for any parallel application, and it passes all tests we are
   aware of.

   For generating just floating-point numbers, xoshiro256+ is even faster.

   The state must be seeded so that it is not everywhere zero. If you have
   a 64-bit seed, we suggest to seed a splitmix64 generator and use its
   output to fill s. */

static inline uint64_t rotl(const uint64_t x, int k) {
	return (x << k) | (x >> (64 - k));
}


GBLDEF uint64_t x256_s[4];

uint64_t x256_next(void) {
	const uint64_t result = rotl(x256_s[0] + x256_s[3], 23) + x256_s[0];

	const uint64_t t = x256_s[1] << 17;

	x256_s[2] ^= x256_s[0];
	x256_s[3] ^= x256_s[1];
	x256_s[1] ^= x256_s[2];
	x256_s[0] ^= x256_s[3];

	x256_s[2] ^= t;

	x256_s[3] = rotl(x256_s[3], 45);

	return result;
}


/* This is the jump function for the generator. It is equivalent
   to 2^128 calls to x256_next(); it can be used to generate 2^128
   non-overlapping subsequences for parallel computations. */

void x256_jump(void) {
	static const uint64_t JUMP[] = { 0x180ec6d33cfd0aba, 0xd5a61266f0c9392c, 0xa9582618e03fc9aa, 0x39abdc4529b1661c };

	uint64_t s0 = 0;
	uint64_t s1 = 0;
	uint64_t s2 = 0;
	uint64_t s3 = 0;
	for(int i = 0; i < sizeof JUMP / sizeof *JUMP; i++)
		for(int b = 0; b < 64; b++) {
			if (JUMP[i] & UINT64_C(1) << b) {
				s0 ^= x256_s[0];
				s1 ^= x256_s[1];
				s2 ^= x256_s[2];
				s3 ^= x256_s[3];
			}
			x256_next();
		}

	x256_s[0] = s0;
	x256_s[1] = s1;
	x256_s[2] = s2;
	x256_s[3] = s3;
}



/* This is the long-jump function for the generator. It is equivalent to
   2^192 calls to x256_next(); it can be used to generate 2^64 starting points,
   from each of which x256_long_jump() will generate 2^64 non-overlapping
   subsequences for parallel distributed computations. */

void x256_long_jump(void) {
	static const uint64_t LONG_JUMP[] = { 0x76e15d3efefdcbbf, 0xc5004e441c522fb3, 0x77710069854ee241, 0x39109bb02acbe635 };

	uint64_t s0 = 0;
	uint64_t s1 = 0;
	uint64_t s2 = 0;
	uint64_t s3 = 0;
	for(int i = 0; i < sizeof LONG_JUMP / sizeof *LONG_JUMP; i++)
		for(int b = 0; b < 64; b++) {
			if (LONG_JUMP[i] & UINT64_C(1) << b) {
				s0 ^= x256_s[0];
				s1 ^= x256_s[1];
				s2 ^= x256_s[2];
				s3 ^= x256_s[3];
			}
			x256_next();
		}

	x256_s[0] = s0;
	x256_s[1] = s1;
	x256_s[2] = s2;
	x256_s[3] = s3;
}
