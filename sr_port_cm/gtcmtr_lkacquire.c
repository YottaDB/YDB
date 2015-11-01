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
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "mlkdef.h"
#include "gtcmtr_protos.h"
#include "gt_timer.h"
#include "gtcmlkdef.h"
#include "gtcml.h"

GBLREF connection_struct *curr_entry;

bool gtcmtr_lkacquire(void)
{
	unsigned char *ptr, return_val, action, incr;
	cm_region_list *reg_walk;

	if (*curr_entry->clb_ptr->mbf == CMMS_L_LKCANCEL)
		return CM_NOOP;

	ptr = curr_entry->clb_ptr->mbf;
	assert(*ptr == CMMS_L_LKACQUIRE || *ptr == CMMS_L_LKCANCEL);

	if (curr_entry->transnum == curr_entry->lk_cancel)
		return_val = gtcml_lkcancel();
	else if (curr_entry->transnum == curr_entry->last_cancelled)
	{ /* LKACQUIRE arrived at the server after the INT CANCEL message for the same lock transaction; discard LKACQUIRE message;
	     no need to respond to stale LKACQUIRE message */
		return CM_READ; /* post a read for future messages from client */
	} else
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
