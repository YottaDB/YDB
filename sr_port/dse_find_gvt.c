/****************************************************************
 *								*
 * Copyright (c) 2013-2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "dse.h"
#include "min_max.h"
#include "targ_alloc.h"
#include "hashtab_umname.h"
#include "stringpool.h"
#include "mdq.h"

GBLREF gd_addr	*original_header;

gv_namehead *dse_find_gvt(gd_region *reg, char *name, int name_len)
{
	boolean_t		added;
	gd_gblname		*gname;
	gv_namehead		*gvt;
	hash_table_umname	*gvt_hashtab;
	ht_ent_umname		*tabent;
	unmanaged_mname_entry	gvent = {0};
	sgmnt_addrs		*csa;

	assert(reg->open);
	assert(IS_REG_BG_OR_MM(reg));
	csa = &FILE_INFO(reg)->s_addrs;
	gvt_hashtab = (hash_table_umname *)csa->miscptr;
	if (NULL == gvt_hashtab)
	{
		gvt_hashtab = (hash_table_umname *)malloc(SIZEOF(hash_table_umname));
		csa->miscptr = (void *)gvt_hashtab;
		init_hashtab_umname(gvt_hashtab, 0, HASHTAB_NO_COMPACT, HASHTAB_NO_SPARE_TABLE);
	}
	gvent.var_name.addr = name;
	gvent.var_name.len = MIN(name_len, MAX_MIDENT_LEN);
	COMPUTE_HASH_MNAME(&gvent);
	assert(!IS_IN_STRINGPOOL(gvent.var_name.addr, gvent.var_name.len));
	if (NULL != (tabent = lookup_hashtab_umname(gvt_hashtab, &gvent)))
		gvt = (gv_namehead *)tabent->value;
	else
	{
		gvt = targ_alloc(reg->max_key_size, &gvent, reg);
		gvent = gvt->gvname;
		added = add_hashtab_umname(gvt_hashtab, &gvent, gvt, &tabent);
		assert(added);
	}
	if (original_header->n_gblnames)
	{	/* If a "collation" sequence is specified for current global name in the GBLNAME section of the .GLD file,
		 * setup gvt with that info */
		gname = gv_srch_gblname(original_header, gvt->gvname.var_name.addr, gvt->gvname.var_name.len);
		if (NULL != gname)
		{
			gvt->act_specified_in_gld = TRUE;
			gvt->act = gname->act;
			gvt->ver = gname->ver;
		}
	}
	return gvt;
}
