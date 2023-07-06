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

/*-----------------------------------------------------------------------------
 * MurmurHash3 was written by Austin Appleby, and is placed in the public
 * domain. The author hereby disclaims copyright to this source code.
 *
 * This version converted to C for use in GT.M by FIS.
 * The original implementation has been superseded by an incremental,
 * endian stable one, with only a few core pieces remaining.
 *-----------------------------------------------------------------------------*/

#include "mdef.h"
#include "gtm_string.h"
#include "mmrhash.h"

#define C1 BIG_CONSTANT(0x87c37b91114253d5)
#define C2 BIG_CONSTANT(0x4cf5ad432745937f)
#define M1 0x52dce729
#define M2 0x38495ab5

#define LSHIFT_BYTES_SAFE(LHS, RHS)	((LHS) << ((RHS) * 8))
#define RSHIFT_BYTES_SAFE(LHS, RHS)	((LHS) >> ((RHS) * 8))
#define LSHIFT_BYTES(LHS, RHS)		((RHS < SIZEOF(LHS)) ? LSHIFT_BYTES_SAFE(LHS, RHS) : 0)
#define RSHIFT_BYTES(LHS, RHS)		((RHS < SIZEOF(LHS)) ? RSHIFT_BYTES_SAFE(LHS, RHS) : 0)

/* Implementation */

#ifdef BIGENDIAN
#	if defined(__GNUC__) && (__GNUC__>4 || (__GNUC__==4 && __GNUC_MINOR__>=3))
#	define UI64_TO_LE(X) (__builtin_bswap64(*((uint64_t*)&(X))))
#	else
#	define UI64_TO_LE(X) (X) = ((((X) & BIG_CONSTANT(0xff)) << 56)								\
					| (((X) & BIG_CONSTANT(0xff00)) << 40)							\
					| (((X) & BIG_CONSTANT(0xff0000)) << 24)						\
					| (((X) & BIG_CONSTANT(0xff000000)) << 8)						\
					| (((X) & BIG_CONSTANT(0xff00000000)) >> 8)						\
					| (((X) & BIG_CONSTANT(0xff0000000000)) >> 24)						\
					| (((X) & BIG_CONSTANT(0xff000000000000)) >> 40)					\
					| (((X) & BIG_CONSTANT(0xff00000000000000)) >> 56))
#	endif
#else
#define UI64_TO_LE(X)			/* nothing */
#endif

#define GETBLOCK(p,i) (((const uint4 *)p)[(int)i])

static inline uint4 fmix(uint4 h)
{
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;

	return h;
}

static inline gtm_uint8 fmix64(gtm_uint8 k)
{
	k ^= k >> 33;
	k *= BIG_CONSTANT(0xff51afd7ed558ccd);
	k ^= k >> 33;
	k *= BIG_CONSTANT(0xc4ceb9fe1a85ec53);
	k ^= k >> 33;

	return k;
}

#if !defined(BIGENDIAN)

/* Little Endian */

