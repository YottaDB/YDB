/****************************************************************
 *								*
 *	Copyright 2006, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 * -----------------------------------------------
 * op_fnzp1 $ZPIece function (the piecemaker)
 * Special case of 1 char delimiter and 1 piece (reference)
 *
 * Arguments:
 *	src		- pointer to Source mval
 *	del		- delimiter char to use looking for a piece
 *	trgpcidx	- index of piece to extract from source string
 *	dst		- pointer to Destination mval to save the piece in
 *
 * Return:
 *	none
 *
 * Side effects:
 *	dst structure gets filled with the result
 * -----------------------------------------------
 */


#include "mdef.h"

#include "fnpc.h"
#include "min_max.h"
#include "op.h"

GBLREF boolean_t	badchar_inhibit;	/* Not recognizing bad characters in UTF8 */

#ifdef DEBUG
GBLREF	uint4	process_id;
GBLREF	boolean_t	setp_work;		/* The work we are doing is for set $piece */
GBLREF	int	c_miss;				/* cache misses (debug) */
GBLREF	int	c_hit;				/* cache hits (debug) */
GBLREF	int	c_small;			/* scanned small string brute force */
GBLREF	int	c_small_pcs;			/* chars scanned by small scan */
GBLREF	int	c_pskip;			/* number of pieces "skipped" */
GBLREF	int	c_pscan;			/* number of pieces "scanned" */
GBLREF	int	c_parscan;			/* number of partial scans (partial cache hits) */
GBLREF	int	cs_miss;			/* cache misses (debug) */
GBLREF	int	cs_hit;				/* cache hits (debug) */
GBLREF	int	cs_small;			/* scanned small string brute force */
GBLREF	int	cs_small_pcs;			/* chars scanned by small scan */
GBLREF	int	cs_pskip;			/* number of pieces "skipped" */
GBLREF	int	cs_pscan;			/* number of pieces "scanned" */
GBLREF	int	cs_parscan;			/* number of partial scans (partial cache hits) */
GBLREF	int	c_clear;			/* cleared due to (possible) value change */
#  define COUNT_EVENT(x) if (setp_work) ++cs_##x; else ++c_##x;
#  define INCR_COUNT(x,y) if (setp_work) cs_##x += y; else c_##x += y;
#else
#  define COUNT_EVENT(x)
#  define INCR_COUNT(x,y)
#endif

