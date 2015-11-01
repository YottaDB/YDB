/****************************************************************
 *      Copyright 2001 Sanchez Computer Associates, Inc.        *
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#include "mdef.h"
#include "gtm_ctype.h"
#include "is_canonic_name.h"

boolean_t is_canonic_name(mval *src)
{
	char		envend;
	int		envpart;
	int		isrc;
	int		instring;
        char		letter;
	char		previous;
	char		next;
	int		point;
	int		state;

	/* state:
	 *    0      before start of name
	 *    1      found ^ allow environment
	 *    2      found | or [ expect environment
	 *    3      in 'environment' string
	 *    4      in 'environemnt' number
	 *    5      expect first letter of name
	 *    6      expect next letter of name
	 *    7      at ( or ,
	 *    8      in string subscript
	 *    9      in numeric subscript
	 *   10      at )
	 */

	MV_FORCE_STR(src);

	state = 0;
	envpart = 0;
	letter = -1;
	for (isrc = 0; isrc < src->str.len; isrc++)
	{
		previous = letter;
		letter = src->str.addr[isrc];
		switch (state)
		{
		case 0: if ('^' == letter)
			{
				state = 1;
				break;
			}
			if ('|' == letter)
			{
				if (0 == isrc)
					return FALSE; /* local name cannot have environment */
				state = 2;
				envend = '|';
				break;
			}
			if ('[' == letter)
			{
				state = 2;
				envend = ']';
				envpart = 0;
				break;
			}
			if (('%' == letter) || isalpha(letter))
			{
				state = 6;
				break;
			}
			return FALSE;
		case 1: if ('|' == letter)
			{
				state = 2;
				envend = '|';
				break;
			}
			if ('[' == letter)
			{
				state = 2;
				envend = ']';
				break;
			}
			if (('%' == letter) ||isalpha(letter))
			{
				state = 6;
				break;
			}
			return FALSE;
		case 2: point = 0;
			instring = 0;
			if ('"' == letter)
			{
				state = 3;
				instring = 1;
				break;
			}
			if ('.' == letter)
				point++;
			if ('0' == letter) /* Canonic number cannot start with 0 unless is single char */
			{
				if (isrc + 1 < src->str.len)
					next = src->str.addr[isrc + 1];
				else
					return FALSE;	/* Cannot end environment with "0" */
				if ('.' == next || isdigit(next))
					return FALSE;	/* Not a single char number */
			}
			if (('.' == letter) || ('-' == letter) || isdigit(letter))
			{
				state = 4;
				break;
			}
			return FALSE;
		case 3: if ('"' == letter)
				instring = 1 - instring;
			else if (!instring)
				if (envend == letter)
					state = 5;
				else if ((']' == envend) && (',' == letter))
				{
					envpart++;
					state = 2;
					if (envpart > 1)
						return FALSE;
				}
				else
					return FALSE;
			break;
		case 4: if ((']' == envend) && (',' == letter))
			{
				if (point && ('0' == previous))
					return FALSE;
				envpart++;
				state = 2;
				if (envpart > 1)
					return FALSE;
			}
			else if (('.' != letter) && !isdigit(letter) && (envend != letter))
				return FALSE;
			if (envend == letter)
			{
				if (point && ('0' == previous))
					return FALSE;
				state = 5;
				break;
			}
			if (('.' == letter) && (++point > 1))
				return FALSE;
			break;
		case 5: if (('%' == letter) || isalpha(letter))
			{
				state = 6;
				break;
			}
			return FALSE;
		case 6: if ('(' == letter)
				state = 7;
			else if (!isalnum(letter))
				return FALSE;
			break;
		case 7: point = 0;
			instring = 0;
			if ('"' == letter)
			{
				state = 8;
				instring = 1;
				break;
			}
			if ('.' == letter)
				point++;
			if ('0' == letter) /* Canonic number cannot start with 0 */
			{
				if (isrc + 1 < src->str.len)
					next = src->str.addr[isrc + 1];
				else
					return FALSE;	/* Cannot end subscript list with "0" */
				if ('.' == next || isdigit(next))
					return FALSE;	/* Not a single char number */
			}
			if (('-' == letter) || ('.' == letter) || isdigit(letter))
			{
				state = 9;
				break;
			}
			return FALSE;
		case 8: if ('"' == letter)
				instring = 1 - instring;
			else if (!instring)
			{
				if (',' == letter)
				{
					state = 7;
					break;
				}
				if (')' == letter)
				{
					state = 10;
					break;
				}
				return FALSE;
			}
			break;
		case 9: if (',' == letter)
			{
				if (point && ('0' == previous))
					return FALSE;
				state = 7;
				break;
			}
			if (')' == letter)
			{
				if (point && ('0' == previous))
					return FALSE;
				state = 10;
				break;
			}
			if (('.' != letter) && !isdigit(letter))
				return FALSE;
			if (('.' == letter) && (++point > 1))
				return FALSE;
			break;
		case 10: return FALSE;
			break;
		}
	}
	if ((10 != state) && (6 != state))
		return FALSE;
	return TRUE;
}
