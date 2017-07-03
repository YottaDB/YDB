/****************************************************************
 *								*
 * Copyright (c) 2006-2017 Fidelity National Information	*
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

#include "gtm_string.h"

#include "op.h"
#include "mvalconv.h"
#include "fnpc.h"
#include "gtm_utf8.h"

GBLREF boolean_t	gtm_utf8_mode;		/* We are indeed doing the UTF8 thang */
GBLREF boolean_t	badchar_inhibit;	/* No BADCHAR errors should be signaled */

STATICFNDCL int GetPieceCountFromPieceCache(mval *src, mval *del);

/* Called for $LENGTH() when a second argument is supplied.
 *
 * Parameters:
 *   src - Input string mval
 *   del - Input delimiter string mval
 *   dst - Return value mval - Returns the number of "pieces" in the string given the supplied delimiter. If the input
 *         string ends in a delimiter the count returned is +1'd.
 */
void	op_fnpopulation(mval *src, mval *del, mval *dst)
{
	int 		charidx, piececnt;
	mval		dummy;

	assert(gtm_utf8_mode);
	MV_FORCE_STR(src);
	MV_FORCE_STR(del);
	if (0 < src->str.len)
	{
		MV_FORCE_LEN(del);
		switch(del->str.char_len)
		{	/* Processing depends on character length of the delimiter */
			case 0:
				/* No delimiter, no pieces */
				piececnt = 0;
				break;
			case 1:
				piececnt = GetPieceCountFromPieceCache(src, del);
				break;
			default:
				/* Run a loop of $FIND()s to count the delimiters */
				for (charidx = 1, piececnt = 0; charidx ; piececnt++)
					charidx = op_fnfind(src, del, charidx, &dummy);
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
STATICFNDEF int GetPieceCountFromPieceCache(mval *src, mval *del)
{
	int		piececnt, mblen;
	unsigned int	srclen, dellen, fnpc_indx;
	mval		dummy;
	fnpc   		*cfnpc;
	unsigned char	*srcaddr, *last, *end;
	delimfmt	ldelim;
	boolean_t	valid_char;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	DEBUG_ONLY((TREF(LengthReentCnt))++);
	assert(TREF(LengthReentCnt) <= FNPC_RECURR_MAX);
	/* Format delimiter as 4 byte "string" in an int as it is used in the piece cache and calls to "op_fnp1" */
	ldelim.unichar_val = 0;
	dellen = del->str.len;
	assert(SIZEOF(int) >= dellen);
	if (1 == dellen)
		ldelim.unibytes_val[0] = *del->str.addr;
	else
		memcpy(ldelim.unibytes_val, del->str.addr, dellen);
	srcaddr = (unsigned char *)src->str.addr;
	srclen = src->str.len;
	fnpc_indx = src->fnpc_indx - 1;
	cfnpc = &(TREF(fnpca)).fnpcs[fnpc_indx];
	/* Although the same test as below is done in op_fnp1(), we need to do it here first to see if the
	 * current cache is usable or needs to be rebuilt.
	 */
	if ((FNPC_MAX > fnpc_indx) && (cfnpc->last_str.addr == (char *)srcaddr)
		&& (cfnpc->last_str.len == srclen) && (cfnpc->delim == ldelim.unichar_val)
		&& !cfnpc->byte_oriented)
	{	/* If here, the cache is at least partially built though may be incomplete */
		if (cfnpc->pstart[cfnpc->npcs] >= srclen)
		{	/* The entire string is described in the cache so we can just pick the piece count from
			 * the cache and be (almost) done.
			 */
			piececnt = cfnpc->npcs;
			/* If there's a trailing delimiter, the srclen and last piece offset should be the same. In that
			 * case, we need to add one to the returned piececnt.
			 */
			if (cfnpc->pstart[cfnpc->npcs] == srclen)
				piececnt++;
		} else
		{	/* The cache is not complete - it may just need more scanning (a $piece() left off without
			 * scanning the entire line) or the piece cache is full and we need to pick up where it
			 * left off and do our own scan.
			 */
			if (cfnpc->npcs != FNPC_ELEM_MAX)
			{	/* The cache is not full - request further scanning be done */
				op_fnp1(src, ldelim.unichar_val, FNPC_ELEM_MAX, &dummy);	/* Scan to fill cache but no more */
				piececnt = GetPieceCountFromPieceCache(src, del);
			} else
			{	/* The cache is full - scan chars beyond the cache to see how many more delimiters we can find */
				piececnt = cfnpc->npcs;
				last = srcaddr + cfnpc->pstart[FNPC_ELEM_MAX];
				end = srcaddr + srclen;
				while (last < end)
				{	/* Searching for delimiter chars */
					while (last < end)
					{
						valid_char = UTF8_VALID(last, end, mblen);	/* Length of next char */
						if (!valid_char)
						{	/* Next character is not valid unicode. If badchar error is not inhibited,
							 * signal it now. If it is inhibited, just treat the character as a single
							 * character and continue.
							 */
							if (!badchar_inhibit)
								utf8_badchar(0, last, end, 0, NULL);
							assert(1 == mblen);
						}
						/* Getting mblen first allows us to do quick length compare before the
						 * heavier weight memcmp call.
						 */
						assert(0 < mblen);
						if (mblen == dellen)
						{
							if (1 == dellen)
							{	/* Shortcut - test single byte */
								if (*last == ldelim.unibytes_val[0])
									break;
							} else
							{	/* Longcut - for multibyte check */
								if (0 == memcmp(last, ldelim.unibytes_val, dellen))
									break;
							}
						}
						last += mblen;  /* Find delim signaling end of piece */
					}
					last += dellen;		/* Bump past delim to first byte of next piece */
					piececnt++;
				}
				/* Like above, we now have a count of pieces but need to bump by 1 if a delimiter was the last
				 * scanned. Since we have already incremented last past the delimiter, if it is the same as
				 * our end address, a trailing delimiter was scanned and piecent should be bumped.
				 */
				if (end == last)
					piececnt++;
			}
		}
	} else
	{	/* The cache coherency check failed so rebuild the piece cache for this string */
		op_fnp1(src, ldelim.unichar_val, FNPC_ELEM_MAX, &dummy);
		piececnt = GetPieceCountFromPieceCache(src, del);
	}
	DEBUG_ONLY((TREF(LengthReentCnt))--);
	return piececnt;
}
