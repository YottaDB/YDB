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
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "cmidef.h"
#include "cmmdef.h"
#include "gtcmtr_kill.h"

GBLREF connection_struct *curr_entry;
GBLREF gv_namehead	*gv_target;
GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gv_namehead	*gv_target;
GBLREF gv_key 		*gv_currkey;
GBLREF jnl_process_vector *prc_vec;

bool gtcmtr_kill(void)
{
	cm_region_list *reg_ref, *gtcm_find_region();
	unsigned char *ptr,regnum;
	unsigned short top,len;

	ptr = curr_entry->clb_ptr->mbf;
	assert(*ptr == CMMS_Q_KILL);
	ptr++;
	len = *((unsigned short *)ptr);
	ptr += sizeof(unsigned short);
	regnum = *ptr++;
	reg_ref = gtcm_find_region(curr_entry,regnum);
	len--; /* subtract size of regnum */
	top = gv_currkey->top;
	memcpy(gv_currkey, ptr, len);
	gv_currkey->top = top;

	gtcm_bind_name(reg_ref->reghead);
	if (JNL_ENABLED(cs_addrs->hdr))
	{
		prc_vec = curr_entry->pvec;
		if (!reg_ref->pini_addr)
		{
			cs_addrs->jnl->pini_addr = 0;
			grab_crit(gv_cur_region);
			jnl_put_jrt_pini(cs_addrs);
			reg_ref->pini_addr = cs_addrs->jnl->pini_addr;
			rel_crit(gv_cur_region);
		}
		cs_addrs->jnl->pini_addr = reg_ref->pini_addr;
	}

	if (gv_target->root)
	{	gvcst_kill(TRUE);
	}
	ptr = curr_entry->clb_ptr->mbf;
	*ptr++ = CMMS_R_KILL;
	curr_entry->clb_ptr->cbl = S_HDRSIZE;
	return TRUE;
}
