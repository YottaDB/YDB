/****************************************************************
 *								*
 * Copyright (c) 2016-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "repl_instance.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_sem.h"
#include "ftok_sems.h"
#include "repl_inst_ftok_counter_halted.h"

GBLREF	jnlpool_addrs_ptr_t	jnlpool;

error_def(ERR_JNLPOOLSETUP);
error_def(ERR_NOMORESEMCNT);
error_def(ERR_TEXT);

/* This function sets the "ftok_counter_halted" field to TRUE in the journal pool.
 * Caller must be attached to the journal pool and have already gotten the ftok lock on the instance file.
 */
void	repl_inst_ftok_counter_halted(unix_db_info *udi)
{
	assert(udi->grabbed_ftok_sem);	/* this ensures we have a lock before we modify the instance file header */
	if ((NULL != jnlpool) && (NULL != jnlpool->jnlpool_ctl))
	{
		grab_lock(jnlpool->jnlpool_dummy_reg, TRUE, ASSERT_NO_ONLINE_ROLLBACK);
		assert(!jnlpool->jnlpool_ctl->ftok_counter_halted);
		if (!jnlpool->repl_inst_filehdr->qdbrundown)
		{
			rel_lock(jnlpool->jnlpool_dummy_reg);
			if (udi->grabbed_access_sem)
				rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
			udi->grabbed_access_sem = FALSE;
			udi->counter_acc_incremented = FALSE;
			ftok_sem_release(jnlpool->jnlpool_dummy_reg, udi->counter_ftok_incremented, TRUE);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0,
				ERR_TEXT, 2, RTS_ERROR_LITERAL("Error incrementing the ftok semaphore counter"), ERANGE);
		}
		jnlpool->jnlpool_ctl->ftok_counter_halted = TRUE;
		rel_lock(jnlpool->jnlpool_dummy_reg);
	}
	send_msg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_NOMORESEMCNT, 5, LEN_AND_LIT("ftok"), FILE_TYPE_REPLINST, LEN_AND_STR(udi->fn));
}
