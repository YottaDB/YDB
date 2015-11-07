/****************************************************************
 *                                                              *
 *      Copyright 2011, 2014 Fidelity Information Services, Inc       *
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
 *-----------------------------------------------------------------------------*/

/* Note that all these want the key to be a multiple of N bits, where N is the
   last number in the function name. */

void MurmurHash3_x86_32  ( const void * key, int len, uint4 seed, void * out );

void MurmurHash3_x86_128 ( const void * key, int len, uint4 seed, void * out );

void MurmurHash3_x64_128 ( const void * key, int len, uint4 seed, void * out );

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

void gtmmrhash_128(const void *key, int len, uint4 seed, gtm_uint16 *out);
void gtmmrhash_32(const void *key, int len, uint4 seed, uint4 *out4);
void gtmmrhash_128_ingest(hash128_state_t *state, const void *key, int len);
void gtmmrhash_128_result(hash128_state_t *state, uint4 total_len, gtm_uint16 *out);

void gtmmrhash_128_hex(const gtm_uint16 *hash, unsigned char *out);
void gtmmrhash_128_bytes(const gtm_uint16 *hash, unsigned char *out);

#endif

/*-----------------------------------------------------------------------------*/
