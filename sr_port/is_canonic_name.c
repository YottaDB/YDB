/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_ctype.h"
#include "is_canonic_name.h"

#ifdef	DEBUG
#include "subscript.h"
#endif

#ifdef UNICODE_SUPPORTED
#include "gtm_utf8.h"
GBLREF	boolean_t	badchar_inhibit;
error_def(ERR_BADCHAR);
#endif

/*
 * -----------------------------------------------
 * is_canonic_name()
 * validate a variable name
 *
 * Arguments:
 *	src	   - Pointer to Source Name string mval
 *	subscripts - Pointer to sequence number of subscript to find & return of subscript count
 *	start_off  - Pointer offset of the component requested by op_fnqsubscript
 *	stop_off   - Pointer offset of the end of the component requested by op_fnqsubscript
 * Return:
 *	boolean_t  - TRUE indicates good name; FALSE indicates defective
 * -----------------------------------------------
 */
boolean_t is_canonic_name(mval *src, int *subscripts, int *start_off, int *stop_off)
{	/* subscripts is overloaded - out to op_fnqlength, which doesn't use the last 2 arguments & in from op_fnqsubscript */
	char		term;
	int		envpart;
	boolean_t	instring;
	int		isrc;
	boolean_t	keep_quotes;
	char		letter;
	int		point;
	char		previous;
	int		seq;
	int		start;
	int		state;
	int		stop;
	int		subs_count;
	int		utf8_len;

	/* state:
	 *    0      before start of name
	 *    1      found ^ allow environment
	 *    2      dispatch for starting a component
	 *    3      in string
	 *    4      in number
	 *    5      expect first letter of name
	 *    6      expect next letter of name
	 *    7      in $CHAR()
	 *    8      at end of processing
	 */

	MV_FORCE_STR(src);
	seq = *subscripts;
	keep_quotes = FALSE;
	start = stop = 0;
	state = 0;
	subs_count = -1;
	for (isrc = 0; isrc < src->str.len; )
	{
		letter = src->str.addr[isrc];
		switch (state)
		{
			case 0:		/* start of name */
				if ('^' == letter)	/* before start of name */
				{
					state = 1;	/* check for environment */
					break;
				}
				if (('%' == letter) || ISALPHA_ASCII(letter))
				{
					if (0 == seq)
						start = isrc;
					state = 6;	/* rest of name */
					break;
				}
				return FALSE;
			case 1:		/* global name */
				if (('%' == letter) ||ISALPHA_ASCII(letter))	/* found ^ allow environment */
				{	/* found ^ allow environment */
					if (0 == seq)
						start = isrc;
					state = 6;	/* rest of name */
					break;
				}
				if (('|' == letter) || ('[' == letter))
				{
					term = (letter == '[') ? ']' : letter;
					envpart = 0;
					if (subs_count == seq)
						start = isrc + 1;
					state = 2;	/* process environment */
					break;
				}
				return FALSE;
			case 2:		 /* dispatch for starting a component */
				point = 0;
				instring = FALSE;
				if (envpart > 1)
					return FALSE;	/* too many environment components */
				if (')' == term)
					subs_count++;	/* new subscript */
				else
					envpart++;	/* next environment component */
				if ((subs_count == seq) && (0 == stop))
					start = isrc;
				if ('"' == letter)
				{
					if ((subs_count == seq) && (1 == envpart))
						start++;
					instring = TRUE;
					state = 3;	/* string */
					break;
				}
				if ('$' ==letter)
				{
					state = 7;	/* $[z]char() */
					break;
				}
				if ('0' == letter) /* Canonic number cannot start with 0 unless is single char */
				{
					if (++isrc < src->str.len)
						letter = src->str.addr[isrc];
					else
						return FALSE;	/* Cannot end with "0" */
					if (term == letter)
						state = (')' == term) ? 8 : 5;		/* end or name */
					else if (',' != letter)
						return FALSE;	/* Not a single char number */
					if ((subs_count == seq) && (0 == stop))
						stop = isrc;
					break;
				}
				if (('-' == letter) || ('.' == letter) || ISDIGIT_ASCII(letter))
				{
					if ('.' == letter)
						point++;
					previous = letter;
					state = 4;	/* numeric */
					break;
				}
				return FALSE;
			case 3:		/* [quoted] string */
				if ('"' == letter)	/* in string */
				{
					instring = !instring;
					if (instring)
						break;
					if (isrc + 1 >= src->str.len)
						return FALSE;
					if ('_' != src->str.addr[isrc + 1])
						break;
					isrc++;
					if (++isrc < src->str.len)
						letter = src->str.addr[isrc];
					else
						return FALSE;
					if ('$' != letter)
						return FALSE;
					state = 7;	/* $[z]char() */
					break;
				}
				if (!instring)
				{
					if (',' == letter)
						state = 2;	/* on to next */
					else if (term == letter)
						state = (')' == term) ? 8 : 5;	/* end or name */
					else
						return FALSE;
					if ((subs_count == seq) && (0 == stop))
						/* Not returning 2nd env part - maybe problem */
						stop = isrc - (keep_quotes ? 0 : 1);
				}
				break;
			case 4:		/* numeric */
				if (ISDIGIT_ASCII(letter))	/* in number */
				{
					if (('-' == previous) && ('0' == letter))
						return FALSE;
					previous = letter;
					break;
				}
				if ('.' == letter)
				{
					if ((++point > 1))
						return FALSE;
					previous = letter;
					break;
				}
				if (point && ('0' == previous))
					return FALSE;
				if (',' == letter)
					state = 2;	/* next */
				else if (term == letter)
					state = (')' == term) ? 8 : 5;		/* end or name */
				else
					return FALSE;
				if ((subs_count == seq) && (0 == stop))
					stop = isrc;
				previous = letter;
				break;
			case 5:		/* expect first letter of name */
				if (('%' == letter) || ISALPHA_ASCII(letter))
				{
					if (0 == seq)
						start = isrc;
					state = 6;	/* rest of name */
					break;
				}
				return FALSE;
			case 6:		/* expect next letter of name */
				if ('(' == letter)
				{
					term = ')';
					envpart = 1;
					subs_count = 0;
					state = 2;	/* done with name */
					if (0 == seq)
						stop = isrc;
				} else if (!ISALNUM_ASCII(letter))
					return FALSE;
				break;
			case 7:		/* $[Z]CHAR() */
				previous = letter;	/* in $CHAR() - must be ASCII */
				if (('Z' == letter) || ('z' == letter))
				{	if (++isrc < src->str.len)
						letter = src->str.addr[isrc];
					else
						return FALSE;
					if ('z' == previous)
						previous = 'Z';
				}
				if (!(('C' == letter) || ('c' == letter)))
					return FALSE;
				if (++isrc < src->str.len)
					letter = src->str.addr[isrc];
				else
					return FALSE;
				if (('H' == letter) || ('h' == letter))
				{
					if (++isrc < src->str.len)
						letter = src->str.addr[isrc];
					else
						return FALSE;
					if (!(('A' == letter) || ('a' == letter) || (('(' == letter) && ('Z' == previous))))
						return FALSE;
				} else if ('Z' == previous)
					return FALSE;
				if ('(' != letter)
				{
					if (++isrc < src->str.len)
						letter = src->str.addr[isrc];
					else
						return FALSE;
					if (!('R' == letter) || ('r' == letter))
						return FALSE;
					if (++isrc < src->str.len)
						letter = src->str.addr[isrc];
					else
						return FALSE;
				}
				if ('(' != letter)
					return FALSE;
				if (subs_count == seq)
					keep_quotes = TRUE;
				for (++isrc ;isrc < src->str.len; isrc++)
				{
					letter = src->str.addr[isrc];
					if (ISDIGIT_ASCII(letter))
						continue;
					if (!((',' == letter) || (')' == letter)))
						return FALSE;
					previous = letter;
					if (++isrc < src->str.len)
						letter = src->str.addr[isrc];
					else
						return FALSE;
					if (')' == previous)
						break;
					if (!ISDIGIT_ASCII(letter))
						return FALSE;
				}
				if (isrc > src->str.len)
					return FALSE;
				if ('_' == letter)
				{
					if (++isrc < src->str.len)
						letter = src->str.addr[isrc];
					else
						return FALSE;
					if ('$' == letter)
						break;
					if ('"' != letter)
						return FALSE;
					instring = TRUE;
					state = 3;	/* back to string */
					break;
				}
				if (',' == letter)
					state = 2;
				else if (term == letter)
					state = (')' == term) ? 8 : 5;		/* end or name */
				else
					return FALSE;
				if ((subs_count == seq) && (0 == stop))
					stop = isrc - (keep_quotes ? 0 : 1);	/* Not returning 2nd env part - maybe problem */
				break;
			case 8:		/* end of subscript but no closing paren - ")" */
				return FALSE;
				break;
		}
#		ifdef UNICODE_SUPPORTED
		if (!gtm_utf8_mode || (0 == (letter & 0x80)))
			isrc++;
		else if (0 < (utf8_len = UTF8_MBFOLLOW(&src->str.addr[isrc++])))
		{	/* multi-byte increment */
			assert(4 > utf8_len);
			if (0 > utf8_len)
				rts_error(VARLSTCNT(6) ERR_BADCHAR, 4, 1, &src->str.addr[isrc - 1], LEN_AND_LIT(UTF8_NAME));
			isrc += utf8_len;
		}
#		endif
		NON_UNICODE_ONLY(isrc++);
	}
	if ((8 != state) && (6 != state))
		return FALSE;
	if ((0 <= seq) && (0 == stop))
		stop = src->str.len - (8 == state ? 1 : 0);
	if (keep_quotes && ('"' == src->str.addr[start - 1]))
		start--;
	assert((0 < subs_count) || ((6 == state) && (-1 == subs_count)));
	if (6 == state)
		subs_count = 0;
	assert((('^' == src->str.addr[0]) ? MAX_GVSUBSCRIPTS : MAX_LVSUBSCRIPTS) > subs_count);
	assert((0 < isrc) && (isrc == src->str.len));
	assert(stop <= isrc);
	assert((0 <= start) && (start <= stop));
	*subscripts = subs_count;
	*start_off = start;
	*stop_off = stop;
	return TRUE;
}
