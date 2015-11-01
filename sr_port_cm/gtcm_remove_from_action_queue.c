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

#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "hashdef.h"
#include "cmidef.h"
#include "cmmdef.h"
#include "relqueopi.h"
#include "gtcm_remove_from_action_queue.h"

GBLREF relque			action_que;
GBLREF gd_region		*gv_cur_region, *action_que_dummy_reg;
GBLREF connection_struct	*curr_entry;
GBLREF sgmnt_addrs		*cs_addrs;
GBLREF sgmnt_data_ptr_t		cs_data;

void gtcm_remove_from_action_queue()
{
	DEBUG_ONLY(gd_region	*r_save;)

	VMS_ONLY(assert(lib$ast_in_prog()));
	UNIX_ONLY(DEBUG_ONLY(r_save = gv_cur_region; TP_CHANGE_REG(action_que_dummy_reg);) /* for LOCK_HIST macro */)
	curr_entry = (connection_struct *)REMQHI(&action_que);
	UNIX_ONLY(DEBUG_ONLY(TP_CHANGE_REG(r_save);) /* restore gv_cur_region */)
	if ((connection_struct *)EMPTY_QUEUE != curr_entry && (connection_struct *)INTERLOCK_FAIL != curr_entry)
	{
		switch(*curr_entry->clb_ptr->mbf)
		{
			case CMMS_S_TERMINATE:
			case CMMS_E_TERMINATE:
				break;
			default:
				curr_entry->waiting_in_queue = FALSE;
		}
	}
	return;
}
