/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

#include "mdef.h"

#include "matchc.h"
#include "min_max.h"

#define	RETURN_NOMATCH			\
{					\
	*res = 0;			\
	assert(0 < numpcs_unmatched);	\
	*numpcs = numpcs_unmatched;	\
	return src_top;			\
}

#define	RETURN_YESMATCH(RET)		\
{					\
	*res = RET;			\
	assert(0 == numpcs_unmatched);	\
	*numpcs = 0;			\
	return src_ptr;			\
}

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
 *	numpcs  - pointer to the number of pieces that are desired to be matched.
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
 *	set numpcs arg to # of pieces that could not be matched (because end of source string was reached before then)
 * -----------------------------------------------
 */
#ifdef UTF8_SUPPORTED
#include "gtm_utf8.h"

GBLREF	boolean_t	gtm_utf8_mode;
GBLREF	boolean_t	badchar_inhibit;

static inline unsigned char *matchc_ascii_del(int del_len, unsigned char *del_str, int src_len, unsigned char *src_str,
						int *res, int *numpcs, boolean_t piece) __attribute__ ((always_inline));

/* matchc() for a delimiter of only ASCII characters (the caller has checked this, and that "del_len" is not 0). Does what
 * the general code in matchc() does, including which characters of the source are checked for BADCHAR, but faster.
 * An ASCII byte in the source is always one character and no other character has an ASCII byte, so this function
 * searches for the 1st delimiter byte, compares the other delimiter bytes, and finds the length of only the non-ASCII
 * characters that the search goes past.
 */
static inline unsigned char *matchc_ascii_del(int del_len, unsigned char *del_str, int src_len, unsigned char *src_str,
						int *res, int *numpcs, boolean_t piece)
{
	unsigned char 	*src_ptr, *src_top, *restart_ptr, del1;
	int		mb_extra, bytelen, numpcs_unmatched, cmplen, i;

	numpcs_unmatched = *numpcs;	/* note down # of pieces left to match */
	src_ptr = src_str;
	src_top = src_str + src_len;
	del1 = *del_str;
	/* "mb_extra" is the number of bytes after the 1st byte of each multi-byte character before "src_ptr",
	 * so "(src_ptr - src_str) - mb_extra" is the number of characters before "src_ptr".
	 */
	for (mb_extra = 0; (src_ptr < src_top) && (src_top - src_ptr) >= del_len; )
	{	/* find the occurrence of 1st delimiter char in the source */
		for ( ; ; )
		{	/* Go past ASCII characters that are not the 1st delimiter char. For a $PIECE() ("piece"), whose
			 * delimiters are often a few bytes apart, check one byte at a time, as the check of 8 bytes at a
			 * time costs more than it saves for such short distances. Otherwise use match_find_byte().
			 */
			if (piece)
			{
				for ( ; (src_ptr < src_top) && (*src_ptr != del1) && (ASCII_MAX >= *src_ptr); src_ptr++)
					;
			} else
				src_ptr = match_find_byte(src_ptr, src_top, del1, TRUE, FALSE);
			if (src_ptr == src_top)
			{	/* could not find the 1st delimiter char in the source */
				RETURN_NOMATCH;
			}
			if (*src_ptr == del1)
				break;
			/* A non-ASCII character. Go past it and any non-ASCII characters that follow it here, as a string of
			 * multi-byte characters, with no ASCII between them, would otherwise call match_find_byte() for each.
			 */
			do
			{
				if (!UTF8_VALID_NONASCII(*src_ptr, src_ptr, src_top, bytelen) && !badchar_inhibit)
					utf8_badchar(0, src_ptr, src_top, 0, NULL);
				src_ptr += bytelen;
				mb_extra += bytelen - 1;
			} while ((src_ptr < src_top) && (ASCII_MAX < *src_ptr));
		}
		/* 1st delimiter character match found. match the other delimiter characters */
		assert((src_ptr < src_top) && (*src_ptr == del1));
		restart_ptr = ++src_ptr;	/* advance past the 1st source character */
		cmplen = MIN(del_len - 1, src_top - src_ptr);
		for (i = 0; (i < cmplen) && (src_ptr[i] == del_str[1 + i]); i++)
			;
		if ((del_len - 1) == i)
		{	/* Match found : Return success if no more pieces to match else continue with scan */
			src_ptr += i;
			assert(0 < numpcs_unmatched);
			if (0 == --numpcs_unmatched)
				RETURN_YESMATCH(1 + (int)(src_ptr - src_str) - mb_extra);
		} else if (i < cmplen)
		{	/* match lost at a source character. Check that character for BADCHAR, as the general code in
			 * matchc() does, and restart the search skipping the first delimiter character.
			 */
			if ((ASCII_MAX < src_ptr[i]) && !badchar_inhibit && !UTF8_VALID(&src_ptr[i], src_top, bytelen))
				utf8_badchar(0, &src_ptr[i], src_top, 0, NULL);
			src_ptr = restart_ptr;
		} else	/* the source ended before the delimiter did */
			src_ptr = src_top;
	}
	RETURN_NOMATCH;
}

