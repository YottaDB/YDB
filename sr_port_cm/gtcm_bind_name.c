/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"		/* needed for "jnl.h" and others */
#include "cmidef.h"
#include "hashtab_mname.h"
#include "cmmdef.h"
#include "gtcm_bind_name.h"
#include "gv_xform_key.h"
#include "targ_alloc.h"
#include "gvcst_protos.h"	/* for gvcst_root_search prototype */
#include "gvnh_spanreg.h"
#include "gtmimagename.h"
#include "gv_trigger_common.h"	/* for *HASHT* macros used inside GVNH_REG_INIT macro */
#include "jnl.h"		/* needed for "jgbl" */

#define DIR_ROOT 1

GBLREF gv_key		*gv_currkey;
GBLREF sgmnt_data	*cs_data;
GBLREF gv_namehead	*gv_target;

void gtcm_bind_name(cm_region_head *rh, boolean_t xform)
{
	ht_ent_mname	*tabent;
	mname_entry	 gvent;
	gvnh_reg_t	*gvnh_reg;

	GTCM_CHANGE_REG(rh);	/* sets the global variables gv_cur_region/cs_addrs/cs_data appropriately */
	gvent.var_name.addr = (char *)gv_currkey->base;
	gvent.var_name.len = STRLEN((char *)gv_currkey->base);
	COMPUTE_HASH_MNAME(&gvent);
	if (NULL != (tabent = lookup_hashtab_mname(rh->reg_hash, &gvent)))	/* WARNING ASSIGNMENT */
	{
		gvnh_reg = (gvnh_reg_t *)tabent->value;
		assert(NULL != gvnh_reg);
		gv_target = gvnh_reg->gvt;
	} else
	{
		assert(IS_REG_BG_OR_MM(rh->reg));
		gv_target = targ_alloc(cs_data->max_key_size, &gvent, rh->reg);
		GVNH_REG_INIT(NULL, rh->reg_hash, NULL, gv_target, rh->reg, gvnh_reg, tabent);
	}
	GVCST_ROOT_SEARCH;
	if ((gv_target->collseq || gv_target->nct) && xform)
		gv_xform_key(gv_currkey, FALSE);
	return;
}
