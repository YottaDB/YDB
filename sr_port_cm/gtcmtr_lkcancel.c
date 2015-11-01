/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
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
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "mlkdef.h"
#include "gtcmtr_protos.h"
#include "gt_timer.h"
#include "gtcmlkdef.h"
#include "gtcml.h"

GBLREF connection_struct *curr_entry;

bool gtcmtr_lkcancel(void)
{
	cm_region_list *reg_walk;
	unsigned char *ptr,action, return_val,incr;
	unsigned short transnum;

	ptr = curr_entry->clb_ptr->mbf;
	assert(*ptr == CMMS_L_LKCANCEL);
	ptr++;
	ptr++;
	transnum = *ptr++;
	assert(curr_entry->transnum == transnum);

	return_val = gtcml_lkcancel();

	*curr_entry->clb_ptr->mbf = return_val;
	curr_entry->clb_ptr->cbl = 1;
	return TRUE;
}
