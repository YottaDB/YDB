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

/* Written in 2015 by Sebastiano Vigna (vigna@acm.org)
	 To the extent possible under law, the author has dedicated all copyright
	 and related and neighboring rights to this software to the public domain
	 worldwide. This software is distributed without any warranty.

See <http://creativecommons.org/publicdomain/zero/1.0/>. */

#include "xoshiro.h"

/* This is a fixed-increment version of Java 8's SplittableRandom generator
   See http://dx.doi.org/10.1145/2714064.2660195 and
   http://docs.oracle.com/javase/8/docs/api/java/util/SplittableRandom.html

   It is a very fast generator passing BigCrush, and it can be useful if
   for some reason you absolutely want 64 bits of state. */

/*
 * Since this code is largely third-party, its form is somewhat different than
 * other code in YottaDB. This is intentional, at least for the initial version,
 * in order to allow for comparison with the original. This may be cleaned up
 * in future versions, as no revisions to the original public domain code are
 * expected. The original code is hosted at http://xoshiro.di.unimi.it .
 *
 *
 */

 GBLDEF uint64_t sm64_x;

uint64_t sm64_next(void) {
	uint64_t z = (sm64_x += 0x9e3779b97f4a7c15);
	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
	z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
	return z ^ (z >> 31);
}
