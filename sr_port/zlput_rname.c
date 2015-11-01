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
#include "gtm_string.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "hashdef.h"
#include "fix_pages.h"

#define S_CUTOFF 7
#define FREE_RTNTBL_SPACE 17

GBLREF rtn_tables *rtn_fst_table,*rtn_names,*rtn_names_end,*rtn_names_top;
GBLREF stack_frame *frame_pointer;
GBLREF htab_desc rt_name_tbl;	/* globally defined routine table name */

bool zlput_rname (rhdtyp *hdr)
{
	rhdtyp		*old_rhead;
	rtn_tables	*rbot, *mid, *rtop;
	stack_frame	*fp;
	char		*name, *src, *new, *old_table;
	int		src_len, comp;
	ht_entry	*ht;
	int		size;

	name = &hdr->routine_name.c[0];
	rbot = rtn_names;
	rtop = rtn_names_end;
	for (;;)
	{
		if ((char *) rtop - (char *) rbot < S_CUTOFF * sizeof(rtn_tables))
		{	comp = -1;
			for (mid = rbot; mid <= rtop ;mid++)
			{
				comp = memcmp(mid->rt_name.c,name,sizeof(mident));
				if (comp >= 0)
					break;
			}
			break;
		}
		else
		{	mid = rbot + (rtop - rbot)/2;
			comp = memcmp(mid->rt_name.c,name,sizeof(mident));
			if (!comp)
				break;
			else if (comp < 0)
			{	rbot = mid + 1;
				continue;
			}
			else
			{	rtop = mid - 1;
				continue;
			}
		}
	}
	if (comp)
	{
		old_table = (char *)0;
		src = (char *) mid;
		src_len = (char *) rtn_names_end - (char *) mid + sizeof(rtn_tables);
		if (rtn_names_end >= rtn_names_top)
		{	size = (char *) rtn_names_end - (char *) rtn_names + sizeof(rtn_tables)
				* FREE_RTNTBL_SPACE;
			new = malloc(size);
			memcpy(new,rtn_names,(char *) mid - (char *) rtn_names);
			mid = (rtn_tables*)((char *) mid + (new - (char *) rtn_names));
			old_table = (char *) rtn_names;
			rtn_names_end = (rtn_tables*)((char *) rtn_names_end + (new - (char *) rtn_names));
			rtn_names = (rtn_tables *)new;
			rtn_names_top = (rtn_tables *)(new + size - sizeof(rtn_tables));
			memset(rtn_names_end + 1,0,size - ((char *) (rtn_names_end + 1) - new));
		}
		memmove(mid + 1,src,src_len);
		memmove(mid->rt_name.c,name,sizeof(mident));
		rtn_names_end++;
		if (old_table && old_table != (char *)rtn_fst_table)
			free(old_table);		/* original table can't be freed */
	}
	else
	{
		old_rhead = (rhdtyp *) mid->rt_ptr;
		for (fp = frame_pointer; fp ; fp = fp->old_frame_pointer)
			if (fp->rvector == old_rhead)
				return FALSE;

		hdr->old_rhead_ptr = (int4) old_rhead;
		if (!old_rhead->old_rhead_ptr)
			fix_pages((unsigned char *)old_rhead, (unsigned char *)old_rhead
				+ old_rhead->lnrtab_ptr + sizeof(int4)
				* old_rhead->lnrtab_len);

		if (rt_name_tbl.base)		 /* if source has been read in for old module, free space */
			if ((ht = ht_get (&rt_name_tbl, (mname *)&mid->rt_name.c[0])) != 0 && ht->ptr)
			{	free(ht->ptr);
				ht->ptr = 0;
			}
	}
	mid->rt_ptr = (unsigned char *) hdr;
	return TRUE;
}
