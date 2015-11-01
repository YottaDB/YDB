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

#include "hashdef.h"
#include "lv_val.h"
#include "mv_stent.h"
#include "mlkdef.h"
#include "zshow.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "zwrite.h"
#include "op.h"
#include "patcode.h"

GBLREF symval *curr_symval;
GBLREF lvzwrite_struct lvzwrite_block;
GBLREF zshow_out	*zwr_output;

void lvzwr_fini(zshow_out *out,int t)
{
	error_def(ERR_UNDEF);
	ht_entry *q;
	mident	m;
	int4	size;
	mval local;

	zwr_output = out;
	if (!lvzwrite_block.name_type)
	{	size = (lvzwrite_block.pat->str.len <= sizeof(mident)) ? lvzwrite_block.pat->str.len : sizeof(mident);
		memcpy(&m.c[0], lvzwrite_block.pat->str.addr, size);
		memset(&m.c[size], 0, sizeof(mident) - size);
		if (!(q = ht_get(&curr_symval->h_symtab , (mname *)&m)) ||
			 (!MV_DEFINED(&((lv_val *)q->ptr)->v)) && !((lv_val *)q->ptr)->ptrs.val_ent.children)
		{
			lvzwrite_block.subsc_count = 0;
			rts_error(VARLSTCNT(4) ERR_UNDEF, 2, size, lvzwrite_block.pat->str.addr);
		}
		else
		{	lvzwrite_block.curr_name = (mident *)&q->nb;
			lvzwrite_block.fixed = (lvzwrite_block.fixed ? TRUE : FALSE);
			lvzwr_var(((lv_val *) q->ptr), 0);
		}
	} else
	{
		local.mvtype = MV_STR;
		local.str.addr = &m.c[0];
		local.str.len = 1;
		memset(&m.c[0], 0, sizeof(mident));
		m.c[0] = '%';

		lvzwrite_block.fixed = FALSE;
		while (local.str.len)
		{
			if (do_pattern(&local, lvzwrite_block.pat))
			{
				memset(&m.c[local.str.len], 0,sizeof(mident) - local.str.len);
				if ((q = ht_get(&curr_symval->h_symtab , (mname *)&m)))
				{
					lvzwrite_block.curr_name = (mident *)&q->nb;
					lvzwr_var(((lv_val *) q->ptr), 0);
				}
			}
			op_fnlvname(&local,&local);
			assert(local.str.len <= sizeof(mident));
			memcpy(&m.c[0],local.str.addr,local.str.len);
			local.str.addr = &m.c[0];
		}
	}
	lvzwrite_block.curr_subsc = lvzwrite_block.subsc_count = 0;
	return;
}
