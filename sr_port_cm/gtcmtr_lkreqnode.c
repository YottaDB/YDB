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
#include "mlkdef.h"
#include "cmidef.h"
#include "hashdef.h"
#include "cmmdef.h"
#include "gtcmtr_lk.h"
#include "gt_timer.h"
#include "gtcmlkdef.h"
#include "gtcml.h"

GBLREF connection_struct *curr_entry;

bool gtcmtr_lkreqnode(void)
{
	unsigned char *ptr;

	ptr = curr_entry->clb_ptr->mbf;
	assert(*ptr == CMMS_L_LKREQNODE);
	ptr++;
	gtcml_lkhold();
	gtcml_lklist();
	return FALSE;
}
