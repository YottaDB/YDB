/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <prtdef.h>
#include <psldef.h>
#include <ssdef.h>

#include "stringpool.h"
#include "fix_pages.h"

#define CHUNK_SIZE	16384	/* should be the largest OS_PAGE_SIZE, or a multiple thereof, and not an unfair strain on stp */

GBLREF spdesc		stringpool;
OS_PAGE_SIZE_DECLARE

void fix_pages(unsigned char *inbot, unsigned char *intop)
{
	register unsigned char *bot, *top;
	unsigned char	*buff, *range[2];
	uint4		chunk, size, status;

	bot = ((uint4)inbot) & ~(OS_PAGE_SIZE - 1);
	top = ((uint4)intop) & ~(OS_PAGE_SIZE - 1);
  	top += OS_PAGE_SIZE;
	range[0] = bot;
	range[1] = top - 1;
	status = sys$setprt(range, NULL, (uint4)PSL$C_USER, (uint4)PRT$C_UW, NULL);
	if (status != SS$_NORMAL)	/* can't update shared readonly memory */
	{
		chunk = top - bot;	/* already a multiple of OS_PAGE_SIZE due to rounding of bot and top */
		chunk = chunk < CHUNK_SIZE ? chunk : CHUNK_SIZE;
		ENSURE_STP_FREE_SPACE(chunk);		/* ensure temp space in the stringpool */
		buff = stringpool.free;
		assert(buff >= stringpool.base);
		assert(buff <= stringpool.top);
		chunk = (stringpool.top - buff) & ~(OS_PAGE_SIZE - 1);	/* use as much as there is */
		for (;  bot < top;  bot += size)
		{
			size = top - bot;
			if (size > chunk)
				size = chunk;
			assert(0 == (size & (OS_PAGE_SIZE - 1)));
			memcpy(buff, bot, size);			/* save the content of a chunk */
			range[0] = bot;
			range[1] = bot + size - 1;
			status = sys$cretva(range, 0, PSL$C_USER);	/* replace an address range with empty pages */
			if (status != SS$_NORMAL)
			{
				rts_error(VARLSTCNT(1) status);
				break;					/* hygenic; should never be reached */
			}
			memcpy(bot, buff, size);			/* restore content to the new overlay */
		}
	}
	return;
}