/* multi-byte character-oriented substring matching */
unsigned char *matchc(int del_len, unsigned char *del_str, int src_len, unsigned char *src_str, int *res, int *numpcs)
{
	unsigned char 	*src_ptr, *src_top, *src_next, *del_ptr, *del_top, *del_next, *del_next1, *restart_ptr;
	wint_t		src_cp, del_cp, del_cp1;	/* code points for the source and delimiter characters */
	int		char_len, restart_char_len, del_charlen, bytelen, numpcs_unmatched;

	if (!gtm_utf8_mode)
	{	/* In M mode, only the contains operator calls matchc(): op_fnpiece() and op_setpiece() run only in UTF-8 mode, and
		 * op_fnfind() calls matchc() only in UTF-8 mode. So use matchb_find(), as a contains search is like a $FIND().
		 */
		return matchb_find(del_len, del_str, src_len, src_str, res, numpcs);
	}
	assert(0 <= del_len);
	assert(0 < *numpcs);
	if (0 == del_len)
	{	/* always matches a null string */
		*numpcs = 0;
		*res = 1;
		return src_str;
	}
	src_ptr = src_str;
	src_top = src_str + src_len;
	del_top = del_str + del_len;
	if (del_len == utf8_ascii_len(del_str, del_top))
	{	/* More than one piece to find is a $PIECE() (op_fnpiece() and op_setpiece()). $FIND() and the contains
		 * operator find one. matchc_ascii_del() is inlined twice, with a constant "piece", so that each loop
		 * is compiled for its case.
		 */
		if (1 < *numpcs)
			return matchc_ascii_del(del_len, del_str, src_len, src_str, res, numpcs, TRUE);
		return matchc_ascii_del(del_len, del_str, src_len, src_str, res, numpcs, FALSE);
	}
	/* Check UTF8 byte sequence validity of delimiter string. The following code is very similar to utf8_len() but
	 * we dont invoke the function here for performance reasons as this piece of code is used by heavy hitters like $piece.
	 * Also, the code below can be forked off into two cases depending on the value of "badchar_inhibit". This is a
	 * performance enhancement that can be done later if this is found to be a bottleneck.
	 */
	if (!badchar_inhibit)
	{
		for (del_charlen = 0, del_ptr = del_str; del_ptr < del_top; del_charlen++, del_ptr += bytelen)
		{
			if (!UTF8_VALID(del_ptr, del_top, bytelen))
				utf8_badchar(0, del_ptr, del_top, 0, NULL);
		}
	}
	numpcs_unmatched = *numpcs;	/* note down # of pieces left to match */
	/* compute the code point of the 1st delimiter char */
	del_next1 = UTF8_MBTOWC(del_str, del_top, del_cp1);
	assert((WEOF != del_cp1) || badchar_inhibit);
	for (char_len = 0; (src_ptr < src_top) && (src_top - src_ptr) >= del_len; )
	{
		src_next = src_ptr;
		do
		{	/* find the occurrence of 1st delimiter char in the source */
			src_ptr = src_next;
			src_next = UTF8_MBTOWC(src_ptr, src_top, src_cp);
			if ((WEOF == src_cp) && !badchar_inhibit)
				utf8_badchar(0, src_ptr, src_top, 0, NULL);
			++char_len; /* maintain the source character position */
		} while ((src_next < src_top) && ((src_cp != del_cp1) || ((WEOF == src_cp) && (*src_ptr != *del_str))));

		if ((src_cp != del_cp1) || ((WEOF == src_cp) && (*src_ptr != *del_str)))
		{	/* could not find the 1st delimiter char in the source */
			RETURN_NOMATCH;
		}
		/* 1st delimiter character match found. match the other delimiter characters */
		del_ptr = del_next1; 		/* advance past the 1st delimiter character */
		restart_ptr = src_ptr = src_next; /* advance past the 1st source character */
		restart_char_len = char_len;
		for ( ; (src_ptr < src_top) && (del_ptr < del_top); src_ptr = src_next, del_ptr = del_next, ++char_len)
		{
			src_next = UTF8_MBTOWC(src_ptr, src_top, src_cp);
			if ((WEOF == src_cp) && !badchar_inhibit)
				utf8_badchar(0, src_ptr, src_top, 0, NULL);
			del_next = UTF8_MBTOWC(del_ptr, del_top, del_cp);
			if ((src_cp != del_cp) || ((WEOF == src_cp) && *src_ptr != *del_ptr))
			{	/* match lost. restart the search skipping the first delimiter character */
				src_ptr = restart_ptr;
				char_len = restart_char_len;
				break;
			}
		}
		if (del_ptr >= del_top)
		{	/* Match found : Return success if no more pieces to match else continue with scan */
			assert(del_top == del_ptr);
			assert(0 < numpcs_unmatched);
			if (0 == --numpcs_unmatched)
				RETURN_YESMATCH(1 + char_len);
		}
	}
	RETURN_NOMATCH;
}
#endif /* UTF8_SUPPORTED */

