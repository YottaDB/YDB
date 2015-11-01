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
#include "min_max.h"
#include "compress.h"

GBLREF unsigned char *lastpatptr;
GBLREF bool	     last_infinite;

int compress(uint4 patcode, unsigned char patmask, void *strlit_buff, unsigned char length, bool infinite)
{
	unsigned char 	 y_max;
	unsigned char	lastpat_code;
	uint4	lastpat_mask;

	if (infinite == last_infinite)
	{
		lastpat_code = *lastpatptr;
		if (lastpat_code & PATM_USRDEF)
		{
			GET_LONG(lastpat_mask, lastpatptr);
			lastpat_mask &= PATM_LONGFLAGS;
		} else
			lastpat_mask = lastpat_code & PATM_SHORTFLAGS;
		y_max = MAX(lastpat_mask, patmask);
		if (lastpat_code == patcode && lastpat_mask == patmask)
		{
			if (patcode >= PATM_STRLIT)
			{
				if (!memcmp(lastpatptr + PATSIZE, strlit_buff, length + 1))
					return TRUE;
			} else
				return TRUE;
		} else if (infinite && lastpat_code < PATM_STRLIT && patcode < PATM_STRLIT)
		{
			if ((lastpat_mask & patmask) &&
		 	   ((lastpat_mask | patmask) == y_max))
			{
				if (y_max & PATM_I18NFLAGS)
				{
					PUT_LONG(lastpatptr, y_max);
					*lastpatptr |= PATM_USRDEF;
				} else
					*lastpatptr = y_max;
				return TRUE;
			}
		}
	}
	return FALSE;
}
