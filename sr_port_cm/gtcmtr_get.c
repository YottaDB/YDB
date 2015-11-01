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

GBLREF connection_struct *curr_entry;
GBLREF gv_namehead	*gv_target;
GBLREF gv_key		*gv_currkey;

bool gtcmtr_get()
{
	void		gtcm_bind_name();
	bool		gvcst_get();
	unsigned char	*ptr, regnum;
	unsigned short	top, len;
	mval		v;
	cm_region_list	*reg_ref, *gtcm_find_region();

	ptr = curr_entry->clb_ptr->mbf;
	assert(CMMS_Q_GET == *ptr);
	ptr++;
	len = *((unsigned short *)ptr);
	ptr += sizeof(short);
	regnum = *ptr++;
	len--;	/* subtract size of regnum */
	reg_ref = gtcm_find_region(curr_entry, regnum);
	top = gv_currkey->top;
	memcpy(gv_currkey, ptr, len);
	gv_currkey->top = top;
	gtcm_bind_name(reg_ref->reghead);
	ptr = curr_entry->clb_ptr->mbf;
	if (gv_target->root ? gvcst_get(&v) : FALSE)
	{
		*ptr++ = CMMS_R_GET;
		*((unsigned short *)ptr) = v.str.len;
		ptr += sizeof(short);
		memcpy(ptr, v.str.addr, v.str.len);
		ptr += v.str.len;
	} else
		*ptr++ = CMMS_R_UNDEF;
	curr_entry->clb_ptr->cbl = ptr - curr_entry->clb_ptr->mbf;
	return TRUE;
}
