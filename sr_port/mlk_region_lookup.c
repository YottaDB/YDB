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
#include "min_max.h"
#include "mlk_region_lookup.h"
#include "targ_alloc.h"
#include "hashtab_mname.h"
#include "gvnh_spanreg.h"
#include "gtmimagename.h"
#include "gv_trigger_common.h"	/* for *HASHT* macros used inside GVNH_REG_INIT macro */
#include "filestruct.h"		/* needed for "jnl.h" used by next line */
#include "jnl.h"		/* needed for "jgbl" used inside GVNH_REG_INIT macro */

#define DIR_ROOT 1

gd_region *mlk_region_lookup(mval *ptr, gd_addr *addr)
{
	ht_ent_mname		*tabent;
	mname_entry		 gvent;
	gd_binding		*map;
	gv_namehead		*targ;
	gd_region		*reg;
	register char		*p;
	int			plen;
	gvnh_reg_t		*gvnh_reg;

	p = ptr->str.addr;
	plen = ptr->str.len;
	if (*p != '^')				/* is local lock */
	{
		reg = addr->maps->reg.addr; 	/* local lock map is first */
		if (!reg->open)
			gv_init_reg (reg, NULL);
	} else
	{
		p++;
		plen--;
		gvent.var_name.addr = p;
		gvent.var_name.len = MIN(plen, MAX_MIDENT_LEN);
		COMPUTE_HASH_MNAME(&gvent);
		if (NULL != (tabent = lookup_hashtab_mname(addr->tab_ptr, &gvent)))
		{
			gvnh_reg = (gvnh_reg_t *)tabent->value;
			assert(NULL != gvnh_reg);
			targ = gvnh_reg->gvt;
			reg = gvnh_reg->gd_reg;
			if (!reg->open)
			{
				targ->clue.end = 0;
				gv_init_reg(reg, NULL);
			}
		} else
		{
			map = gv_srch_map(addr, gvent.var_name.addr, gvent.var_name.len, SKIP_BASEDB_OPEN_FALSE);
			reg = map->reg.addr;
			if (!reg->open)
				gv_init_reg(reg, NULL);
			targ = (gv_namehead *)targ_alloc(reg->max_key_size, &gvent, reg);
			GVNH_REG_INIT(addr, addr->tab_ptr, map, targ, reg, gvnh_reg, tabent);
		}
	}
	return reg;
}
