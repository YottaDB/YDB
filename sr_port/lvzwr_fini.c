/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "lv_val.h"
#include <rtnhdr.h>
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

GBLREF symval		*curr_symval;
GBLREF lvzwrite_datablk	*lvzwrite_block;
GBLREF zshow_out	*zwr_output;

error_def(ERR_UNDEF);

void lvzwr_fini(zshow_out *out, int t)
{
	int4		size;
	mval 		local;
	mname_entry	temp_key;
	ht_ent_mname	*tabent;
	mident_fixed	m;

	zwr_output = out;
	assert(lvzwrite_block);
	if (zwr_patrn_mident == lvzwrite_block->zwr_intype)
	{	/* Mident specified for "pattern" (fixed name, no pattern) */
		size = (lvzwrite_block->pat->str.len <= MAX_MIDENT_LEN) ? lvzwrite_block->pat->str.len : MAX_MIDENT_LEN;
		temp_key.var_name = lvzwrite_block->pat->str;
		COMPUTE_HASH_MNAME(&temp_key);
		tabent = lookup_hashtab_mname(&curr_symval->h_symtab, &temp_key);
		if (!tabent || !LV_IS_VAL_DEFINED(tabent->value) && !LV_HAS_CHILD(tabent->value))
		{
			lvzwrite_block->subsc_count = 0;
			rts_error(VARLSTCNT(4) ERR_UNDEF, 2, size, lvzwrite_block->pat->str.addr);
		} else
		{
			lvzwrite_block->curr_name = &tabent->key.var_name;
			lvzwr_var(((lv_val *)tabent->value), 0);
		}
	} else
	{	/* mval specified for character "pattern" (pattern matching) */
		assert(zwr_patrn_mval == lvzwrite_block->zwr_intype);
		memset(m.c, 0, SIZEOF(m.c));
		local.mvtype = MV_STR;
		local.str.addr = &m.c[0];
		local.str.len = 1;
		m.c[0] = '%';	/* Starting variable name for search (first possible varname) */

		lvzwrite_block->fixed = FALSE;
		while (local.str.len)
		{
			if (do_pattern(&local, lvzwrite_block->pat))
			{
				memset(&m.c[local.str.len], 0, SIZEOF(m.c) - local.str.len);
				temp_key.var_name = local.str;
				COMPUTE_HASH_MNAME(&temp_key);
				if (NULL != (tabent = lookup_hashtab_mname(&curr_symval->h_symtab, &temp_key)))
				{
					lvzwrite_block->curr_name = &tabent->key.var_name;
					lvzwr_var(((lv_val *)tabent->value), 0);
				}
			}
			op_fnlvname(&local, TRUE, &local);
			assert(local.str.len <= MAX_MIDENT_LEN);
			memcpy(&m.c[0], local.str.addr, local.str.len);
			local.str.addr = &m.c[0];
		}
	}
	lvzwrite_block->curr_subsc = lvzwrite_block->subsc_count = 0;
	return;
}
