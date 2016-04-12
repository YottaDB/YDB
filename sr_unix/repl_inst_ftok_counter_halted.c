/****************************************************************
 *								*
 * Copyright (c) 2016 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

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
#include "repl_inst_ftok_counter_halted.h"

GBLREF	jnlpool_addrs		jnlpool;

error_def(ERR_NOMORESEMCNT);

/* This function sets the "ftok_counter_halted" field to TRUE in the instance file header and flushes it to disk.
 * Caller could be attached to the journal pool or not. If not, update file header directly. If yes, go through locks.
 */
void	repl_inst_ftok_counter_halted(unix_db_info *udi, char *file_type, repl_inst_hdr *repl_instance)
{
	assert(udi->grabbed_ftok_sem);	/* this ensures we have a lock before we modify the instance file header */
	if (NULL != jnlpool.repl_inst_filehdr)
	{
		assert(!jnlpool.repl_inst_filehdr->ftok_counter_halted);
		jnlpool.repl_inst_filehdr->ftok_counter_halted = TRUE;
		grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, ASSERT_NO_ONLINE_ROLLBACK);
		repl_inst_flush_filehdr();
		rel_lock(jnlpool.jnlpool_dummy_reg);
	} else
	{
		assert(!repl_instance->ftok_counter_halted);
		repl_instance->ftok_counter_halted = TRUE;
		repl_inst_write(udi->fn, (off_t)0, (sm_uc_ptr_t)repl_instance, SIZEOF(repl_inst_hdr));
	}
	/* Ignore any errors while flushing the "halted" value to the file header. The only consequence is other processes
	 * will incur a performance overhead trying to unnecessarily bump the semaphore counter when it is already ERANGE.
	 */
	send_msg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_NOMORESEMCNT, 5, LEN_AND_LIT("ftok"), file_type, LEN_AND_STR(udi->fn));
}
