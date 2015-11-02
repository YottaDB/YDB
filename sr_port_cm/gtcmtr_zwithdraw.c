/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_string.h"
#include "copy.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gtcm_find_region.h"
#include "gtcm_bind_name.h"
#include "gvcst_protos.h"	/* for gvcst_kill prototype */
#include "gtcmtr_protos.h"

GBLREF connection_struct	*curr_entry;
GBLREF gv_key			*gv_currkey;
GBLREF gv_namehead		*gv_target;
GBLREF gd_region		*gv_cur_region;

bool gtcmtr_zwithdraw(void)
{
	cm_region_list	*reg_ref;
	unsigned char	*ptr, regnum;
	unsigned short	top, len;

	error_def(ERR_DBPRIVERR);

	ptr = curr_entry->clb_ptr->mbf;
	assert(*ptr == CMMS_Q_ZWITHDRAW);
	ptr++;
	GET_USHORT(len, ptr);
	ptr += SIZEOF(unsigned short);
	regnum = *ptr++;
	reg_ref = gtcm_find_region(curr_entry,regnum);
	len--; /* subtract size of regnum */
	CM_GET_GVCURRKEY(ptr, len);
	gtcm_bind_name(reg_ref->reghead, TRUE);
	if (gv_cur_region->read_only)
		rts_error(VARLSTCNT(4) ERR_DBPRIVERR, 2, DB_LEN_STR(gv_cur_region));
	if (gv_target->root)
		gvcst_kill(FALSE);
	ptr = curr_entry->clb_ptr->mbf;
	*ptr++ = CMMS_R_ZWITHDRAW;
	curr_entry->clb_ptr->cbl = S_HDRSIZE;
	return TRUE;
}
