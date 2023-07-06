/****************************************************************
<<<<<<< HEAD
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
=======
 *                                                              *
 * Copyright (c) 2011-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
>>>>>>> 3c1c09f2 (GT.M V7.1-001)
 ****************************************************************/

#ifndef MURMURHASH_H
#define MURMURHASH_H 1

#include "gtm_common_defs.h"	/* needed for "uint4" and "ydb_uint8" types */

/*-----------------------------------------------------------------------------
 * MurmurHash3 was written by Austin Appleby, and is placed in the public
 * domain. The author hereby disclaims copyright to this source code.
 *
<<<<<<< HEAD
 * This version converted to C for use in GT.M/YottaDB by FIS/YottaDB.
=======
 * This version converted to C for use in GT.M by FIS.
 * The original implementation has been superseded by an incremental,
 * endian stable one, with only a few core pieces remaining.
>>>>>>> 3c1c09f2 (GT.M V7.1-001)
 *-----------------------------------------------------------------------------*/

#define ROTL32(X,Y)		(((X) << (Y)) | ((X) >> (32 - (Y))))
#define ROTL64(X,Y)		(((X) << (Y)) | ((X) >> (64 - (Y))))
#define BIG_CONSTANT(x)		(x##LLU)

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

<<<<<<< HEAD
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
=======
#define HASH128_EXTRACT_32(HASH)	((uint4)(HASH).one)

/* Classic non-incremental, endian unstable, implementation. */
void MurmurHash3_x86_32(const void *key, int len, uint4 seed, void *out);
>>>>>>> 3c1c09f2 (GT.M V7.1-001)

/* Incremental, endian stable implementation. */
inline void	gtmmrhash_32(const void *key, int len, uint4 seed, uint4 *out4);
inline void	gtmmrhash_128(const void *key, int len, uint4 seed, gtm_uint16 *out);
int		gtmmrhash_128_ingest(hash128_state_t *state, const void *key, int len);
void		gtmmrhash_128_result(hash128_state_t *state, uint4 total_len, gtm_uint16 *out);

inline void	gtmmrhash_128_hex(const gtm_uint16 *hash, unsigned char *out);
inline void	gtmmrhash_128_bytes(const gtm_uint16 *hash, unsigned char *out);

/* Implementation */

inline void gtmmrhash_128(const void *key, int len, uint4 seed, gtm_uint16 *out)
{
	hash128_state_t state;

	HASH128_STATE_INIT(state, seed);
	assert((state.carry_bytes == 0) && (state.c.one == 0) && (state.c.two == 0));
	gtmmrhash_128_ingest(&state, key, len);
	gtmmrhash_128_result(&state, len, out);
}

inline void gtmmrhash_32(const void *key, int len, uint4 seed, uint4 *out4)
{
	gtm_uint16	out16;

	gtmmrhash_128(key, len, seed, &out16);
	*out4 = HASH128_EXTRACT_32(out16);
}

inline void gtmmrhash_128_bytes(const gtm_uint16 *hash, unsigned char *out)
{
#	ifdef BIGENDIAN
#	define EXTRACT_BYTE(X, N)	(((uint64_t)(X) & ((uint64_t)0xff << (N) * 8)) >> (N) * 8)
	out[0] = EXTRACT_BYTE(hash->one, 0);
	out[1] = EXTRACT_BYTE(hash->one, 1);
	out[2] = EXTRACT_BYTE(hash->one, 2);
	out[3] = EXTRACT_BYTE(hash->one, 3);
	out[4] = EXTRACT_BYTE(hash->one, 4);
	out[5] = EXTRACT_BYTE(hash->one, 5);
	out[6] = EXTRACT_BYTE(hash->one, 6);
	out[7] = EXTRACT_BYTE(hash->one, 7);
	out[8] = EXTRACT_BYTE(hash->two, 0);
	out[9] = EXTRACT_BYTE(hash->two, 1);
	out[10] = EXTRACT_BYTE(hash->two, 2);
	out[11] = EXTRACT_BYTE(hash->two, 3);
	out[12] = EXTRACT_BYTE(hash->two, 4);
	out[13] = EXTRACT_BYTE(hash->two, 5);
	out[14] = EXTRACT_BYTE(hash->two, 6);
	out[15] = EXTRACT_BYTE(hash->two, 7);
#	else
	((gtm_uint8 *)out)[0] = hash->one;
	((gtm_uint8 *)out)[1] = hash->two;
#	endif
}

inline void gtmmrhash_128_hex(const gtm_uint16 *hash, unsigned char *out)
{
	int			i;
	unsigned char		bytes[16], n;

	gtmmrhash_128_bytes(hash, bytes);
	for (i = 0; i < 16; i++)
	{
		n = bytes[i] & 0xf;
		out[i * 2 + 1] = (n < 10) ? (n + '0') : (n - 10 + 'a');
		n = (bytes[i] >> 4) & 0xf;
		out[i * 2] = (n < 10) ? (n + '0') : (n - 10 + 'a');
	}
}
#endif

/*-----------------------------------------------------------------------------*/
