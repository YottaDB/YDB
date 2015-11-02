/****************************************************************
 *                                                              *
 *      Copyright 2011 Fidelity Information Services, Inc       *
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
 * Note - The x86 and x64 versions do _not_ produce the same results, as the
 * algorithms are optimized for their respective platforms. You can still
 * compile and run any of them on any platform, but your performance with the
 * non-native version will be less than optimal.
 *
 * This version converted to C for use in GT.M by FIS.
 *-----------------------------------------------------------------------------*/

#include "mdef.h"
#include "mmrhash.h"

/*
 * Since this code is largely third-party, its form is somewhat different than
 * other code in GT.M. This is intentional, at least for the initial version,
 * in order to allow for comparison with the original. This may be cleaned up
 * in future versions, as no revisions to the original public domain code are
 * expected. The original code is hosted at http://code.google.com/p/smhasher/ .
 *
 * Note also that GT.M is currently only using the 32 bit hash function,
 * MurmurHash3_x86_32(). The 128 bit functions are retained for completeness
 * and possible future use.
 *
 */

#if 0
#define	FORCE_INLINE __attribute__((always_inline))
#else
#define	FORCE_INLINE
#endif


static uint4 rotl32 ( uint4 x, char r );
static uint4 fmix ( uint4 h );

#ifdef GTM64
static gtm_uint8 rotl64 ( gtm_uint8 x, char r );
static gtm_uint8 fmix64 ( gtm_uint8 k );
#endif

static uint4 rotl32 ( uint4 x, char r )
{
  return (x << r) | (x >> (32 - r));
}

#ifdef GTM64
static gtm_uint8 rotl64 ( gtm_uint8 x, char r )
{
  return (x << r) | (x >> (64 - r));
}
#endif

#define	ROTL32(x,y)	rotl32(x,y)
#define ROTL64(x,y)	rotl64(x,y)

#define BIG_CONSTANT(x) (x##LLU)

#define GETBLOCK(p,i) (((const uint4 *)p)[(int)i])

#ifdef GTM64
#define GETBLOCK64(p,i) (((const gtm_uint8 *)p)[(int)i])
#endif

/*-----------------------------------------------------------------------------
 * Finalization mix - force all bits of a hash block to avalanche            */

static FORCE_INLINE uint4 fmix ( uint4 h )
{
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;

  return h;
}

/*----------*/

#ifdef GTM64
static FORCE_INLINE gtm_uint8 fmix64 ( gtm_uint8 k )
{
  k ^= k >> 33;
  k *= BIG_CONSTANT(0xff51afd7ed558ccd);
  k ^= k >> 33;
  k *= BIG_CONSTANT(0xc4ceb9fe1a85ec53);
  k ^= k >> 33;

  return k;
}
#endif

/*-----------------------------------------------------------------------------*/

void MurmurHash3_x86_32 ( const void * key, int len,
                          uint4 seed, void * out )
{
  int i;
  const unsigned char * data = (const unsigned char*)key;
  const unsigned char * tail;
  const uint4 * blocks;
  const int nblocks = len / 4;

  uint4 h1 = seed;

  uint4 c1 = 0xcc9e2d51;
  uint4 c2 = 0x1b873593;
  uint4 k1;

  /*----------
   * body   */

  blocks = (const uint4 *)(data + nblocks*4);

  for(i = -nblocks; i; i++)
  {
    k1 = GETBLOCK(blocks,i);

    k1 *= c1;
    k1 = ROTL32(k1,15);
    k1 *= c2;

    h1 ^= k1;
    h1 = ROTL32(h1,13);
    h1 = h1*5+0xe6546b64;
  }

  /*----------
   * tail   */

  tail = (const unsigned char*)(data + nblocks*4);

  k1 = 0;

  switch(len & 3)
  {
  case 3: k1 ^= tail[2] << 16;
  case 2: k1 ^= tail[1] << 8;
  case 1: k1 ^= tail[0];
          k1 *= c1; k1 = ROTL32(k1,15); k1 *= c2; h1 ^= k1;
  };

  /*----------
   * finalization  */

  h1 ^= len;

  h1 = fmix(h1);

  *(uint4*)out = h1;
}

