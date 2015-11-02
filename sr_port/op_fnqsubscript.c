/****************************************************************
 *      Copyright 2001, 2004 Sanchez Computer Associates, Inc.        *
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#include "mdef.h"
#include "error.h"
#include "stringpool.h"
#include "op.h"
#include "is_canonic_name.h"

GBLREF spdesc stringpool;

/*
 * -----------------------------------------------
 * op_fnqsubscript()
 * MUMPS QSubscript function
 *
 * Arguments:
 *	src	- Pointer to Source Name string mval
 *      seq     - Sequence number of subscript to find
 *	dst	- Pointer to mval in which to save the subscript
 *
 * Return:
 *	none
 * -----------------------------------------------
 */
void op_fnqsubscript(mval *src, int seq, mval *dst)
{
	int	copy;
	int	done;
	int	lcnt;
	int	instring;
	int	subscript;
	char	envend;
	char	letter;
	mval	srcmval;
	error_def(ERR_NOSUBSCRIPT);
	error_def(ERR_NOCANONICNAME);

	if (seq < -1)
        {
                /* error "Cannot return subscript number ###" */
		rts_error(VARLSTCNT(3) ERR_NOSUBSCRIPT, 1, seq);
        }
	MV_FORCE_STR(src);
	if (!is_canonic_name(src))
		rts_error(VARLSTCNT(4) ERR_NOCANONICNAME, 2, src->str.len, src->str.addr);
	/*  A subscript cannot be longer than the whole name... */
	if (stringpool.free + src->str.len > stringpool.top)
		stp_gcol(src->str.len);
	srcmval = *src;
	src = &srcmval;		/* Copy of source mval in case same as dst mval */
	dst->str.len = 0;
	dst->str.addr = (char *)stringpool.free;
	dst->mvtype = MV_STR;
	if (-1 == seq)
	{
		copy = 0;
		instring = 0;
		for (lcnt = 0, done = 0; (lcnt < src->str.len) && !done; lcnt++)
		{
			letter = src->str.addr[lcnt];
			if (copy)
			{
				if ('"' == letter)
					instring = 1 - instring;
				if (instring)
				{
					if ('"' != letter)
						dst->str.addr[dst->str.len++] = letter;
				} else
				{
					if ((envend == letter) || (',' == letter))
						done = 1;
					else
						if (('"' != letter) || ('"' == src->str.addr[lcnt + 1]))
							dst->str.addr[dst->str.len++] = letter;
				}
			} else
			{
				if ('|' == letter)
				{
					copy = 1;
					envend = '|';
				} else if ('[' == letter)
				{
					copy = 1;
					envend = ']';
				} else if ('(' == letter)
					done = 1;
			}
		}
	} else if (0 == seq)
	{
		copy = 1;
		instring = 0;
		for (lcnt = 0, done = 0; (lcnt < src->str.len) && !done; lcnt++)
		{
			letter = src->str.addr[lcnt];
			if (copy)
			{
				if ('|' == letter)
				{
					copy = 0;
					envend = '|';
				} else if ('[' == letter)
				{
					copy = 0;
					envend = ']';
				} else if ('(' == letter)
					done = 1;
				else
					dst->str.addr[dst->str.len++] = letter;
			} else
			{
				if ('"' == letter)
					instring = 1 - instring;
				if (!instring && (envend == letter))
					copy = 1;
			}
		}
	} else
	{
		copy = 0;
		instring = 0;
		subscript = 0;
		for (lcnt = 0, done = 0; (lcnt < src->str.len) && !done; lcnt++)
		{
			letter = src->str.addr[lcnt];
			if (copy)
			{
				if ('"' == letter)
					instring = 1 - instring;
				if (instring)
				{
					if ('"' != letter)
						dst->str.addr[dst->str.len++] = letter;
				} else
				{
					if ((',' == letter) || (')' == letter))
						done = 1;
					else
						if (('"' != letter) || ('"' == src->str.addr[lcnt + 1]))
							dst->str.addr[dst->str.len++] = letter;
				}
			} else
			{
				if ('"' == letter)
					instring = 1 - instring;
				else if (!instring)
				{
					if ('(' == letter)
						subscript = 1;
					else if ((',' == letter) && (0 < subscript))
					/* don't count comma in [xxx,xxx] part... */
						subscript++;
				}
				if (subscript == seq)
					copy = 1;
			}
		}
	}
	stringpool.free += dst->str.len;
}
