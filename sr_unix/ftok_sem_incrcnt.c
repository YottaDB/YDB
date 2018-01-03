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

#include <sys/sem.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_instance.h"
#include "repl_inst_ftok_counter_halted.h"
#include "ftok_sem_incrcnt.h"
#include "gtm_semutils.h"
#include "eintr_wrapper_semop.h"
#include "gtmmsg.h"

GBLREF	gd_region		*ftok_sem_reg;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	boolean_t		is_src_server;
GBLREF	boolean_t		is_rcvr_server;

error_def(ERR_CRITSEMFAIL);

/*
 * Description:
 * 	Assumes that ftok semaphore id already exists. Increment only the COUNTER SEMAPHORE in that semaphore set.
 * Parameters:
 *	reg		: Regions structure
 * Return Value: TRUE, if succsessful. *ftok_counter_halted contains whether counter increment happened or not
 *               FALSE, if fails.
 */
boolean_t ftok_sem_incrcnt(gd_region *reg, const char *file_type_str, boolean_t *ftok_counter_halted)
{
	int			save_errno, status;
	unix_db_info		*udi;
	sgmnt_addrs		*csa;
	struct sembuf		ftok_sop;
	repl_inst_hdr		repl_instance;
	boolean_t		issue_error;

	assert(NULL != reg);
	assert(NULL == ftok_sem_reg);	/* assert that we never hold more than one FTOK semaphore at any point in time */
	/* For now, the only callers to "ftok_sem_incrcnt" are for the replication instance file and not for the database.
	 * Assert this as it is relied upon by the "ERANGE" code below.
	 */
	assert(!MEMCMP_LIT(file_type_str, FILE_TYPE_REPLINST));
	assert((NULL != jnlpool) && (reg == jnlpool->jnlpool_dummy_reg));	/* this is assumed by the code below */
	udi = FILE_INFO(reg);
	csa = &udi->s_addrs;
	assert(!csa->now_crit);
	assert(INVALID_SEMID != udi->ftok_semid);
	ftok_sop.sem_num = DB_COUNTER_SEM;
	ftok_sop.sem_op = DB_COUNTER_SEM_INCR; /* increment counter */
	ftok_sop.sem_flg = SEM_UNDO;
	SEMOP(udi->ftok_semid, (&ftok_sop), 1, status, NO_WAIT);
	if (-1 == status)	/* We couldn't increment it in one shot -- see if we already have it */
	{
		save_errno = errno;
		udi->counter_ftok_incremented = FALSE;
		issue_error = TRUE;
		if (ERANGE == save_errno)
		{	/* "repl_inst_read" and "repl_inst_write" (invoked from "repl_inst_ftok_counter_halted")
			 * rely on the caller holding the ftok semaphore. Although we dont hold it in this case,
			 * if we are the source server (is_src_server) our caller (gtmsource.c) would have ensured
			 * there is a parent pid that is holding the ftok and waiting for us to finish this counter
			 * increment. Therefore steal the ftok semaphore temporarily for the assert.  If we are the
			 * receiver server (is_rcvr_server) our caller (gtmrecv.c) does not ensure this so grab the
			 * ftok in that case. Those are the only two possibilities for the caller as asserted below.
			 */
			assert(is_src_server || is_rcvr_server);
			assert(!udi->grabbed_ftok_sem);
			DEBUG_ONLY(if (is_src_server) udi->grabbed_ftok_sem = TRUE;)
			if (is_rcvr_server)
				repl_inst_ftok_sem_lock();
			repl_inst_read(udi->fn, (off_t)0, (sm_uc_ptr_t)&repl_instance, SIZEOF(repl_inst_hdr));
			if (repl_instance.qdbrundown)
			{
				issue_error = FALSE;
				if (!jnlpool->jnlpool_ctl->ftok_counter_halted)
					repl_inst_ftok_counter_halted(udi);
			}
			if (is_rcvr_server)
				repl_inst_ftok_sem_release();
			DEBUG_ONLY(if (is_src_server) udi->grabbed_ftok_sem = FALSE;)
		}
		if (issue_error)
		{
			gtm_putmsg_csa(CSA_ARG(REG2CSA(reg)) VARLSTCNT(4) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg));
			gtm_putmsg_csa(CSA_ARG(REG2CSA(reg)) VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("semop"),
													CALLFROM, save_errno);
			*ftok_counter_halted = FALSE;
			return FALSE;
		}
	} else
		udi->counter_ftok_incremented = TRUE;
	*ftok_counter_halted = !udi->counter_ftok_incremented;
	return TRUE;
}
