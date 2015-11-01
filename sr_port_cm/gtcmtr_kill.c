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
#include "copy.h"
#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "cmidef.h"
#include "hashdef.h"
#include "cmmdef.h"
#include "gtcmtr_kill.h"
#include "gtcm_find_region.h"
#include "gtcm_bind_name.h"
#include "gvcst_kill.h"

GBLREF connection_struct *curr_entry;
GBLREF gv_namehead	*gv_target;
GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gv_namehead	*gv_target;
GBLREF gv_key 		*gv_currkey;
GBLREF jnl_process_vector *originator_prc_vec;

bool gtcmtr_kill(void)
{
	cm_region_list	*reg_ref;
	unsigned char	*ptr, regnum;
	unsigned short	len;
	static readonly	gds_file_id file;

	error_def(ERR_DBPRIVERR);

	ptr = curr_entry->clb_ptr->mbf;
	assert(*ptr == CMMS_Q_KILL);
	ptr++;
	GET_SHORT(len, ptr);
	ptr += sizeof(unsigned short);
	regnum = *ptr++;
	reg_ref = gtcm_find_region(curr_entry,regnum);
	len--; /* subtract size of regnum */
	CM_GET_GVCURRKEY(ptr, len);
	gtcm_bind_name(reg_ref->reghead, TRUE);
	if (gv_cur_region->read_only)
		rts_error(VARLSTCNT(4) ERR_DBPRIVERR, 2, DB_LEN_STR(gv_cur_region));
	if (JNL_ENABLED(cs_addrs->hdr))
	{
		originator_prc_vec = curr_entry->pvec;
		cs_addrs->jnl->pini_addr = reg_ref->pini_addr;
	}
	if (gv_target->root)
		gvcst_kill(TRUE);
	if (JNL_ENABLED(cs_addrs->hdr))
		reg_ref->pini_addr = cs_addrs->jnl->pini_addr;  /* In case  journal switch occurred */
	ptr = curr_entry->clb_ptr->mbf;
	*ptr++ = CMMS_R_KILL;
	curr_entry->clb_ptr->cbl = S_HDRSIZE;
	return TRUE;
}