#define ADD_BYTES(STATEPTR, KEY, COUNT)										\
MBSTART {													\
	int			ab_count, ab_offset, ab_carry_bytes;						\
	const unsigned char	*ab_key, *ab_ptr;								\
	gtm_uint8		c1, c2;										\
														\
	assert((STATEPTR)->carry_bytes + (COUNT) >= 0);								\
	assert((STATEPTR)->carry_bytes + (COUNT) <= 16);							\
	ab_count = (COUNT);											\
	ab_carry_bytes = (STATEPTR)->carry_bytes;								\
	ab_offset = ab_carry_bytes + ab_count;									\
	ab_key = (KEY);												\
	ab_ptr = ab_key + ab_count - 1;										\
	c1 = (STATEPTR)->c.one;											\
	c2 = (STATEPTR)->c.two;											\
	assert((ab_carry_bytes != 0) || ((c1 == 0) && (c2 == 0)));						\
	for (; (ab_offset > 8) && (ab_ptr >= ab_key); ab_ptr--, ab_offset--)					\
		c2 |= LSHIFT_BYTES_SAFE((gtm_uint8)*ab_ptr, (ab_offset - 9));					\
	for (; ab_ptr >= ab_key; ab_ptr--, ab_offset--)								\
		c1 |= LSHIFT_BYTES_SAFE((gtm_uint8)*ab_ptr, (ab_offset - 1));					\
	assert(((ab_carry_bytes + ab_count) != 0) || ((c1 == 0) && (c2 == 0)));					\
	assert((ab_count != 0) || (((STATEPTR)->c.one == c1) && ((STATEPTR)->c.two == c2)));			\
	(STATEPTR)->c.one = c1;											\
	(STATEPTR)->c.two = c2;											\
	(STATEPTR)->carry_bytes = ab_carry_bytes + ab_count;							\
} MBEND

#define GET_BLOCK(STATEPTR, KEY, K1, K2)									\
MBSTART {													\
	int		gb_carry_bytes;										\
	gtm_uint8	gb_m1, gb_m2, gb_k1, gb_k2, gb_c1, gb_c2;						\
														\
	assert(((STATEPTR)->carry_bytes != 0) || (((STATEPTR)->c.one == 0) && ((STATEPTR)->c.two == 0)));	\
	assert((STATEPTR)->carry_bytes < 16);									\
	gb_carry_bytes = (STATEPTR)->carry_bytes;								\
	gb_m1 = ((gtm_uint8 *)(KEY))[0];									\
	gb_m2 = ((gtm_uint8 *)(KEY))[1];									\
	if (0 == gb_carry_bytes)										\
	{													\
		(K1) = gb_m1;											\
		(K2) = gb_m2;											\
	}													\
	else if (8 == gb_carry_bytes)										\
	{													\
		gb_k1 = (STATEPTR)->c.one;									\
		gb_k2 = gb_m1;											\
		gb_c1 = gb_m2; 											\
		/* gb_c2 = 0; */										\
		(K1) = gb_k1;											\
		(K2) = gb_k2;											\
		(STATEPTR)->c.one = gb_c1;									\
		assert(0 == (STATEPTR)->c.two);									\
	}													\
	else if (gb_carry_bytes < 8)										\
	{													\
		gb_k1 = (STATEPTR)->c.one | LSHIFT_BYTES_SAFE(gb_m1, gb_carry_bytes);				\
		gb_k2 = RSHIFT_BYTES_SAFE(gb_m1, (8 - gb_carry_bytes))				 		\
			| LSHIFT_BYTES_SAFE(gb_m2, gb_carry_bytes);						\
		gb_c1 = RSHIFT_BYTES_SAFE(gb_m2, (8 - gb_carry_bytes));						\
		/* gb_c2 = 0; */										\
		(K1) = gb_k1;											\
		(K2) = gb_k2;											\
		(STATEPTR)->c.one = gb_c1;									\
		assert(0 == (STATEPTR)->c.two);									\
	}													\
	else {													\
		gb_k1 = (STATEPTR)->c.one;									\
		gb_k2 = (STATEPTR)->c.two									\
			| LSHIFT_BYTES_SAFE(gb_m1, (gb_carry_bytes - 8));					\
		gb_c1 = RSHIFT_BYTES_SAFE(gb_m1, (16 - gb_carry_bytes))						\
			| LSHIFT_BYTES_SAFE(gb_m2, (gb_carry_bytes - 8));					\
		gb_c2 = RSHIFT_BYTES_SAFE(gb_m2, (16 - gb_carry_bytes));					\
		(K1) = gb_k1;											\
		(K2) = gb_k2;											\
		(STATEPTR)->c.one = gb_c1;									\
		(STATEPTR)->c.two = gb_c2;									\
	}													\
} MBEND

#else

