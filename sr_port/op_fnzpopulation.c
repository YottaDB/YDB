/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *                                                              *
 * Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.                                         *
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "op.h"
#include "mvalconv.h"
#include "fnpc.h"

STATICFNDCL int ZGetPieceCountFromPieceCache(mval *src, mval *del);

/* Called for $ZLENGTH() when a second argument is supplied.
 *
 * Parameters:
 *   src - Input string mval
 *   del - Input delimiter string mval
 *   dst - Return value mval - Returns the number of "pieces" in the string given the supplied delimiter. If the input
 *         string ends in a delimiter the count returned is +1'd.
 */
void	op_fnzpopulation(mval *src, mval *del, mval *dst)
{
	int	charidx, piececnt;
	mval	dummy;

	MV_FORCE_STR(src);
	MV_FORCE_STR(del);
	if (0 < src->str.len)
	{
		switch(del->str.len)
		{
			case 0:
				/* No delimiter, no pieces */
				piececnt = 0;
				break;
			case 1:
				piececnt = ZGetPieceCountFromPieceCache(src, del);
				/* So far, piececnt contains the number of pieces in the cache but if the end of the string is
				 * a delimiter, we need to return one more so do that check now and bump if appropriate.
				 */
				if (*del->str.addr == *(src->str.addr + src->str.len - 1))
					piececnt++;
				break;
			default:
				/* Run a loop of $ZFIND()s to count the delimiters */
				for (charidx = 1, piececnt = 0; charidx ; piececnt++)
					charidx = op_fnzfind(src, del, charidx, &dummy);
		}
	} else
	{	/* If del length is 0, return 0, else a null string with non-null delim returns 1 according to M standard */
		piececnt = (0 < del->str.len) ? 1 : 0;
	}
	MV_FORCE_MVAL(dst, piececnt);
}

/* Routine to force completion of the piece cache and/or arrange for manual scanning of the string as necessary.
 *
 * Parameters:
 *   src - Input string mval
 *   del - Input delimiter string mval
 *
 * Return value:
 *   Piece count
 */
STATICFNDEF int ZGetPieceCountFromPieceCache(mval *src, mval *del)
{
	int		piececnt;
	unsigned int	fnpc_indx, srclen;
	mval		dummy;
	fnpc   		*cfnpc;
	unsigned char	dlmc, *srcaddr, *last, *end;
	delimfmt	ldelim;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	DEBUG_ONLY((TREF(ZLengthReentCnt))++);
	assert(TREF(ZLengthReentCnt) <= FNPC_RECURR_MAX);
	/* Format delimiter as 4 byte "string" in an int as it is used in the piece cache and calls to "op_fnzp1" */
	dlmc = *del->str.addr;			/* Extract single char delim from del mval */
	ldelim.unichar_val = 0;
	ldelim.unibytes_val[0] = dlmc;		/* Format delimiter is stored in cache */
	srcaddr = (unsigned char *)src->str.addr;
	srclen = src->str.len;
	fnpc_indx = src->fnpc_indx - 1;
	cfnpc = &(TREF(fnpca)).fnpcs[fnpc_indx];
	/* Although the same test as below is done in op_fnzp1(), we need to do it here first to see if the
	 * current cache is usable or needs to be rebuilt.
	 */
	if ((FNPC_MAX > fnpc_indx) && (cfnpc->last_str.addr == (char *)srcaddr)
		&& (cfnpc->last_str.len == srclen) && (cfnpc->delim == ldelim.unichar_val)
		&& cfnpc->byte_oriented)
	{	/* If here, the cache is at least partially built though may be incomplete */
		if (cfnpc->pstart[cfnpc->npcs] >= srclen)
		{	/* The entire string is described in the cache so we can just pick the piece count from
			 * the cache and be (almost) done.
			 */
			piececnt = cfnpc->npcs;
		} else
		{	/* The cache is not complete - it may just need more scanning (a $piece() left off without
			 * scanning the entire line) or the piece cache is full and we need to pick up where it
			 * left off and do our own scan.
			 */
			if (cfnpc->npcs != FNPC_ELEM_MAX)
			{	/* The cache is not full - request further scanning be done */
				op_fnzp1(src, ldelim.unichar_val, FNPC_ELEM_MAX, &dummy);	/* Scan to fill cache but no more */
				piececnt = ZGetPieceCountFromPieceCache(src, del);
			} else
			{	/* The cache is full - scan chars beyond the cache to see how many more delimiters we can find */
				piececnt = cfnpc->npcs;
				last = srcaddr + cfnpc->pstart[FNPC_ELEM_MAX];
				end = srcaddr + srclen;
				while (last < end)
				{	/* Searching for delimiter chars */
					while ((last < end) && (*last != dlmc))
						last++;
					piececnt++;
					last++;			/* Move past delimiter */
				}
			}
		}
	} else
	{	/* The cache coherency check failed so rebuild the piece cache for this string */
		op_fnzp1(src, ldelim.unichar_val, FNPC_ELEM_MAX, &dummy);
		piececnt = ZGetPieceCountFromPieceCache(src, del);
	}
	DEBUG_ONLY((TREF(ZLengthReentCnt))--);
	return piececnt;
}
