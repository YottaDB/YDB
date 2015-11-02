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

void gvname_env_save(gvname_info *  curr_gvname_info)
{
	DEBUG_ONLY(boolean_t	is_bg_or_mm;)

	DEBUG_ONLY(is_bg_or_mm = (dba_mm == gv_cur_region->dyn.addr->acc_meth || dba_bg == gv_cur_region->dyn.addr->acc_meth);)
	curr_gvname_info->s_gv_target = gv_target;
	curr_gvname_info->s_gv_cur_region = gv_cur_region;
	assert((is_bg_or_mm && cs_addrs->hdr == cs_data) || dba_cm == gv_cur_region->dyn.addr->acc_meth ||
		dba_usr == gv_cur_region->dyn.addr->acc_meth);
	curr_gvname_info->s_cs_addrs = cs_addrs;
	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
	assert(gv_currkey->top <= curr_gvname_info->s_gv_currkey->top);
	curr_gvname_info->s_gv_currkey->end = gv_currkey->end;
	curr_gvname_info->s_gv_currkey->prev = gv_currkey->prev;
	memcpy(curr_gvname_info->s_gv_currkey->base, gv_currkey->base, gv_currkey->end + 1);
	curr_gvname_info->s_sgm_info_ptr = sgm_info_ptr;
	assert((is_bg_or_mm && ((dollar_tlevel && sgm_info_ptr) || (!dollar_tlevel && !sgm_info_ptr))) ||
	       dba_cm == gv_cur_region->dyn.addr->acc_meth || dba_usr == gv_cur_region->dyn.addr->acc_meth);
}
