/****************************************************************
 *								*
 *	Copyright 2006, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "stringpool.h"
#include "min_max.h"
#include "fnpc.h"
#include "op.h"

GBLREF spdesc		stringpool;

#ifdef DEBUG
GBLREF	boolean_t	setp_work;
GBLREF	int		cs_small;			/* scanned small string brute force */
GBLREF	int		cs_small_pcs;			/* chars scanned by small scan */
#  define SETWON setp_work = TRUE;
#  define SETWOFF setp_work = FALSE;
#  define COUNT_EVENT(x) ++x;
#  define INCR_COUNT(x,y) x += y;
#else
#  define SETWON
#  define SETWOFF
#  define COUNT_EVENT(x)
#  define INCR_COUNT(x,y)
#endif

error_def(ERR_MAXSTRLEN);

/*
 * ----------------------------------------------------------
 * Fast path setzpiece when delimiter is one (lit) char replacing
 * a single piece (last is same as first).
 *
 * Arguments:
 *	src	- source mval
 *	delim	- delimiter char
 *	expr	- expression string mval
 *	ind	- index in source mval to be set
 *	dst	- destination mval where the result is saved.
 *
 * Return:
 *	none
 * ----------------------------------------------------------
 */
void op_setzp1(mval *src, int delim, mval *expr, int ind, mval *dst)
{
	size_t		str_len, delim_cnt;
	int		len, pfx_str_len, sfx_start_offset, sfx_str_len, rep_str_len, pfx_scan_offset;
	int		cpy_cache_lines;
	unsigned char	ldelimc, lc, *start_sfx, *str_addr, *end_pfx, *end_src, *start_pfx;
	boolean_t	do_scan;
	mval		dummymval;	/* It's value is not used but is part of the call to op_fnp1() */
	fnpc		*cfnpc, *pfnpc;
	delimfmt	ldelim;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ldelim.unichar_val = delim;		/* Local copys (in unsigned char and integer formats) */
	ldelimc = ldelim.unibytes_val[0];
	do_scan = FALSE;
	cpy_cache_lines = -1;
	MV_FORCE_STR(expr);			/* Expression to put into piece place */
	if (MV_DEFINED(src))
	{
		/* We have 3 possible scenarios:
		 *
		 * 1) If length of src is too small to cause cacheing by op_fnzp1, then just do
		 *    the work ourselves with no cacheing.
		 * 2) If the requested piece is larger than can be cached by op_fnzp1, call fnzp1
		 *    for the maximum piece possible, use the cache info to "prime the pump" and
		 *    then process the rest of the string ourselves.
		 * 3) If the requested piece can be obtained from the cache, call op_fnzp1 to validate
		 *    and rebuild the cache if necessary and then retrieve the necessary info from
		 *    the fnpc cache.
		 */
		MV_FORCE_STR(src);	/* Make sure is string prior to length check */
		if (FNPC_STRLEN_MIN < src->str.len && FNPC_ELEM_MAX >= ind)
		{	/* 3) Best of all possible cases. The op_fnzp1 can do most of our work for us
			 *     and we can preload the cache on the new string to help its subsequent
			 *     uses along as well.
			 */
			SETWON;
			op_fnzp1(src, delim, ind, &dummymval, FALSE);
			SETWOFF;
			cfnpc = &(TREF(fnpca)).fnpcs[src->fnpc_indx - 1];
			assert(cfnpc->last_str.addr == src->str.addr);
			assert(cfnpc->last_str.len == src->str.len);
			assert(cfnpc->delim == ldelim.unichar_val);
			assert(0 < cfnpc->npcs);
			/* Three more scenarios: #1 piece all in cache, #2 piece would be in cache but ran
			 * out of text or #3 piece is beyond what can be cached
			 */
			if (cfnpc->npcs >= ind)
			{	/* #1 The piece we want is totally within the cache which is good news */
				pfx_str_len = cfnpc->pstart[ind - 1];
				delim_cnt = 0;
				sfx_start_offset = cfnpc->pstart[ind] - 1;			/* Include delimiter */
				rep_str_len = cfnpc->pstart[ind] - cfnpc->pstart[ind - 1] - 1;	/* Replace string length */
				sfx_str_len = src->str.len - pfx_str_len - rep_str_len;
				cpy_cache_lines = ind - 1;
			} else
			{	/* #2 The string was too short so the cache does not contain our string. This means
				 * that the prefix becomes any text that IS in the cache and we set the delim_cnt
				 * to be the number of missing pieces so the delimiters can be put in as part of the
				 * prefix when we build the new string.
				 */
				pfx_str_len = cfnpc->pstart[cfnpc->npcs] - 1;
				delim_cnt = (size_t)(ind - cfnpc->npcs);
				sfx_start_offset = 0;
				sfx_str_len = 0;
				cpy_cache_lines = cfnpc->npcs;
			}
		} else if (FNPC_STRLEN_MIN < src->str.len)
		{	/* 2) We have a element that would not be able to be in the fnpc cache. Go ahead
			 *    and call op_fnzp1 to get cache info up to the maximum and then we will continue
			 *    the scan on our own.
			 */
			SETWON;
			op_fnzp1(src, delim, FNPC_ELEM_MAX, &dummymval, FALSE);
			SETWOFF;
			cfnpc = &(TREF(fnpca)).fnpcs[src->fnpc_indx - 1];
			assert(cfnpc->last_str.addr == src->str.addr);
			assert(cfnpc->last_str.len == src->str.len);
			assert(cfnpc->delim == ldelim.unichar_val);
			assert(0 < cfnpc->npcs);
			if (FNPC_ELEM_MAX > cfnpc->npcs)
			{	/* We ran out of text so the scan is complete. This is basically the same
				 * as case #2 above.
				 */
				pfx_str_len = cfnpc->pstart[cfnpc->npcs] - 1;
				delim_cnt = (size_t)(ind - cfnpc->npcs);
				sfx_start_offset = 0;
				sfx_str_len = 0;
				cpy_cache_lines = cfnpc->npcs;
			} else
			{	/* We have a case where the piece we want cannot be kept in cache. In the special
				 * case where there is no more text to handle, we don't need to scan further. Otherwise
				 * we prime the pump and continue the scan where the cache left off.
				 */
				if ((pfx_scan_offset = cfnpc->pstart[FNPC_ELEM_MAX]) < src->str.len)
				{	/* Normal case where we prime the pump */
					do_scan = TRUE;
				} else
				{	/* Special case -- no more text to scan */
					pfx_str_len = cfnpc->pstart[FNPC_ELEM_MAX] - 1;
					sfx_start_offset = 0;
					sfx_str_len = 0;
				}
				delim_cnt = (size_t)ind - FNPC_ELEM_MAX;
				cpy_cache_lines = FNPC_ELEM_MAX;
			}
		} else
		{	/* 1) We have a short string where no cacheing happens. Do the scanning work ourselves */
			MV_FORCE_STR(src);
			do_scan = TRUE;
			pfx_scan_offset = 0;
			delim_cnt = (size_t)ind;
		}
	} else
	{	/* Source is not defined -- treat as a null string */
		pfx_str_len = sfx_str_len = sfx_start_offset = 0;
		delim_cnt = (size_t)ind - 1;
	}
	/* If we have been forced to do our own scan, do that here. Note the variable pfx_scan_offset has been
	 * set to where the scan should begin in the src string and delim_cnt has been set to how many delimiters
	 * still need to be processed.
	 */
	if (do_scan)
	{	/* Scan the line isolating prefix piece, and end of the piece being replaced */
		COUNT_EVENT(cs_small);
		end_pfx = start_sfx = (unsigned char *)src->str.addr + pfx_scan_offset;
		end_src = (unsigned char *)src->str.addr + src->str.len;
		/* The compiler would unroll this loop this way anyway but we want to
		 * adjust the start_sfx pointer after the loop but only if we have gone
		 * into it at least once.
		 */
		if ((0 < delim_cnt) && (start_sfx < end_src))
		{
			do
			{
				end_pfx = start_sfx;
				while (start_sfx < end_src && (lc = *start_sfx) != ldelimc) start_sfx++;
				start_sfx++;
				delim_cnt--;
			} while ((0 < delim_cnt) && (start_sfx < end_src));
			/* We have to backup up the suffix start pointer except under the condition
			 * that the last character in the buffer is the last delimiter we were looking
			 * for.
			 */
			if ((0 == delim_cnt) || (start_sfx < end_src) || (lc != ldelimc))
				--start_sfx;				/* Back up suffix to include delimiter char */
			/* If we scanned to the end (no text left) and still have delimiters to
			 * find, the entire src text should be part of the prefix
			 */
			if ((start_sfx >= end_src) && (0 < delim_cnt))
			{
				end_pfx = start_sfx;
				if (lc == ldelimc)			/* if last char was delim, reduce delim cnt */
					--delim_cnt;
			}
		} else
		{
			/* If not doing any token finding, then this count becomes the number
			 * of tokens to output. Adjust accordingly.
			 */
			if (0 < delim_cnt)
				delim_cnt--;
		}
		INCR_COUNT(cs_small_pcs, (int)((size_t)ind - delim_cnt));
		/* Now having the following situation:
		 * end_pfx	-> end of the prefix piece including delimiter
		 * start_sfx	-> start of suffix piece (with delimiter) or = end_pfx/src->str.addr if none
		 */
		pfx_str_len = end_pfx - (unsigned char *)src->str.addr;
		if (0 > pfx_str_len)
			pfx_str_len = 0;
		sfx_start_offset = start_sfx - (unsigned char *)src->str.addr;
		sfx_str_len = src->str.len - sfx_start_offset;
		if (0 > sfx_str_len)
			sfx_str_len = 0;
	}
	/* Calculate total string len. delim_cnt has needed padding delimiters for null fields */
	str_len = (size_t)expr->str.len + (size_t)pfx_str_len + delim_cnt + (size_t)sfx_str_len;
	if (MAX_STRLEN < str_len)
		rts_error(VARLSTCNT(1) ERR_MAXSTRLEN);
	ENSURE_STP_FREE_SPACE((int)str_len);
	str_addr = stringpool.free;
	start_pfx = (unsigned char *)src->str.addr;
	/* copy prefix */
	if (0 < pfx_str_len)
	{
		memcpy(str_addr, src->str.addr, pfx_str_len);
		str_addr += pfx_str_len;
	}
	/* copy delimiters */
	while (0 < delim_cnt--)
		*str_addr++ = ldelimc;
	/* copy expression */
	if (0 < expr->str.len)
	{
		memcpy(str_addr, expr->str.addr, expr->str.len);
		str_addr += expr->str.len;
	}
	/* copy suffix */
	if (0 < sfx_str_len)
	{
		memcpy(str_addr, start_pfx + sfx_start_offset, sfx_str_len);
		str_addr += sfx_str_len;
	}
	assert((str_addr - stringpool.free) == str_len);
	dst->mvtype = MV_STR;
	dst->str.len = str_addr - stringpool.free;
	dst->str.addr = (char *)stringpool.free;
	stringpool.free = str_addr;
	/* If available, update the cache information for this newly created mval to hopefully
	 * give it a head start on its next usage. Note that we can only copy over the cache info
	 * for the prefix. We cannot include information for the 'expression' except where it starts
	 * because the expression could itself contain delimiters that would be found on a rescan.
	 */
	if (0 < cpy_cache_lines)
	{
		pfnpc = cfnpc;				/* pointer for src mval's cache */
		do
		{
			cfnpc = (TREF(fnpca)).fnpcsteal;	/* Next cache element to steal */
			if ((TREF(fnpca)).fnpcmax < cfnpc)
				cfnpc = &(TREF(fnpca)).fnpcs[0];
			(TREF(fnpca)).fnpcsteal = cfnpc + 1;	/* -> next element to steal */
		} while (cfnpc == pfnpc);		/* Make sure we don't step on ourselves */
		cfnpc->last_str = dst->str;		/* Save validation info */
		cfnpc->delim = ldelim.unichar_val;
		cfnpc->npcs = cpy_cache_lines;
		cfnpc->byte_oriented = TRUE;
		dst->fnpc_indx = cfnpc->indx + 1;	/* Save where we are putting this element
							 * (1 based index in mval so 0 isn't so common)
							 */
		memcpy(&cfnpc->pstart[0], &pfnpc->pstart[0], (cfnpc->npcs + 1) * SIZEOF(unsigned int));
	} else
		/* No cache available -- just reset index pointer to get fastest cache validation failure */
		dst->fnpc_indx = -1;

}
