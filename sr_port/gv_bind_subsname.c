/****************************************************************
 *								*
 * Copyright (c) 2013-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"

#include "gtmio.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "targ_alloc.h"
#include "change_reg.h"
#include "gvcst_protos.h"	/* for gvcst_root_search prototype used in GV_BIND_SUBSREG macro */
#include "gtmimagename.h"
#include "io.h"

GBLREF gd_region	*gv_cur_region;
GBLREF gv_key		*gv_currkey;

/* Like gv_bind_name but this operates on a subscripted global reference. In addition, this does a gvcst_root_search too. */
void gv_bind_subsname(gd_addr *addr, gv_key *key, gvnh_reg_t *gvnh_reg)
{
	gd_binding	*map;
	gd_region	*reg;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	map = gv_srch_map(addr, (char *)&key->base[0], key->end - 1, SKIP_BASEDB_OPEN_FALSE);
	TREF(gd_targ_map) = map;
	reg = map->reg.addr;
	GV_BIND_SUBSREG(addr, reg, gvnh_reg);	/* sets gv_target/gv_cur_region/cs_addrs */
	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
}
