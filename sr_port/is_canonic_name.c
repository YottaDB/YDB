/****************************************************************
 *								*
 * Copyright (c) 2001-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

#ifdef UTF8_SUPPORTED
#include "gtm_utf8.h"
GBLREF	boolean_t	badchar_inhibit;
error_def(ERR_BADCHAR);
#endif

/*
 * -----------------------------------------------
 * is_canonic_name()
 * validate a variable name (unsubscripted or subscripted).
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
{
	gv_name_and_subscripts 	start_buff, stop_buff;
	int			seq, contains_env, i;
	int 			*start, *stop;

	/* determine which buffer to use */
	DETERMINE_BUFFER(src, start_buff, stop_buff, start, stop);
	seq = *subscripts;
	if (parse_gv_name_and_subscripts(src, subscripts, start, stop, &contains_env))
	{
#		ifdef DEBUG
		/* Check that the sequence of start[i] and stop[i] is
		 * always increasing. There should never be a case where
		 * a subscript starts at an offset into src greater than
		 * where it stops.
		 */
		for (i = 0; i <= contains_env + *subscripts; i++)
			assert(start[i] < stop[i]);
#		endif
		/* Make sure the user isn't trying to index too far into the
		 * array or trying to index a negative offset.
		 * Remember contains_env is 1 if there is an environment,
		 * and {start,stop}[0] contain the bounds for the environment.
		 */
		if ((0 > contains_env + seq) || (*subscripts < seq))
			*start_off = *stop_off = 0;
		else
		{
			*start_off = start[contains_env + seq];
			*stop_off = stop[contains_env + seq];
		}
		if ('"' == src->str.addr[*start_off])
			start_off++;
		if ((0 < *stop_off) && ('"' == src->str.addr[*stop_off - 1]))
			stop_off--;
		return TRUE;
	}
	return FALSE;
}

/*
 * -----------------------------------------------
 * parse_gv_name_and_subscripts()
 * Validates a global variable name and returns all the corresponding subscripts (unsubscripted or subscripted).
 *
 * Arguments:
 *	src	     - Pointer to Source Name string mval.
 *	subscripts   - Pointer returns the subscript count.
 *	start        - Assumed to be an array of length [MAX_[L,G]VSUBSCRIPTS + 1 + 2], returns the start
 *		       of every subscript. If contains_env is true, the index 0 corresponds to the
 *		       first environment subscript, 1 corresponds to the name, and 2 -> *subscripts
 *		       corresponds to the keys.
 *	stop         - Assumed to be an array of length [MAX_[L,G]VSUBSCRIPTS + 1 + 2], returns the end
 *		       of every subscript, corresponding to each entry in start.
 *	contains_env - Pointer returns whether there was an environment or not. This is an integer
 *		       for ease of indexing into the start and stop arrays, i.e. start[contains_env]
 *		       will always return the start of the name.
 * Return:
 *	boolean_t    - TRUE indicates good name; FALSE indicates defective
 * -----------------------------------------------
 */
