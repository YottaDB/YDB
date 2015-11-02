/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "matchc.h"


/*
 * -----------------------------------------------
 * Pseudo equivalent of VAX matchc instruction
 *
 * Arguments:
 *	del_len	- delimiter length
 *	del_str - pointer to delimiter string
 *	src_len	- length of source string
 *	src_str	- pointer to source string
 *	res	- pointer to the result
 *
 * Return:
 *	pointer to next character after match substring
 *	in the source string, if found.  Otherwise src_str + src_len.
 *
 * Side effects:
 *	set res arg to:
 *		0 		- if match not found
 *		1 + char_len 	- if match found, where char_len is the position
 *				  of the next character after the match substring.
 * -----------------------------------------------
 */
#ifdef UNICODE_SUPPORTED
#include "gtm_utf8.h"

GBLREF	boolean_t	gtm_utf8_mode;
GBLREF	boolean_t	badchar_inhibit;

/* multi-byte character-oriented substring matching */
unsigned char *matchc(int del_len, unsigned char *del_str, int src_len, unsigned char *src_str, int *res)
{
	unsigned char 	*srcptr, *srctop, *srcnext, *delptr, *deltop, *delnext, *delnext1, *restart_ptr;
	wint_t		srccp, delcp, delcp1;	/* code points for the source and delimiter characters */
	int		char_len, restart_char_len, delcharlen, bytelen;

	if (!gtm_utf8_mode)
		return matchb(del_len, del_str, src_len, src_str, res);
	if (del_len == 0)
	{	/* always matches a null string */
		*res = 1;
		return src_str;
	}
	srcptr = src_str;
	srctop = src_str + src_len;
	deltop = del_str + del_len;
	/* Check UTF8 byte sequence validity of delimiter string. The following code is very similar to utf8_len() but
	 * we dont invoke the function here for performance reasons as this piece of code is used by heavy hitters like $piece.
	 * Also, the code below can be forked off into two cases depending on the value of "badchar_inhibit". This is a
	 * performance enhancement that can be done later if this is found to be a bottleneck.
	 */
	if (!badchar_inhibit)
	{
		for (delcharlen = 0, delptr = del_str; delptr < deltop; delcharlen++, delptr += bytelen)
		{
			if (!UTF8_VALID(delptr, deltop, bytelen))
				utf8_badchar(0, delptr, deltop, 0, NULL);
		}
	}
	/* compute the code point of the 1st delimiter char */
	delnext1 = UTF8_MBTOWC(del_str, deltop, delcp1);
	assert((WEOF != delcp1) || badchar_inhibit);
	for (char_len = 0; (srcptr < srctop) && (srctop - srcptr) >= del_len; )
	{
		srcnext = srcptr;
		do
		{	/* find the occurence of 1st delimiter char in the source */
			srcptr = srcnext;
			srcnext = UTF8_MBTOWC(srcptr, srctop, srccp);
			if (srccp == WEOF && !badchar_inhibit)
				utf8_badchar(0, srcptr, srctop, 0, NULL);
			++char_len; /* maintain the source character position */
		} while ((srcnext < srctop) && ((srccp != delcp1) || ((srccp == WEOF) && (*srcptr != *del_str))));

		if ((srccp != delcp1) || (srccp == WEOF) && (*srcptr != *del_str))
		{	/* could not find the 1st delimiter char in the source */
			*res = 0;
			return srctop;
		}
		/* 1st delimiter character match found. match the other delimiter characters */
		delptr = delnext1; 		/* advance past the 1st delimiter character */
		restart_ptr = srcptr = srcnext; /* advance past the 1st source character */
		restart_char_len = char_len;
		for ( ; (srcptr < srctop) && (delptr < deltop); srcptr = srcnext, delptr = delnext, ++char_len)
		{
			srcnext = UTF8_MBTOWC(srcptr, srctop, srccp);
			if ((srccp == WEOF) && !badchar_inhibit)
				utf8_badchar(0, srcptr, srctop, 0, NULL);
			delnext = UTF8_MBTOWC(delptr, deltop, delcp);
			if ((srccp != delcp) || (srccp == WEOF && *srcptr != *delptr))
			{	/* match lost. restart the search skipping the first delimiter character */
				srcptr = restart_ptr;
				char_len = restart_char_len;
				break;
			}
		}
		if (delptr >= deltop)
		{	/* match found */
			assert(deltop == delptr);
			*res = 1 + char_len;
			return srcptr;
		}
	}
	*res = 0;
	return srctop;
}
#endif /* UNICODE_SUPPORTED */

/* byte-oriented substring matching */
unsigned char *matchb(int del_len, unsigned char *del_str, int src_len, unsigned char *src_str, int *res)
{
	unsigned char 	*psrc, *pdel, *psrc_base;
	int 		src_cnt, del_cnt;
	int 		tmp_src_cnt;

	psrc = psrc_base = src_str;
	pdel = del_str;

	src_cnt = src_len;
	del_cnt = del_len;

	if (del_cnt == 0)
		goto found_match;

	if (src_cnt < del_cnt)
	{
		psrc += src_len;
		goto nomatch;
	}

	while(src_cnt > 0)
	{	/* Quick Find 1st delimiter char */
		while(*psrc != *pdel)
		{
			psrc = ++src_str;
			if (0 >= --src_cnt)
				goto nomatch;
		}

		tmp_src_cnt = src_cnt;

		/* Found delimiter */
		while(*psrc++ == *pdel++)
		{	if (0 >= --del_cnt)
				goto found_match;
			if (0 >= --tmp_src_cnt)
				goto nomatch;
		}

		/* Match lost, goto next source character */
		psrc = ++src_str;
		src_cnt--;
		pdel = del_str;
		del_cnt = del_len;
	}
nomatch:
	*res = 0;
	return (psrc);

found_match:
	*res = 1 + (psrc - psrc_base);
	return (psrc);
}
