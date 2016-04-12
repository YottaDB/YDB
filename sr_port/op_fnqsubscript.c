/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "error.h"
#include "stringpool.h"
#include "op.h"
#include "is_canonic_name.h"
#include "gtm_ctype.h"

#ifdef UNICODE_SUPPORTED
#include "gtm_utf8.h"
GBLREF	boolean_t	badchar_inhibit;
error_def(ERR_INVDLRCVAL);
#endif

GBLREF spdesc stringpool;

#ifdef UNICODE_SUPPORTED
#define UTF_CHAR_CPY(FROM, TO, FROM_OFFSET, TO_OFFSET, UTF_LEN)			\
{										\
	unsigned int	temp_int;						\
	temp_int = UTF8_MBFOLLOW(&FROM->str.addr[--FROM_OFFSET]);		\
	assert((4 > temp_int) && (0 <= (int)temp_int));				\
	do									\
	{									\
		TO->str.addr[TO_OFFSET++] = FROM->str.addr[FROM_OFFSET++];	\
	} while (temp_int--);							\
	++UTF_LEN;								\
}
#else
#define UTF_CHAR_CPY(FROM, TO, FROM_OFFSET, TO_OFFSET, UTF_LEN)
#endif
/*
 * -----------------------------------------------
 * op_fnqsubscript()
 * MUMPS QSubscript function
 *
 * Arguments:
 *	src	- Pointer to Source Name string mval
 *      seq     - Sequence number of subscript to find
 *	dst	- Pointer to mval in which to save the subscript
 * Return:
 *	none
 * -----------------------------------------------
 */
void op_fnqsubscript(mval *src, int seq, mval *dst)
{
#ifdef UNICODE_SUPPORTED
	int		char_len = 0;
#endif
	int		ch_int;
	unsigned char	*cp;
	unsigned char	*end;
	boolean_t	instring;
	int		isrc;
	unsigned char	letter;
	int		odst;
	mval		srcmval;
	int		stop;
	int		subs_count;
	unsigned char	*temp_cp;

	error_def(ERR_NOSUBSCRIPT);
	error_def(ERR_NOCANONICNAME);

	if (seq < -1)	/* error "Cannot return subscript number ###" */
		rts_error(VARLSTCNT(3) ERR_NOSUBSCRIPT, 1, seq);
	subs_count = seq;
	if (!is_canonic_name(src, &subs_count, &isrc, &stop))
		rts_error(VARLSTCNT(4) ERR_NOCANONICNAME, 2, src->str.len, src->str.addr);
	/*  is_canonic_name has to parse it all anyway so it returns a start and stop for the compenent we want
	    because is_canonic_name has established src is of good form, we don't have to be paranoid in parsing
	*/
	assert((isrc >= 0) && (stop <= src->str.len) && (isrc <= stop));
	ENSURE_STP_FREE_SPACE(stop - isrc + 1);		/* Before we reference stingpool.free; + 1 for possible ^ */
	srcmval = *src;		/* Copy of source mval in case same as dst mval */
	src = &srcmval;
	dst->str.addr = (char *)stringpool.free;
	dst->mvtype = MV_STR;
	odst = 0;
	if (subs_count >= seq)
	{
		if ((0 == seq) && ('^' == src->str.addr[0]))
		{	/* add ^ here in case there's an intervening environment */
			dst->str.addr[odst++] = '^';
			UNICODE_ONLY(++char_len);
		}
		if ((0 == isrc) || ('"' == src->str.addr[isrc - 1])
			|| ((('"' != (letter = src->str.addr[isrc])) && ('$' != letter))))
		{	/* easy byte copy, at least if there's no multibyte characters */
			while (isrc < stop)
			{
				letter = src->str.addr[isrc++];
				UNICODE_ONLY(if (!gtm_utf8_mode || (0 == (0x80 & letter))))
				{
					dst->str.addr[odst++] = letter;
					if (('"' == letter) && ('"' == src->str.addr[isrc]))
						isrc++;  /* safe 'cause embedded quotes have at least 1 following char in src */
					UNICODE_ONLY(++char_len);
				}
#				ifdef UNICODE_SUPPORTED
				else
				{
					UTF_CHAR_CPY(src, dst, isrc, odst, char_len);
				}
#				endif
			}
		} else
		{	/* deal with $[z]char() */
			instring = FALSE;
			while (isrc < stop)
			{
				letter = src->str.addr[isrc++];
				if ('"' == letter)
				{	/* process one or more quotes */
					ch_int = odst;
					do
					{
						if (!(instring = !instring))
						{	/* keep quotes in quotes */
							dst->str.addr[odst++] = letter;
							UNICODE_ONLY(++char_len);
						}
						letter = src->str.addr[isrc++];
					} while (('"' == letter) && (isrc <= stop));
					if ((!instring) && (odst > ch_int))
					{	/* loose the closing quote */
						odst--;
						UNICODE_ONLY(char_len--);
					}
					if (isrc == stop)
					{
						assert(!instring);
						break;
					}
				}
				if (instring)
				{
					UNICODE_ONLY(if (!gtm_utf8_mode || (0 == (0x80 & letter))))
					{
						dst->str.addr[odst++] = letter;
						UNICODE_ONLY(++char_len);
					}
#					ifdef UNICODE_SUPPORTED
					else
					{
						UTF_CHAR_CPY(src, dst, isrc, odst, char_len);
					}
#					endif
				}
				else if (isrc < stop)
				{
#					ifdef UNICODE_SUPPORTED
					for ( ; '$' != letter; letter = src->str.addr[isrc++])
						;
					letter = src->str.addr[isrc++];
					if ('z' == letter)
						letter = 'Z';
#					endif
					while ('(' != src->str.addr[isrc++])
						;
					do
					{
						assert(isrc < stop);
						cp = (unsigned char*)&src->str.addr[isrc];
						assert(ISDIGIT_ASCII(*cp));
						for ( ; ISDIGIT_ASCII(src->str.addr[isrc]); isrc++)
							;
						end = (unsigned char*)&src->str.addr[isrc++];
						assert((',' == *end) || (')' == *end));
						A2I(cp, end, ch_int);
						UNICODE_ONLY(if (!gtm_utf8_mode || (0 == (0xFFFFFF80 & ch_int)) || ('Z' == letter)))
						{
							if (0 == (0xFFFFFF00 & ch_int))
							{
								dst->str.addr[odst++] = (char)ch_int;	/* byte copy */
								UNICODE_ONLY(++char_len);
							}
						}
#						ifdef UNICODE_SUPPORTED
						else
						{	/* multi-byte copy */
							cp = (unsigned char*)&dst->str.addr[odst];
							temp_cp = UTF8_WCTOMB((wint_t)ch_int, (char *)cp);
							assert((temp_cp >= cp) && (temp_cp - cp <= 4));
							if (temp_cp != cp)
								++char_len; /* update the UTF character length */
							else if (!badchar_inhibit)
								stx_error(ERR_INVDLRCVAL, 1, ch_int);
							odst += INTCAST(temp_cp - cp);
						}
#						endif
					} while (',' == *end);
					isrc++;
					assert(('"' == (letter = src->str.addr[isrc])) || ('$' == letter) || ((isrc - 1) == stop));
				}
			}
		}
	}
	dst->str.len = odst;
	stringpool.free += odst;
#	ifdef UNICODE_SUPPORTED
	assert((char_len <= odst) && (gtm_utf8_mode || (char_len == odst)));
	dst->str.char_len = char_len;
	dst->mvtype |= MV_UTF_LEN;
#	endif
	return;
}
