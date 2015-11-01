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

GBLREF gv_key *gv_currkey;
GBLREF connection_struct *curr_entry;
GBLREF gv_namehead	*gv_target;
GBLREF gd_region	*gv_cur_region;
GBLREF gv_key		*gv_currkey;

LITREF mval		*fndata_table[2][2];

bool gtcmtr_data()
{
	cm_region_list *reg_ref, *gtcm_find_region();
	unsigned char *ptr,regnum;
	unsigned short top,len;
	mval	v;
	int x;

	ptr = curr_entry->clb_ptr->mbf;
	assert(*ptr == CMMS_Q_DATA);
	ptr++;
	len = *((unsigned short *)ptr);
	ptr += sizeof(unsigned short);
	regnum = *ptr++;
	reg_ref = gtcm_find_region(curr_entry, regnum);
	len--; /* subtract size of regnum */
	top = gv_currkey->top;
	memcpy(gv_currkey, ptr, len);
	gv_currkey->top = top;

	gtcm_bind_name(reg_ref->reghead);
	x = 0;
 	if (gv_target->root)
	{	x = gvcst_data();
	}
	v = *fndata_table[x / 10][x & 1];

	ptr = curr_entry->clb_ptr->mbf;
	*ptr++ = CMMS_R_DATA;
	*((unsigned short *) ptr) = sizeof(unsigned char);
	ptr += sizeof(unsigned short);
	*ptr++ = MV_FORCE_INT(&v);
	curr_entry->clb_ptr->cbl = ptr - curr_entry->clb_ptr->mbf;
	return TRUE;
}
