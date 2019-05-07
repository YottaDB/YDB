/****************************************************************
 *								*
 * Copyright 2011, 2014 Fidelity Information Services, Inc	*
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

#ifndef MURMURHASH_H
#define MURMURHASH_H 1

#include "gtm_common_defs.h"	/* needed for "uint4" and "ydb_uint8" types */

/*-----------------------------------------------------------------------------
 * MurmurHash3 was written by Austin Appleby, and is placed in the public
 * domain. The author hereby disclaims copyright to this source code.
 *
 * This version converted to C for use in GT.M/YottaDB by FIS/YottaDB.
 *-----------------------------------------------------------------------------*/

/* Note that all these want the key to be a multiple of N bits, where N is the
   last number in the function name. */

void MurmurHash3_x86_32  ( const void * key, int len, uint4 seed, void * out );

void MurmurHash3_x86_128 ( const void * key, int len, uint4 seed, void * out );

void MurmurHash3_x64_128 ( const void * key, int len, uint4 seed, void * out );

typedef struct
{
	ydb_uint8	one, two;
} ydb_uint16;

typedef ydb_uint16	gtm_uint16;

typedef struct
{
	ydb_uint16	h, c;
	int		carry_bytes;
} hash128_state_t;

/* Do not use MBSTART/MBEND as it requires "gtm_common_defs.h" which we would rather not require
 * as this file ("mmrhash.h") is an externally visible header file.
 */
#define HASH128_STATE_INIT(STATE, SEED)			\
{							\
	(STATE).h.one = (STATE).h.two = (SEED);		\
	(STATE).c.one = (STATE).c.two = 0;		\
	(STATE).carry_bytes = 0;			\
}

/* Returns a 32-bit (4-byte) endian-independent murmur hash */
void ydb_mmrhash_32(const void *key, int len, uint4 seed, uint4 *out4);

/* Returns a 128-bit (16-byte) endian-independent murmur hash */
void ydb_mmrhash_128(const void *key, int len, uint4 seed, ydb_uint16 *out);

/* The below 2 functions enable users to get a murmur hash through a series of incremental operations.
 * The sequence is to first initialize the "state" variable using the HASH128_STATE_INIT macro,
 * then call "ydb_mmrhash_128_ingest()" one or more times and finally call "ydb_mmrhash_128_result()" to
 * obtain the final hash value. "key" points to the input character array (of length "len") for the hash.
 * "total_len" can be set to any 4-byte value that is unique for the given input whose hash is being determined.
 * An example is to set it to the sum of the "len" values passed in across all calls to "ydb_mmrhash_128_ingest"
 * before "ydb_mmrhash_128_result" is called. "out" points to the structure holding the 16-byte hash result.
 */
void ydb_mmrhash_128_ingest(hash128_state_t *state, const void *key, int len);
void ydb_mmrhash_128_result(hash128_state_t *state, uint4 total_len, ydb_uint16 *out);

/* The below function returns a hex formatted representation of a 16-byte hash value */
void ydb_mmrhash_128_hex(const ydb_uint16 *hash, unsigned char *out);

/* The below functions converts the 16-byte hash stored in a "ydb_uint16" structure (2 8-byte integers)
 * into a byte array "out" of 16 characters. It is also internally used by "ydb_mmrhash_128_hex()".
 */
void ydb_mmrhash_128_bytes(const ydb_uint16 *hash, unsigned char *out);

#endif

/*-----------------------------------------------------------------------------*/
