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

#include <stddef.h>		/* for offsetof macro in VMS */

#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "copy.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "gtm_string.h"
#include "gvname_info.h"

GBLREF gv_key           *gv_currkey;
GBLREF gd_region        *gv_cur_region;
GBLREF gv_namehead      *gv_target;
GBLREF sgmnt_addrs      *cs_addrs;
GBLREF sgmnt_data_ptr_t cs_data;
GBLREF uint4		dollar_tlevel;
GBLREF sgm_info         *sgm_info_ptr;

void gvname_env_save(gvname_info *curr_gvname_info)
{
	DEBUG_ONLY(boolean_t	is_bg_or_mm;)
	DEBUG_ONLY(gd_addr	*addr;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	DEBUG_ONLY(is_bg_or_mm = IS_REG_BG_OR_MM(gv_cur_region);)
	curr_gvname_info->s_gv_target = gv_target;
	curr_gvname_info->s_gv_cur_region = gv_cur_region;
	assert((is_bg_or_mm && cs_addrs->hdr == cs_data)
		|| (dba_cm == REG_ACC_METH(gv_cur_region)) || (dba_usr == REG_ACC_METH(gv_cur_region)));
	curr_gvname_info->s_cs_addrs = cs_addrs;
	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
	COPY_KEY(curr_gvname_info->s_gv_currkey, gv_currkey);
	curr_gvname_info->s_sgm_info_ptr = sgm_info_ptr;
	assert((is_bg_or_mm && ((dollar_tlevel && sgm_info_ptr) || (!dollar_tlevel && !sgm_info_ptr)))
		|| (dba_cm == REG_ACC_METH(gv_cur_region)) || (dba_usr == REG_ACC_METH(gv_cur_region)));
	DEBUG_ONLY(addr = TREF(gd_targ_addr);)
	assert((gv_cur_region >= &addr->regions[0]) && (gv_cur_region < &addr->regions[addr->n_regions]));
	curr_gvname_info->s_gd_targ_gvnh_reg = TREF(gd_targ_gvnh_reg);
	curr_gvname_info->s_gd_targ_map = TREF(gd_targ_map);
	curr_gvname_info->s_gd_targ_addr = TREF(gd_targ_addr);
}
