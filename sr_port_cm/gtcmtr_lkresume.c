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
#include "mlkdef.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gtcmtr_protos.h"
#include "gt_timer.h"
#include "gtcmlkdef.h"
#include "gtcml.h"

GBLREF connection_struct *curr_entry;

bool gtcmtr_lkresume(void)
{
	unsigned char *ptr, return_val;
	cm_region_list *reg_walk;

	ptr = curr_entry->clb_ptr->mbf;
	assert(*ptr == CMMS_L_LKRESUME);
	ptr++;
	ptr++;
	return_val = CMMS_M_LKGRANTED;

	return_val = gtcml_dolock();
	if (return_val == CMLCK_REQUEUE)
		return CM_NOOP;

	*curr_entry->clb_ptr->mbf = return_val;
	curr_entry->clb_ptr->cbl  = 1;
	return TRUE;
}
