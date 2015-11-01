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
#include "cmidef.h"
#include "cmmdef.h"

GBLREF connection_struct *curr_entry;

bool gtcmtr_lkacquire()
{
	unsigned char *ptr, return_val, action, incr;
	cm_region_list *reg_walk;

	if (*curr_entry->clb_ptr->mbf == CMMS_L_LKCANCEL)
		return CM_NOOP;

	ptr = curr_entry->clb_ptr->mbf;
	assert(*ptr == CMMS_L_LKACQUIRE || *ptr == CMMS_L_LKCANCEL);

	if (curr_entry->transnum == curr_entry->lk_cancel)
		return_val = gtcml_lkcancel();
	else
	{
		curr_entry->state |= CMMS_L_LKACQUIRE;
		return_val = gtcml_dolock();

		if ((curr_entry->transnum == curr_entry->lk_cancel) ||
			(return_val == CMMS_M_LKBLOCKED) ||
			(return_val == CMLCK_REQUEUE))
			return CM_NOOP;
	}

	curr_entry->state &= ~CMMS_L_LKACQUIRE;
	curr_entry->clb_ptr->cbl  = 1;
	if (*ptr == CMMS_L_LKACQUIRE)
		*ptr = return_val;
	return CM_WRITE;
}