boolean_t parse_gv_name_and_subscripts(mval *src, int *subscripts, int *start, int *stop, int *contains_env)
{
	char		term;
	int		envpart;
	boolean_t	instring, innum;
	int		isrc;
	char		letter;
	int		point;
	char		previous;
	int		seq;
	int		state;
	int		subs_count;
	int		utf8_len;
	int		subs_max;
	boolean_t 	gvn;
	register char	*cpt;
	char		*lastcpt;
	enum
	{
		BEFORE_NAME,
		CHECK_FOR_ENVIRONMENT,
		DISPATCH_FOR_STARTING_COMPONENT,
		IN_STRING,
		IN_NUMBER,
		EXPECT_FIRST_LETTER_OF_NAME,
		EXPECT_NEXT_LETTER_OF_NAME,
		IN_CHAR_FUNC,
		END_OF_PROCESSING
	};

	gvn = ((0 < src->str.len) && ('^' == src->str.addr[0]));
	MV_FORCE_STR(src);
	seq = *subscripts;
	state = BEFORE_NAME;
	term = 0;
	envpart = 0;
	point = 0;
	previous = 0;
	subs_count = -1;
	*contains_env = 0;
	subs_max = gvn ? MAX_GVSUBSCRIPTS : MAX_LVSUBSCRIPTS;
	isrc = 0;
	lastcpt = src->str.addr + src->str.len;
	for (cpt = src->str.addr; (cpt < lastcpt) && (subs_count < subs_max);)
	{
		letter = *cpt;
		switch (state)
		{
			case BEFORE_NAME:		/* start of name */
				if ('^' == letter)	/* before start of name */
				{
					state = CHECK_FOR_ENVIRONMENT;	/* check for environment */
					break;
				}
				if (('%' == letter) || ISALPHA_ASCII(letter))
				{
					*start++ = isrc;
					state = EXPECT_NEXT_LETTER_OF_NAME;	/* rest of name */
					break;
				}
				return FALSE;
			case CHECK_FOR_ENVIRONMENT:		/* global name */
				if (('%' == letter) ||ISALPHA_ASCII(letter))	/* found ^ allow environment */
				{	/* found ^ allow environment */
					*start++ = isrc;
					state = EXPECT_NEXT_LETTER_OF_NAME;	/* rest of name */
					break;
				}
				if (('|' == letter) || ('[' == letter))
				{
					*contains_env = 1;
					term = (letter == '[') ? ']' : letter;
					envpart = 0;
					state = DISPATCH_FOR_STARTING_COMPONENT;	/* process environment */
					break;
				}
				return FALSE;
			case DISPATCH_FOR_STARTING_COMPONENT:		 /* dispatch for starting a component */
				point = 0;
				instring = FALSE;
				if (1 < envpart)
					return FALSE;	/* too many environment components */
				if (')' == term)
					subs_count++;	/* new subscript */
				else
					envpart++;	/* next environment component */
				if ((0 < subs_count) || (1 == envpart))
					*start++ = isrc;
				if ('"' == letter)
				{
					instring = TRUE;
					state = IN_STRING;	/* string */
					break;
				}
				if ('$' ==letter)
				{
					state = IN_CHAR_FUNC;	/* $[z]char() */
					break;
				}
				if ('0' == letter) /* Canonic number cannot start with 0 unless is single char */
				{
					++isrc;
					if (++cpt < lastcpt)
						letter = *cpt;
					else
						return FALSE;	/* Cannot end with "0" */
					if (term == letter)
						/* end or name */
						state = (')' == term) ? END_OF_PROCESSING : EXPECT_FIRST_LETTER_OF_NAME;
					else if (',' != letter)
						return FALSE;	/* Not a single char number */
					*stop++ = isrc;
					break;
				}
				if ((innum = ISDIGIT_ASCII(letter)) || ('-' == letter) || ('.' == letter)) /* WARNING: assignment */
				{	/* note: innum cannot be TRUE unless dispatch state is IN_NUMBER; can be FALSE too
					 * order of evaluation for the if matters; extra assignment wasted if only about to error
					 */
					if ('.' == letter)
						point++;
					previous = letter;
					state = IN_NUMBER;	/* numeric */
					break;
				}
				return FALSE;
			case IN_STRING:		/* [quoted] string */
				if ('"' == letter)	/* in string */
				{
					instring = !instring;
					if (instring)
						break;
					if (cpt + 1 >= lastcpt)
						return FALSE;
					if ('_' != *(cpt+1))
						break;
					isrc++;
					cpt++;
					if (++isrc, ++cpt < lastcpt)
						letter = *cpt;
					else
						return FALSE;
					if ('$' != letter)
						return FALSE;
					state = IN_CHAR_FUNC;	/* $[z]char() */
					break;
				}
				if (!instring)
				{
					if (',' == letter)
						state = DISPATCH_FOR_STARTING_COMPONENT;	/* on to next */
					else if (term == letter)
						/* end or name */
						state = (')' == term) ? END_OF_PROCESSING : EXPECT_FIRST_LETTER_OF_NAME;
					else
						return FALSE;
					if ((0 < subs_count) || (1 == envpart))
						*stop++ = isrc;
				}
				break;
			case IN_NUMBER:		/* numeric */
				if (ISDIGIT_ASCII(letter))	/* in number */
				{
					if (('-' == previous) && ('0' == letter))
						return FALSE;
					previous = letter;
					innum = TRUE;
					break;
				}
				if ('.' == letter)
				{
					if ((++point > 1))
						return FALSE;
					previous = letter;
					break;
				}
				if (!innum || (point && ('0' == previous)))
					return FALSE;
				if (',' == letter)
					state = DISPATCH_FOR_STARTING_COMPONENT;	/* next */
				else if (term == letter)
					/* end or name */
					state = (')' == term) ? END_OF_PROCESSING : EXPECT_FIRST_LETTER_OF_NAME;
				else
					return FALSE;
				if ((0 < subs_count) || (1 == envpart))
					*stop++ = isrc;
				previous = letter;
				break;
			case EXPECT_FIRST_LETTER_OF_NAME:		/* expect first letter of name */
				if (('%' == letter) || ISALPHA_ASCII(letter))
				{
					*start++ = isrc;
					state = EXPECT_NEXT_LETTER_OF_NAME;	/* rest of name */
					break;
				}
				return FALSE;
			case EXPECT_NEXT_LETTER_OF_NAME:		/* expect next letter of name */
				if ('(' == letter)
				{
					term = ')';
					envpart = 1;
					subs_count = 0;
					state = DISPATCH_FOR_STARTING_COMPONENT;	/* done with name */
					*stop++ = isrc;
				} else if (!ISALNUM_ASCII(letter))
					return FALSE;
				break;
			case IN_CHAR_FUNC:		/* $[Z]CHAR() */
				previous = letter;	/* in $CHAR() - must be ASCII */
				if (('Z' == letter) || ('z' == letter))
				{
					++isrc;
					if (++cpt < lastcpt)
						letter = *cpt;
					else
						return FALSE;
					previous = 'Z';
				}
				if (!(('C' == letter) || ('c' == letter)))
					return FALSE;
				++isrc;
				if (++cpt < lastcpt)
					letter = *cpt;
				else
					return FALSE;
				if (('H' == letter) || ('h' == letter))
				{
					++isrc;
					if (++cpt < lastcpt)
						letter = *cpt;
					else
						return FALSE;
					if (!(('A' == letter) || ('a' == letter) || (('(' == letter) && ('Z' == previous))))
						return FALSE;
				} else if ('Z' == previous)
					return FALSE;
				if ('(' != letter)
				{
					++isrc;
					if (++cpt < lastcpt)
						letter = *cpt;
					else
						return FALSE;
					if (!(('R' == letter) || ('r' == letter)))
						return FALSE;
					++isrc;
					if (++cpt < lastcpt)
						letter = *cpt;
					else
						return FALSE;
				}
				if ('(' != letter)
					return FALSE;
				for (++isrc,++cpt, innum = FALSE; cpt < lastcpt; isrc++, cpt++)
				{
					letter = *cpt;
					if (ISDIGIT_ASCII(letter))
					{
						if (!innum && ('0' == previous))
							return FALSE;
						if ('0' == letter)
							previous = letter;
						else
							innum = TRUE;
						continue;
					}
					if (!((',' == letter) || (')' == letter)))
						return FALSE;
					if (!innum && ('0' != previous))
						return FALSE;
					previous = letter;
					++isrc;
					if (++cpt < lastcpt)
						letter = *cpt;
					else
						return FALSE;
					if (')' == previous)
						break;
					if (!ISDIGIT_ASCII(letter))
						return FALSE;
					if ('0' == letter)
					{
						isrc--;
						cpt--;
						innum = FALSE;
					} else
						innum = TRUE;
				}
				if (cpt > lastcpt)
					return FALSE;
				if ('_' == letter)
				{
					++isrc;
					if (++cpt < lastcpt)
						letter = *cpt;
					else
						return FALSE;
					if ('$' == letter)
						break;
					if ('"' != letter)
						return FALSE;
					instring = TRUE;
					state = IN_STRING;	/* back to string */
					break;
				}
				if (',' == letter)
					state = DISPATCH_FOR_STARTING_COMPONENT;
				else if (term == letter)
					/* end or name */
					state = (')' == term) ? END_OF_PROCESSING : EXPECT_FIRST_LETTER_OF_NAME;
				else
					return FALSE;
				*stop++ = isrc;
				break;
			case END_OF_PROCESSING:		/* end of subscript but no closing paren - ")" */
				return FALSE;
		}
#		ifdef UTF8_SUPPORTED
		if (!gtm_utf8_mode || (0 == (letter & 0x80)))
			isrc++, cpt++;
		else if (0 < (utf8_len = UTF8_MBFOLLOW((isrc++, cpt++))))
		{	/* multi-byte increment */
			assert(4 > utf8_len);
			if (0 > utf8_len)
				RTS_ERROR_CSA_ABT(NULL,
					VARLSTCNT(6) ERR_BADCHAR, 4, cpt-1, LEN_AND_LIT(UTF8_NAME));
			isrc += utf8_len;
			cpt += utf8_len;
		}
#		endif
		NON_UTF8_ONLY({cpt++; isrc++});
	}
	if ((END_OF_PROCESSING != state) && (EXPECT_NEXT_LETTER_OF_NAME != state))
		return FALSE;
	assert((0 < subs_count) || ((EXPECT_NEXT_LETTER_OF_NAME == state) && (-1 == subs_count)));
	if (EXPECT_NEXT_LETTER_OF_NAME == state)
	{
		subs_count = 0;
		*stop = isrc;
	}
	assert((('^' == src->str.addr[0]) ? MAX_GVSUBSCRIPTS : MAX_LVSUBSCRIPTS) > subs_count);
	assert((0 < isrc) && (isrc == src->str.len) && (cpt == lastcpt));
	*subscripts = subs_count;
	return TRUE;
}