/*-----------------------------------------------------------------------------*/

void MurmurHash3_x86_128 ( const void * key, const int len,
                           uint4 seed, void * out )
{
  int i;
  const unsigned char * data = (const unsigned char*)key;
  const unsigned char * tail;
  const int nblocks = len / 16;

  uint4 h1 = seed;
  uint4 h2 = seed;
  uint4 h3 = seed;
  uint4 h4 = seed;

  uint4 c1 = 0x239b961b;
  uint4 c2 = 0xab0e9789;
  uint4 c3 = 0x38b34ae5;
  uint4 c4 = 0xa1e38b93;

  uint4 k1;
  uint4 k2;
  uint4 k3;
  uint4 k4;

  /*----------
   * body   */

  const uint4 * blocks = (const uint4 *)(data + nblocks*16);

  for(i = -nblocks; i; i++)
  {
    k1 = GETBLOCK(blocks,i*4+0);
    k2 = GETBLOCK(blocks,i*4+1);
    k3 = GETBLOCK(blocks,i*4+2);
    k4 = GETBLOCK(blocks,i*4+3);

    k1 *= c1; k1  = ROTL32(k1,15); k1 *= c2; h1 ^= k1;

    h1 = ROTL32(h1,19); h1 += h2; h1 = h1*5+0x561ccd1b;

    k2 *= c2; k2  = ROTL32(k2,16); k2 *= c3; h2 ^= k2;

    h2 = ROTL32(h2,17); h2 += h3; h2 = h2*5+0x0bcaa747;

    k3 *= c3; k3  = ROTL32(k3,17); k3 *= c4; h3 ^= k3;

    h3 = ROTL32(h3,15); h3 += h4; h3 = h3*5+0x96cd1c35;

    k4 *= c4; k4  = ROTL32(k4,18); k4 *= c1; h4 ^= k4;

    h4 = ROTL32(h4,13); h4 += h1; h4 = h4*5+0x32ac3b17;
  }

  /*----------
   * tail   */

  tail = (const unsigned char*)(data + nblocks*16);

  k1 = 0;
  k2 = 0;
  k3 = 0;
  k4 = 0;

  switch(len & 15)
  {
  case 15: k4 ^= tail[14] << 16;
  case 14: k4 ^= tail[13] << 8;
  case 13: k4 ^= tail[12] << 0;
           k4 *= c4; k4  = ROTL32(k4,18); k4 *= c1; h4 ^= k4;

  case 12: k3 ^= tail[11] << 24;
  case 11: k3 ^= tail[10] << 16;
  case 10: k3 ^= tail[ 9] << 8;
  case  9: k3 ^= tail[ 8] << 0;
           k3 *= c3; k3  = ROTL32(k3,17); k3 *= c4; h3 ^= k3;

  case  8: k2 ^= tail[ 7] << 24;
  case  7: k2 ^= tail[ 6] << 16;
  case  6: k2 ^= tail[ 5] << 8;
  case  5: k2 ^= tail[ 4] << 0;
           k2 *= c2; k2  = ROTL32(k2,16); k2 *= c3; h2 ^= k2;

  case  4: k1 ^= tail[ 3] << 24;
  case  3: k1 ^= tail[ 2] << 16;
  case  2: k1 ^= tail[ 1] << 8;
  case  1: k1 ^= tail[ 0] << 0;
           k1 *= c1; k1  = ROTL32(k1,15); k1 *= c2; h1 ^= k1;
  };

  /*----------
   * finalization  */

  h1 ^= len; h2 ^= len; h3 ^= len; h4 ^= len;

  h1 += h2; h1 += h3; h1 += h4;
  h2 += h1; h3 += h1; h4 += h1;

  h1 = fmix(h1);
  h2 = fmix(h2);
  h3 = fmix(h3);
  h4 = fmix(h4);

  h1 += h2; h1 += h3; h1 += h4;
  h2 += h1; h3 += h1; h4 += h1;

  ((uint4*)out)[0] = h1;
  ((uint4*)out)[1] = h2;
  ((uint4*)out)[2] = h3;
  ((uint4*)out)[3] = h4;
}

