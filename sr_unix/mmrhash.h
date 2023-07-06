/****************************************************************
 *                                                              *
 * Copyright (c) 2011-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#ifndef MURMURHASH_H
#define MURMURHASH_H 1

/*-----------------------------------------------------------------------------
 * MurmurHash3 was written by Austin Appleby, and is placed in the public
 * domain. The author hereby disclaims copyright to this source code.
 *
 * This version converted to C for use in GT.M by FIS.
 * The original implementation has been superseded by an incremental,
 * endian stable one, with only a few core pieces remaining.
 *-----------------------------------------------------------------------------*/

#define ROTL32(X,Y)		(((X) << (Y)) | ((X) >> (32 - (Y))))
#define ROTL64(X,Y)		(((X) << (Y)) | ((X) >> (64 - (Y))))
#define BIG_CONSTANT(x)		(x##LLU)

typedef struct
{
	gtm_uint8	one, two;
} gtm_uint16;

typedef struct
{
	gtm_uint16	h, c;
	int		carry_bytes;
} hash128_state_t;

#define HASH128_STATE_INIT(STATE, SEED)							\
MBSTART {										\
	(STATE).h.one = (STATE).h.two = (SEED);						\
	(STATE).c.one = (STATE).c.two = 0;						\
	(STATE).carry_bytes = 0;							\
} MBEND

#define HASH128_EXTRACT_32(HASH)	((uint4)(HASH).one)

/* Classic non-incremental, endian unstable, implementation. */
void MurmurHash3_x86_32(const void *key, int len, uint4 seed, void *out);

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
