/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2025 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
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
#include "dollar_zlevel.h"

GBLREF symval		*curr_symval;
GBLREF lvzwrite_datablk	*lvzwrite_block;
GBLREF bool		undef_inhibit;
GBLREF zshow_out	*zwr_output;

void lvzwr_fini(zshow_out *out, int t)
{
	int4		size;
	mval 		local;
	mint		level;
	mname_entry	temp_key;
	ht_ent_mname	*tabent;
	mident_fixed	m;

	zwr_output = out;
	assert(lvzwrite_block);

	symval *target_symval = curr_symval;
	level = out->stack_level;
	/* -1 is an alias for $STACK. We need to pass some value as the default in `m_zshow`.
	   That function is executed at compile time, so any value it passes is indistinguishable
	   from a value passed by M code at runtime. */
	if (STACK_LEVEL_MINUS_ONE == level)
		level = dollar_zlevel() - 1;
	else if (level < 0 || level >= dollar_zlevel())
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_ZSHOWSTACKRANGE, 1, level);
	while ((NULL != target_symval->last_tab) && (level < target_symval->stack_level)) {
		target_symval = target_symval->last_tab;
	}

	if (zwr_patrn_mident == lvzwrite_block->zwr_intype)
	{	/* Mident specified for "pattern" (fixed name, no pattern) */
		size = (lvzwrite_block->pat->str.len <= MAX_MIDENT_LEN) ? lvzwrite_block->pat->str.len : MAX_MIDENT_LEN;
		temp_key.var_name = lvzwrite_block->pat->str;
		COMPUTE_HASH_MNAME(&temp_key);
		tabent = lookup_hashtab_mname(&target_symval->h_symtab, &temp_key);
		if (!tabent || (!LV_IS_VAL_DEFINED(tabent->value) && !LV_HAS_CHILD(tabent->value)))
		{
			lvzwrite_block->subsc_count = 0;
			if (!undef_inhibit)
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_LVUNDEF, 2, size, lvzwrite_block->pat->str.addr);
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
				if (NULL != (tabent = lookup_hashtab_mname(&target_symval->h_symtab, &temp_key)))
				{
					lvzwrite_block->curr_name = &tabent->key.var_name;
					lvzwr_var(((lv_val *)tabent->value), 0);
				}
			}
			op_fnlvname(&local, TRUE, target_symval, &local);
			assert(local.str.len <= MAX_MIDENT_LEN);
			memcpy(&m.c[0], local.str.addr, local.str.len);
			local.str.addr = &m.c[0];
		}
	}
	lvzwrite_block->curr_subsc = lvzwrite_block->subsc_count = 0;
	return;
}