/* Big Endian */

#define ADD_BYTES(STATEPTR, KEY, COUNT)										\
MBSTART {													\
	int			ab_count, ab_offset, ab_carry_bytes;						\
	const unsigned char	*ab_key, *ab_ptr;								\
	gtm_uint8		c1, c2;										\
														\
	assert((STATEPTR)->carry_bytes + (COUNT) >= 0);								\
	assert((STATEPTR)->carry_bytes + (COUNT) <= 16);							\
	ab_count = (COUNT);											\
	ab_carry_bytes = (STATEPTR)->carry_bytes;								\
	ab_offset = ab_carry_bytes + ab_count;									\
	ab_key = (KEY);												\
	ab_ptr = ab_key + ab_count - 1;										\
	c1 = (STATEPTR)->c.one;											\
	c2 = (STATEPTR)->c.two;											\
	assert((ab_carry_bytes != 0) || ((c1 == 0) && (c2 == 0)));						\
	for (; (ab_offset > 8) && (ab_ptr >= ab_key); ab_ptr--, ab_offset--)					\
		c2 |= LSHIFT_BYTES_SAFE((gtm_uint8)*ab_ptr, (16 - ab_offset));					\
	for (; ab_ptr >= ab_key; ab_ptr--, ab_offset--)								\
		c1 |= LSHIFT_BYTES_SAFE((gtm_uint8)*ab_ptr, (8 - ab_offset));					\
	assert(((ab_carry_bytes + ab_count) != 0) || ((c1 == 0) && (c2 == 0)));					\
	assert((ab_count != 0) || (((STATEPTR)->c.one == c1) && ((STATEPTR)->c.two == c2)));			\
	(STATEPTR)->c.one = c1;											\
	(STATEPTR)->c.two = c2;											\
	(STATEPTR)->carry_bytes = ab_carry_bytes + ab_count;							\
} MBEND

#define GET_BLOCK(STATEPTR, KEY, K1, K2)									\
MBSTART {													\
	int		gb_carry_bytes;										\
	gtm_uint8	gb_m1, gb_m2, gb_k1, gb_k2, gb_c1, gb_c2;						\
														\
	assert(((STATEPTR)->carry_bytes != 0) || (((STATEPTR)->c.one == 0) && ((STATEPTR)->c.two == 0)));	\
	gb_carry_bytes = (STATEPTR)->carry_bytes;								\
	gb_m1 = ((gtm_uint8 *)(KEY))[0];									\
	gb_m2 = ((gtm_uint8 *)(KEY))[1];									\
	if (0 == gb_carry_bytes)										\
	{													\
		(K1) = gb_m1;											\
		(K2) = gb_m2;											\
	}													\
	else if (8 == gb_carry_bytes)										\
	{													\
		gb_k1 = (STATEPTR)->c.one;									\
		gb_k2 = gb_m1;									 		\
		gb_c1 = gb_m2;											\
		/* gb_c2 = 0; */										\
		(K1) = gb_k1;											\
		(K2) = gb_k2;											\
		(STATEPTR)->c.one = gb_c1;									\
		assert(0 == (STATEPTR)->c.two);									\
	}													\
	else if ( gb_carry_bytes < 8)										\
	{													\
		gb_k1 = (STATEPTR)->c.one | RSHIFT_BYTES_SAFE(gb_m1, gb_carry_bytes);				\
		gb_k2 = LSHIFT_BYTES_SAFE(gb_m1, (8 - gb_carry_bytes))				 		\
			| RSHIFT_BYTES_SAFE(gb_m2, gb_carry_bytes);						\
		gb_c1 = LSHIFT_BYTES_SAFE(gb_m2, (8 - gb_carry_bytes));						\
		/* gb_c2 = 0; */										\
		(K1) = gb_k1;											\
		(K2) = gb_k2;											\
		(STATEPTR)->c.one = gb_c1;									\
		assert(0 == (STATEPTR)->c.two);									\
	}													\
	else {													\
		gb_k1 = (STATEPTR)->c.one;									\
		gb_k2 = (STATEPTR)->c.two									\
			| RSHIFT_BYTES_SAFE(gb_m1, (gb_carry_bytes - 8));					\
		gb_c1 = LSHIFT_BYTES_SAFE(gb_m1, (16 - gb_carry_bytes))						\
			| RSHIFT_BYTES_SAFE(gb_m2, (gb_carry_bytes - 8));					\
		gb_c2 = LSHIFT_BYTES_SAFE(gb_m2, (16 - gb_carry_bytes));					\
		(K1) = gb_k1;											\
		(K2) = gb_k2;											\
		(STATEPTR)->c.one = gb_c1;									\
		(STATEPTR)->c.two = gb_c2;									\
	}													\
} MBEND

