/****************************************************************
 *								*
 * Copyright (c) 2017-2018 YottaDB LLC. and/or its subsidiaries.*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "libyottadb_int.h"
#include "libydberrors.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "dpgbldir.h"	/* for "get_next_gdr" prototype and other data structures */
#include "getjobnum.h"	/* for "getjobnum" prototype */
#include "mutex.h"	/* for "mutex_per_process_init" prototype */

GBLREF	uint4	mutex_per_process_init_pid;

/* Routine that is invoked right after a "fork()" in the child pid.
 * This will do needed setup so the parent and child pids are treated as different pids
 * as far as the YottaDB process state is concerned (database, local variables, etc.)
 *
 * Parameters:
 *   param  - A pointer to a structure that is currently ignored (placeholder for future interface enhancement).
 */
int	ydb_child_init(void *param)
{
	boolean_t		error_encountered;
	gd_addr			*addr_ptr;
	gd_region		*reg_top, *reg;
	unix_db_info		*udi;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	jnl_private_control	*jpc;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Verify entry conditions, make sure YDB CI environment is up etc. */
	LIBYOTTADB_INIT(LYDB_RTN_CHILDINIT);	/* Note: macro could "return" from this function in case of errors */
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{
		REVERT;
		return -(TREF(ydb_error_code));
	}
	clear_timers();	/* see comment before FORK macro in fork_init.h for why this is needed in child pid */
	getjobnum();	/* set "process_id" to a value different from parent */
	for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
	{
		for (reg = addr_ptr->regions, reg_top = reg + addr_ptr->n_regions; reg < reg_top; reg++)
		{
			if (reg->open && !reg->was_open && IS_REG_BG_OR_MM(reg))
			{
				udi = (unix_db_info *)(reg->dyn.addr->file_cntl->file_info);
				csa = &udi->s_addrs;
				assert(!csa->hold_onto_crit);
				csa->now_crit = FALSE;	/* in case parent was holding crit while doing the "fork" */
				csd = csa->hdr;
				jpc = csa->jnl;
				/* Bump ftok semaphore counter. Note that we require the parent pid to not exit until
				 * ydb_child_init() is done on the child. That way it is okay to do a "ftok_sem_lock"
				 * (as if the parent dies, it is possible the ftok semid gets removed from the system).
				 */
				assert(!udi->grabbed_ftok_sem);	/* parent could be holding crit (e.g. in middle of TP
								 * transaction in final retry) but cannot be holding
								 * ftok semaphore during the "fork".
								 */
				if (!ftok_sem_lock(reg, IMMEDIATE_FALSE))
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(reg),
						ERR_SYSCALL, 5, RTS_ERROR_TEXT("ydb_child_init() ftok_sem_lock"), CALLFROM, errno);
				if (!ftok_sem_release(reg, DECR_CNT_FALSE, IMMEDIATE_FALSE))
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(reg));
				/* NARSTODO: Ensure shm rundown is skipped when child exits */
				/* NARSTODO: Shm count bump for database */
				/* NARSTODO: Sem count bump for database */
				/* Ensure that journal files are reopened by this process since we need to write a
				 * new PINI record on behalf of the child pid (instead of inheriting the parent PINI).
				 */
				if ((NULL != jpc) && (NOJNL != jpc->channel))
					jpc->cycle--;
			}
		}
	}
	/* NARSTODO: Ensure journal pool shm rundown is skipped when child exits */
	/* NARSTODO: Shm count bump for journal pool(s) */
	/* NARSTODO: Shm count bump for receive pool */
	/* NARSTODO: Ensure receive pool shm rundown is skipped when child exits */
	/* NARSTODO: Sem count bump for journal pool(s) */
	/* NARSTODO: Sem count bump for receive pool */
	/* Re-initialize mutex socket, memory semaphore etc. with child's pid if already done by parent */
	if (mutex_per_process_init_pid)
		mutex_per_process_init();
	/* Since we are removing artifacts from the originating process (which still has these files open), there is
	 * no need to decrement the counts (they will increase if this process links the same files). The FALSE
	 * argument prevents the relinkctl-attach & rtnobj-reference counts from being modified in this cleanup.
	 */
	ARLINK_ONLY(relinkctl_rundown(FALSE, FALSE));
	REVERT;
	return YDB_OK;
}
