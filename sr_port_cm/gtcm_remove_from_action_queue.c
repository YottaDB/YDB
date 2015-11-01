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

#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "relqueopi.h"
#include "gtcm_remove_from_action_queue.h"

GBLREF	relque			action_que;
GBLREF	gd_region		*action_que_dummy_reg;
GBLREF	connection_struct	*curr_entry;
GBLREF	node_local_ptr_t	locknl;

void gtcm_remove_from_action_queue()
{
	VMS_ONLY(assert(lib$ast_in_prog()));
	UNIX_ONLY(DEBUG_ONLY(locknl = FILE_INFO(action_que_dummy_reg)->s_addrs.nl;))	/* for DEBUG_ONLY LOCK_HIST macro */
	curr_entry = (connection_struct *)REMQHI(&action_que);
	UNIX_ONLY(DEBUG_ONLY(locknl = NULL;))	/* restore "locknl" to default value */
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