#endif

#define INTEGRATE_K1(K1, H1)											\
MBSTART {													\
	(K1) *= C1;												\
	(K1) = ROTL64((K1), 31);										\
	(K1) *= C2;												\
	(H1) ^= (K1);												\
} MBEND

#define INTEGRATE_K2(K2, H2)											\
MBSTART {													\
	(K2) *= C2;												\
	(K2) = ROTL64((K2), 33);										\
	(K2) *= C1;												\
	(H2) ^= (K2);												\
} MBEND

#define PROCESS_BLOCK(K1, K2, H1, H2)										\
MBSTART {													\
	UI64_TO_LE(K1);												\
	UI64_TO_LE(K2);												\
	INTEGRATE_K1(K1, H1);											\
	(H1) = ROTL64((H1), 27);										\
	(H1) += (H2);												\
	(H1) = (H1) * 5 + M1;											\
	INTEGRATE_K2(K2, H2);											\
	(H2) = ROTL64((H2), 31);										\
	(H2) += (H1);												\
	(H2) = (H2) * 5 + M2;											\
} MBEND

#define PROCESS_BYTES(STATEPTR, KEY, LEN)									\
MBSTART {													\
	int pb_carry_fill = 0;											\
														\
	assert((LEN) < 16);											\
	if ((STATEPTR)->carry_bytes + (LEN) >= 16)								\
	{													\
		/* fill the carry field, process it as a block, then clear it */				\
		pb_carry_fill = 16 - (STATEPTR)->carry_bytes;							\
		ADD_BYTES((STATEPTR), (KEY), pb_carry_fill);							\
		assert((STATEPTR)->carry_bytes == 16);								\
		PROCESS_BLOCK((STATEPTR)->c.one, (STATEPTR)->c.two, (STATEPTR)->h.one, (STATEPTR)->h.two);	\
		(STATEPTR)->c.one = (STATEPTR)->c.two = 0;							\
		(STATEPTR)->carry_bytes = 0;									\
	}													\
	ADD_BYTES((STATEPTR), (KEY) + pb_carry_fill, (LEN) - pb_carry_fill);					\
} MBEND

#if (defined(__i386) || defined(__x86_64__) || defined(_AIX))
#	define UNALIGNED_SAFE	(1)
#else
#	define UNALIGNED_SAFE	(0)
#endif

/* Declare these here to generate non-inline versions */
void	gtmmrhash_32(const void *key, int len, uint4 seed, uint4 *out4);
void	gtmmrhash_128(const void *key, int len, uint4 seed, gtm_uint16 *out);
void	gtmmrhash_128_hex(const gtm_uint16 *hash, unsigned char *out);
void	gtmmrhash_128_bytes(const gtm_uint16 *hash, unsigned char *out);

