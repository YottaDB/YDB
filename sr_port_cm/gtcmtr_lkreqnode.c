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

bool gtcmtr_lkreqnode()
{
	unsigned char *ptr;

	ptr = curr_entry->clb_ptr->mbf;
	assert(*ptr == CMMS_L_LKREQNODE);
	ptr++;
	gtcml_lkhold();
	gtcml_lklist();
	return FALSE;
}
