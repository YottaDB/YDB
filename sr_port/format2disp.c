/****************************************************************
 *								*
 *	Copyright 2013, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include "patcode.h"
#include "zshow.h"

#ifdef UNICODE_SUPPORTED
#include "gtm_icu_api.h"	/* U_ISPRINT() needs this header */
#include "gtm_utf8.h"
#endif

GBLREF	uint4		*pattern_typemask;
GBLREF	boolean_t	gtm_utf8_mode;

/* Routine to convert a string to displayable format. That is any nonprintable
 * characters are replaced by a dot (.) and the rest are used as is. No double-quote
 * additions happen like in format2zwr. Used currently by the trigger routines.
 * This function ensures the displayed part never goes more than "displen".
 * A "..." is inserted at the end if input string happens to be more than displen
 * can accommodate. The truncate length is returned in "displen" in that case.
 */
void	format2disp(char *src, int src_len, char *dispbuff, int *displen)
{
	char		*c, *c_top, *nextc, *dst, *dst_top, *disptop;
	int		chlen, dstlen, i;
	unsigned char	ch;
	uint4		codepoint;
	boolean_t	isctl, isill;

	dst = dispbuff;
	dstlen = *displen;
	if (0 > dstlen)
		dstlen = 0;
	disptop = dst + dstlen;
	assert(3 < dstlen);	/* we expect caller to ensure this */
	dstlen = (3 > dstlen)? 0 : ((dstlen < src_len) ? dstlen - 3 : dstlen); /* if neded adjust dstlen to account for ellipsis */
	dst_top = dst + dstlen;
	for (c = src, c_top = c + src_len; c < c_top; )
	{
		if (!gtm_utf8_mode)
		{
			ch = *c;
			isctl = (0 != (pattern_typemask[ch] & PATM_C));
			isill = FALSE;
			chlen = 1;
		}
#		ifdef UNICODE_SUPPORTED
		else {
			nextc = (char *)UTF8_MBTOWC(c, c_top, codepoint);
			isill = (WEOF == codepoint) ? (codepoint = *c, TRUE) : FALSE;
			isctl = (!isill ? !U_ISPRINT(codepoint) : TRUE);
			chlen = (int)(nextc - c);
		}
#		endif
		if ((dst + chlen) > dst_top)
			break;
		if (isctl)
		{	/* control character */
			for (i = 0; i < chlen; i++)
				*dst++ = '.';
			c = c + chlen;
		} else
		{	/* printable character (1 byte or > 1 byte) */
			for (i = 0; i < chlen; i++)
				*dst++ = *c++;
		}
	}
	/* Add "..." if applicable */
	if (c < c_top)
	{
		if (dst < disptop)
			*dst++ = '.';
		if (dst < disptop)
			*dst++ = '.';
		if (dst < disptop)
			*dst++ = '.';
	}
	*displen = dst - dispbuff;
}
