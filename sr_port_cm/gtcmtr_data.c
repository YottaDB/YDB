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
#include "copy.h"
#include "gtm_string.h"
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
#include "gvcst_protos.h"	/* for gvcst_data prototype */
#include "mvalconv.h"
#include "gtcmtr_protos.h"

GBLREF connection_struct *curr_entry;
GBLREF gv_namehead	*gv_target;
GBLREF gd_region	*gv_cur_region;
GBLREF gv_key		*gv_currkey;

LITREF mval		*fndata_table[2][2];

bool gtcmtr_data(void)
{
	cm_region_list *reg_ref;
	unsigned char *ptr,regnum;
	unsigned short top,len;
	mval	v;
	int x;

	ptr = curr_entry->clb_ptr->mbf;
	assert(*ptr == CMMS_Q_DATA);
	ptr++;
	GET_USHORT(len, ptr);
	ptr += SIZEOF(unsigned short);
	regnum = *ptr++;
	reg_ref = gtcm_find_region(curr_entry, regnum);
	len--; /* subtract size of regnum */
	CM_GET_GVCURRKEY(ptr, len);
	gtcm_bind_name(reg_ref->reghead, TRUE);
	x = 0;
 	if (gv_target->root)
		x = gvcst_data();
	v = *fndata_table[x / 10][x & 1];

	ptr = curr_entry->clb_ptr->mbf;
	*ptr++ = CMMS_R_DATA;
	len = SIZEOF(unsigned char);
	PUT_USHORT(ptr, len);
	ptr += SIZEOF(unsigned short);
	*ptr++ = MV_FORCE_INTD(&v);
	curr_entry->clb_ptr->cbl = ptr - curr_entry->clb_ptr->mbf;
	return TRUE;
}
