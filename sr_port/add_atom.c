/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "copy.h"
#include "patcode.h"
#include "stringpool.h"
#include "min_max.h"
#include "compress.h"
#include "add_atom.h"

GBLREF spdesc 		stringpool;
GBLREF unsigned char	*outchar, *lastpatptr;
GBLREF bool		last_infinite;

bool add_atom(short int *count, unsigned char patcode, uint4 patmask,
        void *strlit_buff, unsigned char strlen, bool infinite, short int *min,
        short int *max, short int *size, short int *total, short int *total_max, int lower_bound,
        int upper_bound)
{
	unsigned char *patmaskptr;

	if (compress(patcode, patmask, strlit_buff, strlen, infinite))
	{
		min--; max--; size--;
		*min = MIN(*min + lower_bound,PAT_MAX_REPEAT);
		*max = MIN(*max + upper_bound,PAT_MAX_REPEAT);
	}
	else
	{
		*min = MIN(lower_bound,PAT_MAX_REPEAT);
		*max = MIN(upper_bound,PAT_MAX_REPEAT);
		lastpatptr = patmaskptr = outchar;
		last_infinite = infinite;

		outchar += PATSIZE;
		if (outchar - stringpool.free > MAX_PATTERN_LENGTH)
			return FALSE;

		if (patcode < PATM_STRLIT && patmask & PATM_LONGFLAGS)
		{
			if (patmask & PATM_I18NFLAGS)
			{
				PUT_LONG(patmaskptr, patmask);
				*patmaskptr |= PATM_USRDEF;
				patmaskptr += sizeof(int4);
				outchar += sizeof(int4) - PATSIZE;
				if (outchar - stringpool.free > MAX_PATTERN_LENGTH)
					return FALSE;
			}
			else
			{
				*patmaskptr++ = patmask;
			}
			*size = 1;
		}
		else
		{
			outchar += strlen + 1;
			if (outchar - stringpool.free > MAX_PATTERN_LENGTH)
				return FALSE;
			*patmaskptr++ = patcode;
			memcpy(patmaskptr, strlit_buff, strlen + 1);
			*size = strlen;
		}
		*count += 1;
	}

	*total = MIN(*total + (*size * lower_bound),PAT_MAX_REPEAT);
	*total_max = MIN(*total_max + (*size * upper_bound),PAT_MAX_REPEAT);
	return TRUE;
}
