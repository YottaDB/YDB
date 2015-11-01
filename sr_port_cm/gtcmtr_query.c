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
GBLREF gv_key		*gv_altkey;

bool gtcmtr_query()
{
	bool		gvcst_query();
	unsigned char	*ptr, regnum;
	unsigned short	top, old_top;
	unsigned int	len;
	cm_region_list	*reg_ref, *gtcm_find_region();

	ptr = curr_entry->clb_ptr->mbf;
	assert(CMMS_Q_QUERY == *ptr);
	ptr++;
	len = (unsigned int)*((unsigned short *)ptr);
	ptr += sizeof(short);
	regnum = *ptr++;
	reg_ref = gtcm_find_region(curr_entry, regnum);
	len--;	/* subtract size of regnum */
	old_top = ((gv_key *)ptr)->top;
	top = gv_currkey->top;
	memcpy(gv_currkey, ptr, len);
	gv_currkey->top = top;
	gtcm_bind_name(reg_ref->reghead);
	len = (NULL == gv_target->root) ? 0 : gvcst_query();
	if (len)
	{
		if (gv_target->nct || gv_target->collseq)
			gv_xform_key(gv_altkey, TRUE);
		len = sizeof(gv_key) + gv_altkey->end;
	}
	ptr = curr_entry->clb_ptr->mbf;
	*ptr++ = CMMS_R_QUERY;
	*((unsigned short *)ptr) = (unsigned short)(len + 1);
	ptr += sizeof(short);
	*ptr++ = regnum;
	if (len)
		memcpy(ptr, gv_altkey, len);
	((gv_key *)ptr)->top = old_top;
	curr_entry->clb_ptr->cbl = sizeof(unsigned char) + sizeof(unsigned short) + sizeof(unsigned char) + len;
	return TRUE;
}