/*-----------------------------------------------------------------------------*/

#ifdef GTM64
void MurmurHash3_x64_128 ( const void * key, const int len,
                           const uint4 seed, void * out )
{
  int i;
  const unsigned char * data = (const unsigned char*)key;
  const unsigned char * tail;
  const int nblocks = len / 16;

  gtm_uint8 h1 = seed;
  gtm_uint8 h2 = seed;

  gtm_uint8 c1 = BIG_CONSTANT(0x87c37b91114253d5);
  gtm_uint8 c2 = BIG_CONSTANT(0x4cf5ad432745937f);

  gtm_uint8 k1;
  gtm_uint8 k2;

  /*----------
   * body   */

  const gtm_uint8 * blocks = (const gtm_uint8 *)(data);

  for(i = 0; i < nblocks; i++)
  {
    k1 = GETBLOCK64(blocks,i*2+0);
    k2 = GETBLOCK64(blocks,i*2+1);

    k1 *= c1; k1  = ROTL64(k1,31); k1 *= c2; h1 ^= k1;

    h1 = ROTL64(h1,27); h1 += h2; h1 = h1*5+0x52dce729;

    k2 *= c2; k2  = ROTL64(k2,33); k2 *= c1; h2 ^= k2;

    h2 = ROTL64(h2,31); h2 += h1; h2 = h2*5+0x38495ab5;
  }

  /*----------
   * tail   */

  tail = (const unsigned char*)(data + nblocks*16);

  k1 = 0;
  k2 = 0;

  switch(len & 15)
  {
  case 15: k2 ^= ((gtm_uint8)tail[14]) << 48;
  case 14: k2 ^= ((gtm_uint8)tail[13]) << 40;
  case 13: k2 ^= ((gtm_uint8)tail[12]) << 32;
  case 12: k2 ^= ((gtm_uint8)tail[11]) << 24;
  case 11: k2 ^= ((gtm_uint8)tail[10]) << 16;
  case 10: k2 ^= ((gtm_uint8)tail[ 9]) << 8;
  case  9: k2 ^= ((gtm_uint8)tail[ 8]) << 0;
           k2 *= c2; k2  = ROTL64(k2,33); k2 *= c1; h2 ^= k2;

  case  8: k1 ^= ((gtm_uint8)tail[ 7]) << 56;
  case  7: k1 ^= ((gtm_uint8)tail[ 6]) << 48;
  case  6: k1 ^= ((gtm_uint8)tail[ 5]) << 40;
  case  5: k1 ^= ((gtm_uint8)tail[ 4]) << 32;
  case  4: k1 ^= ((gtm_uint8)tail[ 3]) << 24;
  case  3: k1 ^= ((gtm_uint8)tail[ 2]) << 16;
  case  2: k1 ^= ((gtm_uint8)tail[ 1]) << 8;
  case  1: k1 ^= ((gtm_uint8)tail[ 0]) << 0;
           k1 *= c1; k1  = ROTL64(k1,31); k1 *= c2; h1 ^= k1;
  };

  /*----------
   * finalization  */

  h1 ^= len; h2 ^= len;

  h1 += h2;
  h2 += h1;

  h1 = fmix64(h1);
  h2 = fmix64(h2);

  h1 += h2;
  h2 += h1;

  ((gtm_uint8*)out)[0] = h1;
  ((gtm_uint8*)out)[1] = h2;
}
#endif

/*-----------------------------------------------------------------------------*/

