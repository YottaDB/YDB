/****************************************************************
 *								*
* Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
* All rights reserved.						*
*								*
*	This source code contains the intellectual property	*
*	of its copyright holder(s), and is made available	*
*	under a license.  If you do not know the terms of	*
*	the license, please stop and do not read further.	*
*								*
****************************************************************/

/* Written in 2018 by David Blackman and Sebastiano Vigna (vigna@acm.org)
	 To the extent possible under law, the author has dedicated all copyright
	 and related and neighboring rights to this software to the public domain
	 worldwide. This software is distributed without any warranty.

	 See <http://creativecommons.org/publicdomain/zero/1.0/>. */

#include "xoshiro.h"

/* This is xoshiro256+ 1.0, our best and fastest generator for floating-point
   numbers. We suggest to use its upper bits for floating-point
   generation, as it is slightly faster than xoshiro256++/xoshiro256**. It
   passes all tests we are aware of except for the lowest three bits,
   which might fail linearity tests (and just those), so if low linear
   complexity is not considered an issue (as it is usually the case) it
   can be used to generate 64-bit outputs, too.

   We suggest to use a sign test to extract a random Boolean value, and
   right shifts to extract subsets of bits.

   The state must be seeded so that it is not everywhere zero. If you have
   a 64-bit seed, we suggest to seed a splitmix64 generator and use its
   output to fill s. */

/*
 * Since this code is largely third-party, its form is somewhat different than
 * other code in YottaDB. This is intentional, at least for the initial version,
 * in order to allow for comparison with the original. This may be cleaned up
 * in future versions, as no revisions to the original public domain code are
 * expected. The original code is hosted at http://xoshiro.di.unimi.it .
 *
 *
 */

 GBLDEF uint64_t x256_s[4];

 static inline uint64_t x256_rotl(const uint64_t x, int k) {
 	return (x << k) | (x >> (64 - k));
 }

uint64_t x256_next(void) {
	const uint64_t result = x256_s[0] + x256_s[3];

	const uint64_t t = x256_s[1] << 17;

	x256_s[2] ^= x256_s[0];
	x256_s[3] ^= x256_s[1];
	x256_s[1] ^= x256_s[2];
	x256_s[0] ^= x256_s[3];

	x256_s[2] ^= t;

	x256_s[3] = x256_rotl(x256_s[3], 45);

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
   from each of which x256_jump() will generate 2^64 non-overlapping
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