/* byte-oriented substring matching, the body of matchb() and matchb_find(), which inline it with a constant "find" */
static inline unsigned char *matchb_real(int del_len, unsigned char *del_str, int src_len, unsigned char *src_str, int *res,
						int *numpcs, boolean_t find) __attribute__ ((always_inline));
static inline unsigned char *matchb_real(int del_len, unsigned char *del_str, int src_len, unsigned char *src_str, int *res,
						int *numpcs, boolean_t find)
{
	unsigned char 	*src_ptr, *pdel, *src_base, *src_top, *del_top, del1;
	int 		src_cnt, numpcs_unmatched;
	boolean_t	match_found;

	assert(0 <= del_len);
	assert(0 < *numpcs);
	if (0 == del_len)
	{	/* always matches a null string */
		*numpcs = 0;
		*res = 1;
		return src_str;
	}
	numpcs_unmatched = *numpcs;	/* note down # of pieces to be matched */
	src_ptr = src_base = src_str;
	src_top = src_ptr + src_len;
	if (src_len < del_len)	/* Input string is shorter than delimiter string so no match possible */
		RETURN_NOMATCH;
	del_top = del_str + del_len;
	pdel = del_str;
	del1 = *del_str;
	while (src_ptr < src_top)
	{
		/* Quick Find 1st delimiter char. For $FIND() and the contains operator ("find"), check 8 bytes at a time
		 * with match_find_byte(), as the match can be far away. For $PIECE(), whose pieces are often a few bytes
		 * apart, and which (unlike $PIECE() with a single byte delimiter) has no cache of piece offsets, check one
		 * byte at a time, which is faster for such short distances.
		 */
		if (find)
		{
			src_ptr = src_str = match_find_byte(src_ptr, src_top, del1, FALSE, FALSE);
			if (src_ptr == src_top)
				RETURN_NOMATCH;
		} else
		{
			while (*src_ptr != *pdel)
			{
				src_ptr = ++src_str;
				if (src_ptr == src_top)
					RETURN_NOMATCH;
			}
		}
		match_found = FALSE;
		/* Found delimiter */
		while (*src_ptr++ == *pdel++)
		{
			if (pdel == del_top)
			{	/* Found matching piece. */
				match_found = TRUE;
				break;
			}
			if (src_ptr == src_top)
				RETURN_NOMATCH;
		}
		if (match_found)
		{	/* Return success if no more pieces to match else continue with scan */
			assert(0 < numpcs_unmatched);
			if (0 == --numpcs_unmatched)
				RETURN_YESMATCH(INTCAST(1 + (src_ptr - src_base)));
			src_str = src_ptr;
		} else
			src_ptr = ++src_str; /* Match lost, goto next source character */
		pdel = del_str;
	}
	RETURN_NOMATCH;
}

/* byte-oriented substring matching, for $PIECE() and SET $PIECE() */
unsigned char *matchb(int del_len, unsigned char *del_str, int src_len, unsigned char *src_str, int *res, int *numpcs)
{
	return matchb_real(del_len, del_str, src_len, src_str, res, numpcs, FALSE);
}

/* byte-oriented substring matching, for $FIND() and, through matchc() in M mode, the contains operator */
unsigned char *matchb_find(int del_len, unsigned char *del_str, int src_len, unsigned char *src_str, int *res, int *numpcs)
{
	/* For a source shorter than MATCH_FIND_MIN_LEN bytes, the setup of the check of 8 bytes at a time costs more
	 * than it saves, so check one byte at a time as matchb() does.
	 */
	if (MATCH_FIND_MIN_LEN > src_len)
		return matchb_real(del_len, del_str, src_len, src_str, res, numpcs, FALSE);
	return matchb_real(del_len, del_str, src_len, src_str, res, numpcs, TRUE);
}
