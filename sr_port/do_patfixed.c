/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "patcode.h"
#include "copy.h"

GBLREF	uint4	pat_allmaskbits;
GBLREF	uint4	*pattern_typemask;
GBLDEF	char	codelist[] = PATM_CODELIST;

/* This procedure executes at "run-time".  After a pattern in a MUMPS program has been compiled (by patstr and its
 *  	helper-procedures), this procedure is called to evaluate "fixed-length" patterns.
 * 	i.e. for each pattern atom, the lower-bound is equal to the upper-bound such as 3N2A5N.
 * For patterns with a variable length, procedure do_pattern() is called to do the evaluation.
 */

int do_patfixed(mval *str, mval *pat)
{
	int4			count, tempint;
	int4			*min, *reptr, *rtop;
	int4			repeat;
	int4			*ptop;
	int			bit;
	int			letter;
	int			repcnt;
	int			len;
	unsigned char		*strptr, *pstr;
	uint4			code, tempuint, patstream_len;
	uint4			*patptr;
	uint4			mbit;
	char			buf[CHAR_CLASSES];

	error_def(ERR_PATNOTFOUND);

	/* set up information */
	MV_FORCE_STR(str);
	patptr =  (uint4 *)pat->str.addr;
	DEBUG_ONLY(
		GET_ULONG(tempuint, patptr);
		assert(tempuint);	/* ensure first uint4 is non-zero indicating fixed length pattern string */
	)
	patptr++;
	GET_ULONG(tempuint, patptr);
	DEBUG_ONLY(patstream_len = tempuint);
	patptr += tempuint;
	GET_LONG(count, patptr);
	assert(MAX_PATTERN_ATOMS > count);
	patptr++;
	GET_ULONG(tempuint, patptr);
	patptr++;
	if (tempuint != str->str.len)
		return FALSE;
	patptr++;
	min = (int4 *)patptr;
	rtop = min + count; /* Note: the compiler generates: rtop = min + sizeof(int4) * count */

	/* attempt a match */
	strptr = (unsigned char *)str->str.addr;
	patptr = (uint4 *)pat->str.addr;
	patptr += 2;
	for (reptr = min; reptr < rtop ; reptr++)
	{
		GET_LONG(repeat, reptr);
		GET_ULONG(code, patptr);
		assert(code);
		patptr++;
		if (!(code & PATM_STRLIT))
		{	/* meta character pat atom */
			if (!(code & pat_allmaskbits))
			{	/* current table has no characters with this pattern code */
				len = 0;
				for (bit = 0; bit < 32; bit++)
				{
					mbit = (1 << bit);
					if ((mbit & code & PATM_LONGFLAGS) && !(mbit & pat_allmaskbits))
						buf[len++] = codelist[patmaskseq(mbit)];
				}
				rts_error(VARLSTCNT(4) ERR_PATNOTFOUND, 2, len, buf);
			}
			for (repcnt = 0; repcnt < repeat; repcnt++)
			{
				if (!(code & pattern_typemask[*strptr++]))
					return FALSE;
			}
		} else
		{	/* STRLIT pat atom */
			GET_LONG(len, patptr);
			patptr++;
			/* ensure pattern atom length is within limits of the complete pattern stream */
			assert((0 <= len)
					&& ((patptr + DIVIDE_ROUND_UP(len, sizeof(*patptr)))
						<= ((uint4 *)(pat->str.addr) + patstream_len + 2)));
			if (1 == len)
			{
				pstr = (unsigned char *)patptr;
				for (repcnt = 0; repcnt < repeat; repcnt++)
					if (*pstr != *strptr++)
						return FALSE;
				patptr++;
			} else if (len > 0)
			{
				for (repcnt = 0; repcnt < repeat; repcnt++)
					for (letter = 0, pstr = (unsigned char *)patptr; letter < len; letter++)
						if (*pstr++ != *strptr++)
							return FALSE;
				patptr += DIVIDE_ROUND_UP(len, sizeof(*patptr));
			}
		}
	}
	return TRUE;
}
