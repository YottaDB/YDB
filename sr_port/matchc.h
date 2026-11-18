/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MATCHC_INCLUDED
#define MATCHC_INCLUDED

#include "copy.h"

#define	MATCH_WORD_LOW_BITS	0x0101010101010101ULL	/* The low bit of each byte of an 8-byte word */
#define	MATCH_WORD_HIGH_BITS	0x8080808080808080ULL	/* The high bit of each byte of an 8-byte word */

/* Is not zero if a byte of WORD is zero */
#define	MATCH_WORD_ZERO_BYTES(WORD)	(((WORD) - MATCH_WORD_LOW_BITS) & ~(WORD) & MATCH_WORD_HIGH_BITS)

#define	MATCH_SINGLE_BYTES	8	/* Bytes match_find_byte() checks one at a time before it checks 8 at a time */
#define	MATCH_FIND_MIN_LEN	16	/* Shortest source for which matchb_find() checks 8 bytes at a time */

/* Returns a pointer to the first byte of the "ptr" to "top" byte range that is "byte" or, if "or_nonascii" is TRUE,
 * that is not ASCII. Returns "top" if there is no such byte. Checks 8 bytes at a time while at least 8 bytes remain.
 * If "single_first" is TRUE, first checks up to MATCH_SINGLE_BYTES bytes one at a time, as for the delimiters of a
 * $PIECE() with short pieces, where this is faster than setting up the check of 8 bytes at a time. That loop has one
 * bound, computed before it, so that it takes no longer per byte than a loop with only the "top" bound. Only
 * op_fnp1(), for a $PIECE() with a one byte delimiter in UTF-8 mode, does this. $FIND() and the contains operator,
 * whose match is often tens of bytes away, do not. "or_nonascii" and "single_first" are constants at each call, so
 * the compiler removes their checks from the loops.
 */
static inline unsigned char *match_find_byte(unsigned char *ptr, unsigned char *top, unsigned char byte,
						boolean_t or_nonascii, boolean_t single_first)
{
	unsigned char	*single_top;
	gtm_uint64_t	word, bytes, found;

	if (single_first)
	{
		single_top = ((top - ptr) > MATCH_SINGLE_BYTES) ? (ptr + MATCH_SINGLE_BYTES) : top;
		for ( ; (ptr < single_top) && (*ptr != byte) && (!or_nonascii || (ASCII_MAX >= *ptr)); ptr++)
			;
		if ((ptr < single_top) || (ptr == top))
			return ptr;	/* Found the byte, or checked them all */
	}
	bytes = MATCH_WORD_LOW_BITS * byte;	/* "byte" in each byte of the word */
	for ( ; SIZEOF(word) <= (top - ptr); ptr += SIZEOF(word))
	{
		GET_LLONG(word, ptr);
		found = word ^ bytes;		/* A byte that is "byte" is now zero */
		found = MATCH_WORD_ZERO_BYTES(found);
		if (or_nonascii)
			found |= (word & MATCH_WORD_HIGH_BITS);
		if (found)
			break;		/* The loop below finds the byte */
	}
	for ( ; (ptr < top) && (*ptr != byte) && (!or_nonascii || (ASCII_MAX >= *ptr)); ptr++)
		;
	return ptr;
}

/* Character oriented match call */
unsigned char *matchc(int del_len, unsigned char *del_str, int src_len, unsigned char *src_str, int *res, int *numpcs);
/* Byte oriented match call */
unsigned char *matchb(int del_len, unsigned char *del_str, int src_len, unsigned char *src_str, int *res, int *numpcs);
/* Byte oriented match call for $FIND() and the contains operator in M mode, which checks 8 bytes at a time for a
 * match that may be far away
 */
unsigned char *matchb_find(int del_len, unsigned char *del_str, int src_len, unsigned char *src_str, int *res, int *numpcs);

#endif /* MATCHC_INCLUDED */
