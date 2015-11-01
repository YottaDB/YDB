/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_string.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "hashdef.h"
#include "fix_pages.h"
#include "zbreak.h"
#include "private_code_copy.h"
#include "urx.h"

#define S_CUTOFF 7
#define FREE_RTNTBL_SPACE 17

GBLREF RTN_TABENT	*rtn_fst_table,*rtn_names,*rtn_names_end,*rtn_names_top;
GBLREF stack_frame	*frame_pointer;
GBLREF htab_desc	rt_name_tbl;	/* globally defined routine table name */

bool zlput_rname (rhdtyp *hdr)
{
	rhdtyp		*old_rhead;
	RTN_TABENT	*rbot, *mid, *rtop;
	stack_frame	*fp;
	char		*name, *src, *new, *old_table;
	int		src_len, comp;
	ht_entry	*ht;
	uint4		*src_tbl, entries;
	mstr		*curline;
	int		size;

	name = &hdr->routine_name.c[0];
	rbot = rtn_names;
	rtop = rtn_names_end;
	for (;;)
	{	/* See if routine exists in list via a binary search which reverts to serial
		   search when # of items drops below the threshold S_CUTOFF.
		*/
		if ((char *) rtop - (char *) rbot < S_CUTOFF * sizeof(RTN_TABENT))
		{	comp = -1;
			for (mid = rbot; mid <= rtop ;mid++)
			{
				comp = memcmp(mid->rt_name.c,name,sizeof(mident));
				if (comp >= 0)
					break;
			}
			break;
		} else
		{	mid = rbot + (rtop - rbot)/2;
			comp = memcmp(mid->rt_name.c,name,sizeof(mident));
			if (!comp)
				break;
			else if (comp < 0)
			{	rbot = mid + 1;
				continue;
			} else
			{	rtop = mid - 1;
				continue;
			}
		}
	}
	if (comp)
	{	/* Entry was not found. Add in a new one */
		old_table = (char *)0;
		src = (char *) mid;
		src_len = (char *) rtn_names_end - (char *) mid + sizeof(RTN_TABENT);
		if (rtn_names_end >= rtn_names_top)
		{	/* Not enough room, recreate table in larger area */
			size = (char *) rtn_names_end - (char *) rtn_names + sizeof(RTN_TABENT)
				* FREE_RTNTBL_SPACE;
			new = malloc(size);
			memcpy(new, rtn_names, (char *)mid - (char *)rtn_names);
			mid = (RTN_TABENT *)((char *)mid + (new - (char *)rtn_names));
			old_table = (char *) rtn_names;
			rtn_names_end = (RTN_TABENT *)((char *)rtn_names_end + (new - (char *)rtn_names));
			rtn_names = (RTN_TABENT *)new;
			rtn_names_top = (RTN_TABENT *)(new + size - sizeof(RTN_TABENT));
			memset(rtn_names_end + 1, 0, size - ((char *)(rtn_names_end + 1) - new));
		}
		memmove(mid + 1, src,src_len);
		memmove(mid->rt_name.c, name, sizeof(mident));
		rtn_names_end++;
		if (old_table && old_table != (char *)rtn_fst_table)
			free(old_table);		/* original table can't be freed */
	} else
	{	/* Entry exists. Update it */
		old_rhead = (rhdtyp *) mid->RTNENT_RT_ADR;
		/* Verify routine is not currently active. If it is, we cannot replace it */
		for (fp = frame_pointer; fp ; fp = fp->old_frame_pointer)
			if (fp->rvector == old_rhead)
				return FALSE;
		zr_remove(old_rhead); /* get rid of the now inactive breakpoints and release any private code section */
		NON_USHBIN_ONLY(
			hdr->old_rhead_ptr = (int4)old_rhead;
			if (!old_rhead->old_rhead_ptr)
			{
			        fix_pages((unsigned char *)old_rhead, (unsigned char *)LNRTAB_ADR(old_rhead)
					  + (sizeof(LNR_TABENT) * old_rhead->lnrtab_len));
			}
		)
		USHBIN_ONLY(
			if (!old_rhead->shlib_handle)
		        {	/* Migrate text literals pointing into text area we are about to throw away into the stringpool.
				   We also can release the read-only releasable segment as it is no longer needed.
				*/
			        stp_move(old_rhead->literal_text_adr, old_rhead->literal_text_adr + old_rhead->literal_text_len);
				free(old_rhead->ptext_adr);
			}
			urx_remove(old_rhead->linkage_adr, old_rhead->linkage_len);
			free(old_rhead->literal_adr);	/* Release the read-write releasable segments */
			free(old_rhead->linkage_adr);
			old_rhead->literal_adr = NULL;
			old_rhead->linkage_adr = NULL;
			hdr->old_rhead_adr = old_rhead;
		)
		if (rt_name_tbl.base)		 /* if source has been read in for old module, free space */
		{
			if ((ht = ht_get (&rt_name_tbl, (mname *)&mid->rt_name.c[0])) != 0 && ht->ptr)
			{
				src_tbl = (uint4 *)ht->ptr;
				entries = *(src_tbl + 1);
				if (0 != entries)	/* Don't count line 0 which we bypass */
					entries--;
				/* curline start is 2 uint4s into src_tbl and then space past line 0 or
				   we end up freeing the storage for line 0/1 twice since they have the
				   same pointers.
				*/
				for (curline = (mstr *)(src_tbl + 2) + 1; 0 != entries; --entries, ++curline)
				{
					assert(curline->len);
					free(curline->addr);
				}
				free(ht->ptr);
				ht->ptr = 0;
			}
		}
	}
	mid->RTNENT_RT_ADR = hdr;
	return TRUE;
}
