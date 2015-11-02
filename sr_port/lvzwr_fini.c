/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "hashtab_mname.h"
#include "lv_val.h"
#include "rtnhdr.h"
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
#include "hashtab.h"

GBLREF symval *curr_symval;
GBLREF lvzwrite_struct lvzwrite_block;
GBLREF zshow_out	*zwr_output;

void lvzwr_fini(zshow_out *out,int t)
{
	int4		size;
	mval 		local;
	mname_entry	temp_key;
	ht_ent_mname	*tabent;
	mident_fixed	m;
	error_def(ERR_UNDEF);

	zwr_output = out;
	if (!lvzwrite_block.name_type)
	{
		size = (lvzwrite_block.pat->str.len <= MAX_MIDENT_LEN) ? lvzwrite_block.pat->str.len : MAX_MIDENT_LEN;
		temp_key.var_name = lvzwrite_block.pat->str;
		COMPUTE_HASH_MNAME(&temp_key);
		tabent = lookup_hashtab_mname(&curr_symval->h_symtab, &temp_key);
		if (!tabent || (!MV_DEFINED(&((lv_val *)tabent->value)->v)) && !((lv_val *)tabent->value)->ptrs.val_ent.children)
		{
			lvzwrite_block.subsc_count = 0;
			rts_error(VARLSTCNT(4) ERR_UNDEF, 2, size, lvzwrite_block.pat->str.addr);
		}
		else
		{
			lvzwrite_block.curr_name = &tabent->key.var_name;
			lvzwrite_block.fixed = (lvzwrite_block.fixed ? TRUE : FALSE);
			lvzwr_var(((lv_val *) tabent->value), 0);
		}
	} else
	{
		memset(m.c, 0, sizeof(m.c));
		local.mvtype = MV_STR;
		local.str.addr = &m.c[0];
		local.str.len = 1;
		m.c[0] = '%';

		lvzwrite_block.fixed = FALSE;
		while (local.str.len)
		{
			if (do_pattern(&local, lvzwrite_block.pat))
			{
				memset(&m.c[local.str.len], 0, sizeof(m.c) - local.str.len);
				temp_key.var_name = local.str;
				COMPUTE_HASH_MNAME(&temp_key);
				if (NULL != (tabent = lookup_hashtab_mname(&curr_symval->h_symtab, &temp_key)))
				{
					lvzwrite_block.curr_name = &tabent->key.var_name;
					lvzwr_var(((lv_val *) tabent->value), 0);
				}
			}
			op_fnlvname(&local, &local);
			assert(local.str.len <= MAX_MIDENT_LEN);
			memcpy(&m.c[0], local.str.addr, local.str.len);
			local.str.addr = &m.c[0];
		}
	}
	lvzwrite_block.curr_subsc = lvzwrite_block.subsc_count = 0;
	return;
}
