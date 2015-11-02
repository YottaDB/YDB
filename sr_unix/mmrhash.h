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

#ifdef GTM64
void MurmurHash3_x64_128 ( const void * key, int len, uint4 seed, void * out );
#endif

#endif

/*-----------------------------------------------------------------------------*/