void op_fnzp1(mval *src, int delim, int trgpcidx, mval *dst)
{
	unsigned char	*first, *last, *start, *end;
	unsigned char	dlmc;
	unsigned int	*pcoff, *pcoffmax, fnpc_indx, slen;
	int		trgpc, cpcidx, spcidx;
	mval		ldst;		/* Local copy since &dst == &src .. move to dst at return */
	fnpc   		*cfnpc;
	delimfmt	ldelim;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(src);
	ldst.mvtype = MV_STR;
	DEBUG_ONLY(ldst.str.len = -1);	/* Trigger assert at end if length not changed */
	ldelim.unichar_val = delim;
	dlmc = ldelim.unibytes_val[0];
	start = first = last = (unsigned char *)src->str.addr;
	slen = src->str.len;
	end = start + slen;

	/* Detect annoyance cases and deal with quickly so we don't muck up the
	   logic below trying to handle it properly */
	if (0 >= trgpcidx || 0 == slen)
	{
		ldst.str.addr = (char *)start;
		ldst.str.len  = 0;
		*dst = ldst;
		return;
	}

	/* Test mval for valid cache: index ok, mval addr same, delim same. One additional test
	 * is if byte_oriented is FALSE, then this cache entry was created by $PIECE (using
	 * UTF8 chars) and since we cannot reuse it, we must ignore the cache and rebuild it for
	 * this mval. */
	fnpc_indx = src->fnpc_indx - 1;
	cfnpc = &(TREF(fnpca)).fnpcs[fnpc_indx];
	if (FNPC_MAX > fnpc_indx && cfnpc->last_str.addr == (char *)first &&
	    cfnpc->last_str.len == slen && cfnpc->delim == ldelim.unichar_val &&
	    cfnpc->byte_oriented)
	{
		/* Have valid cache. See if piece we want already in cache */
		COUNT_EVENT(hit);
		INCR_COUNT(pskip, cfnpc->npcs);

		if (trgpcidx <= cfnpc->npcs)
		{
			/* Piece is totally in cache no scan needed */
			ldst.str.addr = (char *)first + cfnpc->pstart[trgpcidx - 1];
			ldst.str.len = cfnpc->pstart[trgpcidx] - cfnpc->pstart[trgpcidx - 1] - 1;
			assert(ldst.str.len >= 0 && ldst.str.len <= src->str.len);
			*dst = ldst;
			return;
		} else
		{
			/* Not in cache but pick up scan where we left off */
			cpcidx = cfnpc->npcs;
			first = last = start + cfnpc->pstart[cpcidx];	/* First char of next pc */
			pcoff = &cfnpc->pstart[cpcidx];
			if (pcoff == cfnpc->pcoffmax)
				++pcoff; 		/* No further updates to pstart array */
			++cpcidx;			/* Now past last piece and on to next one */
			COUNT_EVENT(parscan);
		}
	} else
	{
		/* The piece cache index or mval validation was incorrect.
		   Start from the beginning */

		COUNT_EVENT(miss);

		/* Need to steal a new piece cache, get "least recently reused" */
		cfnpc = (TREF(fnpca)).fnpcsteal;	/* Get next element to steal */
		if ((TREF(fnpca)).fnpcmax < cfnpc)
			cfnpc = &(TREF(fnpca)).fnpcs[0];
		(TREF(fnpca)).fnpcsteal = cfnpc + 1;	/* -> next element to steal */

		cfnpc->last_str = src->str;		/* Save validation info */
		cfnpc->delim = ldelim.unichar_val;
		cfnpc->npcs = 0;
		cfnpc->byte_oriented = TRUE;
		src->fnpc_indx = cfnpc->indx + 1;	/* Save where we are putting this element
							   (1 based index in mval so 0 isn't so common) */
		pcoff = &cfnpc->pstart[0];
		cpcidx = 1;				/* current piece index */
	}

	/* Do scan filling in offsets of pieces if they fit in the cache */
	spcidx = cpcidx;				/* Starting value for search */
	pcoffmax = cfnpc->pcoffmax;			/* Local end of array value */
	while (cpcidx <= trgpcidx && last < end)
	{
		/* Once through for each piece we pass, last time through to find length of piece we want */
		first = last;				/* first char of current piece */
		while (last != end && *last != dlmc) last++;	/* Find delim signaling end of piece */
		last++;					/* Bump past delim to first char next piece,
							   or if hit last char, +2 past end of piece */
		++cpcidx;				/* Next piece */
		if (pcoff < pcoffmax)
			*pcoff++ = (unsigned int)(first - start);	/* offset to this piece */
		if (pcoff == pcoffmax)
			*pcoff++ = (unsigned int)(last - start);	/* store start of first piece beyond what is in cache */
	}

	ldst.str.addr = (char *)first;

	/* If we scanned some chars, adjust end pointer and save end of final piece */
	if (spcidx != cpcidx)
	{
		if (pcoff < pcoffmax)
			*pcoff = (unsigned int)(last - start);		/* If not at end of cache, save start of "next" piece */

		--last;					/* Undo bump past last delim or +2 past end char
							   of piece for accurate string len */
		/* Update count of pieces in cache */
		cfnpc->npcs = MIN((cfnpc->npcs + cpcidx - spcidx), FNPC_ELEM_MAX);
		assert(cfnpc->npcs <= FNPC_ELEM_MAX);
		assert(cfnpc->npcs > 0);

		/* If the above loop ended prematurely because we ran out of text, we return null string */
		if (trgpcidx < cpcidx)
			ldst.str.len = INTCAST(last - first);	/* Length of piece we located */
		else
			ldst.str.len = 0;

		INCR_COUNT(pscan, cpcidx - spcidx);	/* Pieces scanned */
	} else
		ldst.str.len  = 0;

	assert(cfnpc->npcs > 0);
	assert(ldst.str.len >= 0 && ldst.str.len <= src->str.len);
	*dst = ldst;
	return;
}
