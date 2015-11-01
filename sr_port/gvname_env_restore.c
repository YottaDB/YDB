/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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
#include "hashtab.h"
#include "buddy_list.h"
#include "tp.h"
#include "gtm_string.h"
#include "gvname_info.h"

GBLREF gv_key           *gv_currkey;
GBLREF gd_region        *gv_cur_region;
GBLREF gv_namehead      *gv_target;
GBLREF sgmnt_addrs      *cs_addrs;
GBLREF sgmnt_data_ptr_t cs_data;
GBLREF short            dollar_tlevel;
GBLREF sgm_info         *sgm_info_ptr;


void gvname_env_restore(gvname_info *curr_gvname_info)
{
	gv_target = curr_gvname_info->s_gv_target;
	gv_cur_region = curr_gvname_info->s_gv_cur_region;
	cs_addrs = curr_gvname_info->s_cs_addrs;
	cs_data = cs_addrs->hdr;
	assert(gv_currkey->top <= curr_gvname_info->s_gv_currkey->top);
	gv_currkey->end = curr_gvname_info->s_gv_currkey->end;
	gv_currkey->prev = curr_gvname_info->s_gv_currkey->prev;
	memcpy(gv_currkey->base, curr_gvname_info->s_gv_currkey->base, curr_gvname_info->s_gv_currkey->end + 1);
	sgm_info_ptr = curr_gvname_info->s_sgm_info_ptr;
	assert((!dollar_tlevel || sgm_info_ptr) || (dollar_tlevel || !sgm_info_ptr));

}
