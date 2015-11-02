/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include "filestruct.h"
#include "cmidef.h"
#include "hashtab_mname.h"
#include "cmmdef.h"
#include "gtcm_bind_name.h"
#include "gv_xform_key.h"
#include "targ_alloc.h"
#include "gvcst_protos.h"	/* for gvcst_root_search prototype */

#define DIR_ROOT 1

GBLREF gv_key		*gv_currkey;
GBLREF sgmnt_data	*cs_data;
GBLREF gv_namehead	*gv_target;

void gtcm_bind_name(cm_region_head *rh, boolean_t xform)
{
	ht_ent_mname	*tabent;
	mname_entry	 gvent;
	boolean_t	added;
	gvnh_reg_t	*gvnh_reg;

	GTCM_CHANGE_REG(rh);	/* sets the global variables gv_cur_region/cs_addrs/cs_data appropriately */
	gvent.var_name.addr = (char *)gv_currkey->base;
	gvent.var_name.len = STRLEN((char *)gv_currkey->base);
	COMPUTE_HASH_MNAME(&gvent);
	if (NULL == (tabent = lookup_hashtab_mname(rh->reg_hash, &gvent)) || NULL == (gvnh_reg = (gvnh_reg_t *)tabent->value))
	{
		gv_target = targ_alloc(cs_data->max_key_size, &gvent, rh->reg);
		gvnh_reg = (gvnh_reg_t *)malloc(SIZEOF(gvnh_reg_t));
		gvnh_reg->gvt = gv_target;
		gvnh_reg->gd_reg = rh->reg;
		if (NULL != tabent)
		{ 	/* Since the global name was found but gv_target was null and now we created a new gv_target,
			 * the hash table key must point to the newly created gv_target->gvname. */
			tabent->key = gv_target->gvname;
			tabent->value = (char *)gvnh_reg;
		} else
		{
			added = add_hashtab_mname((hash_table_mname *)rh->reg_hash, &gv_target->gvname, gvnh_reg, &tabent);
			assert(added);
		}
	} else
		gv_target = gvnh_reg->gvt;
	GVCST_ROOT_SEARCH;
	if ((gv_target->collseq || gv_target->nct) && xform)
		gv_xform_key(gv_currkey, FALSE);
	return;
}
