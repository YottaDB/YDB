/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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

#ifdef UNICODE_SUPPORTED
#include "gtm_icu_api.h"	/* needed by *TYPEMASK* macros defined in gtm_utf8.h */
#include "gtm_utf8.h"
#endif

GBLDEF	char	codelist[] = PATM_CODELIST;

GBLREF	uint4		pat_allmaskbits;
GBLREF	uint4		*pattern_typemask;
GBLREF	boolean_t	gtm_utf8_mode;

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
	int			bit;
	int			letter;
	int			repcnt;
	int			bytelen, charlen, pbytelen, strbytelen;
	unsigned char		*strptr, *strtop, *strnext, *pstr, *ptop, *pnext;
	uint4			code, tempuint, patstream_len;
	uint4			*patptr;
	uint4			mbit;
	char			buf[CHAR_CLASSES];
	boolean_t		flags, pvalid, strvalid;
	UNICODE_ONLY(
	wint_t			utf8_codepoint;
	)

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
	if (!gtm_utf8_mode)
		charlen = str->str.len;
	UNICODE_ONLY(
	else
	{
		MV_FORCE_LEN(str); /* to set str.char_len if not already done; also issues BADCHAR error if appropriate */
		charlen = str->str.char_len;
	}
	)
	if (tempuint != charlen)
		return FALSE;
	patptr++;
	min = (int4 *)patptr;
	rtop = min + count; /* Note: the compiler generates: rtop = min + SIZEOF(int4) * count */

	/* attempt a match */
	strptr = (unsigned char *)str->str.addr;
	strtop = &strptr[str->str.len];
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
				bytelen = 0;
				for (bit = 0; bit < PAT_MAX_BITS; bit++)
				{
					mbit = (1 << bit);
					if ((mbit & code & PATM_LONGFLAGS) && !(mbit & pat_allmaskbits))
						buf[bytelen++] = codelist[patmaskseq(mbit)];
				}
				rts_error(VARLSTCNT(4) ERR_PATNOTFOUND, 2, bytelen, buf);
			}
			if (!gtm_utf8_mode)
			{
				for (repcnt = 0; repcnt < repeat; repcnt++)
				{
					if (!(code & pattern_typemask[*strptr++]))
						return FALSE;
				}
			}
			UNICODE_ONLY(
			else
			{
				for (repcnt = 0; repcnt < repeat; repcnt++)
				{
					assert(strptr < strtop);	/* PATTERN_TYPEMASK macro relies on this */
					if (!(code & PATTERN_TYPEMASK(strptr, strtop, strnext, utf8_codepoint)))
						return FALSE;
					strptr = strnext;
				}
			}
			)
		} else
		{	/* STRLIT pat atom */
			assert(3 == PAT_STRLIT_PADDING);
			GET_LONG(bytelen, patptr);	/* get bytelen */
			patptr++;
			GET_LONG(charlen, patptr);	/* get charlen */
			patptr++;
			GET_ULONG(flags, patptr);	/* get falgs */
			patptr++;
			assert(!(flags & PATM_STRLIT_BADCHAR));
			/* ensure pattern atom length is within limits of the complete pattern stream */
			assert((0 <= bytelen)
					&& ((patptr + DIVIDE_ROUND_UP(bytelen, SIZEOF(*patptr)))
						<= ((uint4 *)(pat->str.addr) + patstream_len + 2)));
			pstr = (unsigned char *)patptr;
			if (1 == bytelen)
			{
				if (!gtm_utf8_mode)
				{
					for (repcnt = 0; repcnt < repeat; repcnt++)
						if (*pstr != *strptr++)
							return FALSE;
					patptr++;
				}
				UNICODE_ONLY(
				else
				{
					for (repcnt = 0; repcnt < repeat; repcnt++)
					{
						if ((1 != (UTF8_VALID(strptr, strtop, bytelen), bytelen)) || (*pstr != *strptr++))
							return FALSE;
					}
					patptr++;
				}
				)
			} else if (bytelen > 0)
			{
				if (!gtm_utf8_mode)
				{
					for (repcnt = 0; repcnt < repeat; repcnt++)
						for (letter = 0, pstr = (unsigned char *)patptr; letter < bytelen; letter++)
							if (*pstr++ != *strptr++)
								return FALSE;
					patptr += DIVIDE_ROUND_UP(bytelen, SIZEOF(*patptr));
				}
				UNICODE_ONLY(
				else
				{
					pstr = (unsigned char *)patptr;
					ptop = pstr + bytelen;
					for (repcnt = 0; repcnt < repeat; repcnt++)
					{
						pstr = (unsigned char *)patptr;
						for ( ; pstr < ptop; )
						{
							pvalid = UTF8_VALID(pstr, ptop, pbytelen);	/* sets pbytelen */
							assert(pvalid);
							strvalid = UTF8_VALID(strptr, strtop, strbytelen); /* sets strbytelen */
							if (pbytelen != strbytelen)
								return FALSE;
							else
							{
								DEBUG_ONLY(strnext = strptr + pbytelen);
								pnext = pstr + pbytelen;
								do
								{
									if (*pstr++ != *strptr++)
										return FALSE;
								} while (pstr < pnext);
								assert(strptr == strnext);
							}
						}
					}
					patptr += DIVIDE_ROUND_UP(bytelen, SIZEOF(*patptr));
				}
				)
			}
		}
	}
	return TRUE;
}