void MurmurHash3_x86_32(const void *key, int len, uint4 seed, void *out)
{
	int			i;
	const unsigned char 	*tail;
	const uint4		*blocks;
	int			nblocks = len / 4;
	uint4			h1 = seed;
	uint4			c1 = 0xcc9e2d51;
	uint4			c2 = 0x1b873593;
	uint4			k1;
#	ifndef UNALIGNED_ACCESS_SUPPORTED
	static char		*buff;
	static int		bufflen;
#	endif

	# ifndef UNALIGNED_ACCESS_SUPPORTED
	/* Murmur3 hash works only on 4-byte aligned keys so align key to avoid unaligned access error
	* messages from an architecture that does not support it.
	*/
	if (len && (0 != ((UINTPTR_T)key % 4)))
	{	/* make buffer 4-byte aligned */
		if (bufflen < len)
		{
			if (NULL != buff)
			{
				assert(bufflen);
				free(buff);
			}
			bufflen = len * 2;
			buff = malloc(bufflen);
		}
		assert(bufflen >= len);
		memcpy(buff, key, len);
		key = (const unsigned char*)buff;
	}
	# endif
	blocks = (const uint4 *)((char *)key + nblocks * 4);
	for(i = -nblocks; i; i++)
	{
		k1 = GETBLOCK(blocks,i);

		k1 *= c1;
		k1 = ROTL32(k1,15);
		k1 *= c2;

		h1 ^= k1;
		h1 = ROTL32(h1,13);
		h1 = h1 * 5 + 0xe6546b64;
	}
	tail = (const unsigned char*)blocks;
	k1 = 0;

	/* The shifts below assume little endian, so the handling of the tail block is inconsistent with normal blocks
	* on big endian systems. However, we are already using this version and we don't want to change the resulting
	* hashes, so leave it alone.
	*/
	switch(len & 3)
	{
	case 3: k1 ^= tail[2] << 16;
	/* no break */
	case 2: k1 ^= tail[1] << 8;
	/* no break */
	case 1: k1 ^= tail[0];
		k1 *= c1;
		k1 = ROTL32(k1,15);
		k1 *= c2;
		h1 ^= k1;
	};

	h1 ^= len;
	h1 = fmix(h1);
	*(uint4*)out = h1;
}

int gtmmrhash_128_ingest(hash128_state_t *state, const void *key, int len)
{
	int			i;
	gtm_uint8		k1, k2;
	const unsigned char	*keyptr;

	if (0 == len)
		return 0;

	keyptr = key;
#	if !UNALIGNED_SAFE
	/* determine the number of bytes to consume to reach 64-bit (8 byte) alignment */
	i = (0x8 - ((UINTPTR_T)keyptr & 0x7)) & 0x7;
	if (i > len)
		i = len;
	if (i > 0)
	{
		PROCESS_BYTES(state, keyptr, i);
		keyptr += i;
		len -= i;
	}
#	endif
	for (; len >= 16; len -= 16, keyptr += 16)
	{
		GET_BLOCK(state, keyptr, k1, k2);
		PROCESS_BLOCK(k1, k2, state->h.one, state->h.two);
	}
	if (len > 0)
		PROCESS_BYTES(state, keyptr, len);

	return len;
}

void gtmmrhash_128_result(hash128_state_t *state, uint4 total_len, gtm_uint16 *out)
{
	gtm_uint8		k1, k2, h1, h2;

	k1 = state->c.one;
	k2 = state->c.two;
	h1 = state->h.one;
	h2 = state->h.two;

	UI64_TO_LE(k1);
	UI64_TO_LE(k2);
	if (state->carry_bytes != 0)
	{
		if (state->carry_bytes > 8)
		{
			INTEGRATE_K2(k2, h2);
		}
		INTEGRATE_K1(k1, h1);
	}

	h1 ^= total_len;
	h2 ^= total_len;
	h1 += h2;
	h2 += h1;
	h1 = fmix64(h1);
	h2 = fmix64(h2);
	h1 += h2;
	h2 += h1;

	out->one = h1;
	out->two = h2;
}

/*-----------------------------------------------------------------------------*/
