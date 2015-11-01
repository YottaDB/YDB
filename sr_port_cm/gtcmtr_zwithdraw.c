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
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cmidef.h"
#include "cmmdef.h"

GBLREF connection_struct	*curr_entry;
GBLREF gv_key			*gv_currkey;
GBLREF gv_namehead		*gv_target;

bool gtcmtr_zwithdraw()
{
	cm_region_list	*reg_ref, *gtcm_find_region();
	unsigned char	*ptr, regnum;
	unsigned short	top, len;

	ptr = curr_entry->clb_ptr->mbf;
	assert(*ptr == CMMS_Q_ZWITHDRAW);
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
	if (gv_target->root)
		gvcst_kill(FALSE);
	ptr = curr_entry->clb_ptr->mbf;
	*ptr++ = CMMS_R_ZWITHDRAW;
	curr_entry->clb_ptr->cbl = S_HDRSIZE;
	return TRUE;
}
