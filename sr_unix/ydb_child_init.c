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
#include "ftok_sems.h"
#include "gtm_semutils.h"
#include "interlock.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmrecv.h"
#include "repl_inst_ftok_counter_halted.h"

typedef enum
{
	DB_REG = 1,
	JNLPOOL_REG = 2,
	RECVPOOL_REG = 3,
} ydb_reg_type_t;

void	ydb_child_init_incrcnt(gd_region *reg, ydb_reg_type_t reg_type);

GBLREF	uint4			mutex_per_process_init_pid;
GBLREF	boolean_t		skip_exit_handler;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	recvpool_addrs		recvpool;
GBLREF	gd_region		*ftok_sem_reg;
GBLREF	jnl_process_vector	*prc_vec;

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
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Verify entry conditions, make sure YDB CI environment is up etc. */
	LIBYOTTADB_INIT(LYDB_RTN_CHILDINIT);	/* Note: macro could "return" from this function in case of errors */
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{
		REVERT;
		assert(NULL == ftok_sem_reg);
		return -(TREF(ydb_error_code));
	}
	assert(NULL == ftok_sem_reg);
	skip_exit_handler = TRUE; /* Until the child process database state is set up correctly, we should not try "gds_rundown" */
	clear_timers();	/* see comment before FORK macro in fork_init.h for why this is needed in child pid */
	getjobnum();	/* set "process_id" to a value different from parent */
	/* Re-initialize mutex socket, memory semaphore etc. with child's pid if already done by parent */
	if (mutex_per_process_init_pid)
		mutex_per_process_init();
	jnl_prc_vector(prc_vec);	/* Reinitialize prc_vec based on new process_id */
	/* Detach from any relinkctl shared memory segments that the parent is still attached to.
	 * We will attach to those if/when later needed.
	 *
	 * Since we are removing artifacts from the originating process (which still has these files open), there is
	 * no need to decrement the counts (they will increase if this process links the same files). The FALSE
	 * argument prevents the relinkctl-attach & rtnobj-reference counts from being modified in this cleanup.
	 */
	ARLINK_ONLY(relinkctl_rundown(FALSE, FALSE));
	for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
	{
		for (reg = addr_ptr->regions, reg_top = reg + addr_ptr->n_regions; reg < reg_top; reg++)
		{
			if (reg->open && !reg->was_open && IS_REG_BG_OR_MM(reg))
				ydb_child_init_incrcnt(reg, DB_REG);
		}
	}
	reg = jnlpool.jnlpool_dummy_reg;
	if ((NULL != reg) && reg->open)
		ydb_child_init_incrcnt(reg, JNLPOOL_REG);	/* Bump sem/shm count for receive pool */
	reg = recvpool.recvpool_dummy_reg;
	if ((NULL != reg) && reg->open)
		ydb_child_init_incrcnt(reg, RECVPOOL_REG);	/* Bump sem/shm count for receive pool */
	/* NARSTODO: Need to go through all attached jnlpool_ctls (starting V63003 this is a list, not just one jnlpool) */
	skip_exit_handler = FALSE; /* Now that the child process database state is set up correctly, it is safe to "gds_rundown" */
	assert(NULL == ftok_sem_reg);
	REVERT;
	return YDB_OK;
}

void	ydb_child_init_incrcnt(gd_region *reg, ydb_reg_type_t reg_type)
{
	unix_db_info		*udi;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	jnl_private_control	*jpc;
	struct sembuf		sop[3];
	int			save_errno, sopcnt, status, err_code;
	node_local_ptr_t	cnl;
	boolean_t		ftok_counter_halted, shm_counter_halted;

	udi = (unix_db_info *)(reg->dyn.addr->file_cntl->file_info);
	csa = &udi->s_addrs;
	csd = csa->hdr;
	cnl = csa->nl;
	assert(!csa->hold_onto_crit);
	csa->now_crit = FALSE;	/* in case parent was holding crit while doing the "fork" */
	assert(!udi->grabbed_ftok_sem);	/* parent could be holding crit (e.g. in middle of TP
					 * transaction in final retry) but cannot be holding
					 * ftok semaphore during the "fork".
					 */
	if (DB_REG == reg_type)
	{	/* The below code is similar to what is done in "db_init" */
		/* Get the ftok lock on the database before checking if the ftok and/or access control
		 * semaphore counters are overflown.
		 * Note that we require the parent pid to not exit until "ydb_child_init" is done on the child.
		 * That way it is okay to do a "ftok_sem_lock" below (if the parent dies, it is possible the
		 * ftok semid gets removed from the system and the "ftok_sem_lock" fail with an EIDRM/EINVAL error).
		 */
		if (!ftok_sem_lock(reg, IMMEDIATE_FALSE))
		{
			assert(FALSE);
			save_errno = errno;
			rts_error_csa(CSA_ARG(csa) VARLSTCNT(12) ERR_DBFILERR, 2, DB_LEN_STR(reg),
				ERR_SYSCALL, 5, RTS_ERROR_TEXT("ftok_sem_lock"),
				CALLFROM, save_errno);
		}
		/* Bump ftok semaphore counter on behalf of child pid. Note that if the parent did not
		 * increment the semaphore (possible if the counter had overflown the 32K limit), then the child
		 * should not bump the counter.
		 */
		if (udi->counter_ftok_incremented)
		{
			udi->counter_ftok_incremented = FALSE;	/* Reset field in child */
			if (!cnl->ftok_counter_halted)
			{	/* The counter has not overflown till now */
				SET_SOP_ARRAY_FOR_INCR_CNT(sop, sopcnt, SEM_UNDO);
				for ( ; ; )
				{
					status = semop(udi->ftok_semid, sop, sopcnt);
					if (-1 != status)
					{
						udi->counter_ftok_incremented = TRUE;
						break;
					}
					save_errno = errno;
					if (EINTR == save_errno)
						continue;
					if (ERANGE != save_errno)
					{
						assert(FALSE);
						ftok_sem_release(reg, DECR_CNT_FALSE, IMMEDIATE_FALSE);
						rts_error_csa(CSA_ARG(csa) VARLSTCNT(12) ERR_CRITSEMFAIL,
							2, DB_LEN_STR(reg), ERR_SYSCALL, 5,
							RTS_ERROR_TEXT("semop ftok"),
							CALLFROM, save_errno);
					}
					/* Counter has overflown. Check if we are the first ones to see the
					 * overflow. If so, send a syslog message (like "db_init" does).
					 */
					if (!csd->mumps_can_bypass)
					{
						save_errno = ERANGE;
						ftok_sem_release(reg, DECR_CNT_FALSE, IMMEDIATE_FALSE);
						rts_error_csa(CSA_ARG(csa) VARLSTCNT(12) ERR_CRITSEMFAIL,
							2, DB_LEN_STR(reg), ERR_SYSCALL, 5,
							RTS_ERROR_TEXT("semop ftok"),
							CALLFROM, save_errno);
					}
					cnl->ftok_counter_halted = TRUE;
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_NOMORESEMCNT, 5,
						LEN_AND_LIT("ftok"), FILE_TYPE_DB, DB_LEN_STR(reg));
					break;
				}
			} else
			{	/* Shared memory indicates the counter had overflown at some point in the past
				 * If so, no need to increment the counter as otherwise we could later incorrectly
				 * delete this ftok semaphore at rundown time when other processes are still
				 * accessing the database/jnlpool/recvpool.
				 */
			}
		}
		assert(!udi->grabbed_access_sem);	/* parent could be holding crit (e.g. in middle of TP
							 * transaction in final retry) but cannot be holding
							 * access control semaphore during the "fork".
							 */
		/* Bump access control semaphore counter on behalf of child pid. Note that if the parent did not
		 * increment the semaphore (possible if the counter had overflown the 32K limit), then the child
		 * should not bump the counter.
		 */
		if (udi->counter_acc_incremented)
		{
			udi->counter_acc_incremented = FALSE;	/* Reset field in child */
			if (!cnl->access_counter_halted)
			{	/* The counter has not overflown till now */
				SET_SOP_ARRAY_FOR_INCR_CNT(sop, sopcnt, SEM_UNDO);
				for ( ; ; )
				{
					status = semop(udi->semid, sop, sopcnt);
					if (-1 != status)
					{
						udi->counter_acc_incremented = TRUE;
						break;
					}
					save_errno = errno;
					if (EINTR == save_errno)
						continue;
					if (ERANGE != save_errno)
					{
						assert(FALSE);
						ftok_sem_release(reg, DECR_CNT_FALSE, IMMEDIATE_FALSE);
						rts_error_csa(CSA_ARG(csa) VARLSTCNT(12) ERR_CRITSEMFAIL,
							2, DB_LEN_STR(reg), ERR_SYSCALL, 5,
							RTS_ERROR_TEXT("semop access"),
							CALLFROM, save_errno);
					}
					/* Counter has overflown. Check if we are the first ones to see the
					 * overflow. If so, send a syslog message (like "db_init" does).
					 */
					if (!csd->mumps_can_bypass)
					{
						assert(FALSE);
						save_errno = ERANGE;
						ftok_sem_release(reg, DECR_CNT_FALSE, IMMEDIATE_FALSE);
						rts_error_csa(CSA_ARG(csa) VARLSTCNT(12) ERR_CRITSEMFAIL,
							2, DB_LEN_STR(reg), ERR_SYSCALL, 5,
							RTS_ERROR_TEXT("semop access"),
							CALLFROM, save_errno);
					}
					cnl->access_counter_halted = TRUE;
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_NOMORESEMCNT, 5,
						LEN_AND_LIT("access"), FILE_TYPE_DB, DB_LEN_STR(reg));
					break;
				}
			} else
			{	/* Shared memory indicates the counter had overflown at some point in the past
				 * If so, no need to increment the counter as otherwise we could later incorrectly
				 * delete this access control semaphore at rundown time when other processes are
				 * still accessing the database.
				 */
			}
		}
		/* Bump shared memory reference count on behalf of child pid */
		INCR_CNT(&cnl->ref_cnt, &cnl->wc_var_lock);
		/* Release ftok lock */
		if (!ftok_sem_release(reg, DECR_CNT_FALSE, IMMEDIATE_FALSE))
		{
			assert(FALSE);
			save_errno = errno;
			rts_error_csa(CSA_ARG(csa) VARLSTCNT(12) ERR_DBFILERR, 2, DB_LEN_STR(reg),
				ERR_SYSCALL, 5, RTS_ERROR_TEXT("ftok_sem_release"),
				CALLFROM, save_errno);
		}
		/* Ensure that journal files are reopened by this process since we need to write a
		 * new PINI record on behalf of the child pid (instead of inheriting the parent PINI).
		 */
		jpc = csa->jnl;
		if ((NULL != jpc) && (NOJNL != jpc->channel))
			jpc->cycle--;
	} else
	{	/* The below code is similar to "jnlpool_init/recvpool_init" and the flow is modeled like the DB_REG case above */
		assert((JNLPOOL_REG == reg_type) || (RECVPOOL_REG == reg_type));
		err_code = (JNLPOOL_REG == reg_type) ? ERR_JNLPOOLSETUP : ERR_RECVPOOLSETUP;
		if (!ftok_sem_get(reg, FALSE, REPLPOOL_ID, FALSE, &ftok_counter_halted))
		{
			assert(FALSE);
			save_errno = errno;
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(10) err_code, 0,
				ERR_SYSCALL, 5, RTS_ERROR_TEXT("ftok_sem_get"), CALLFROM, save_errno);
		}
		/* Bump ftok semaphore counter on behalf of child pid. Note that if the parent did not
		 * increment the semaphore (possible if the counter had overflown the 32K limit), then the child
		 * should not bump the counter.
		 */
		if (udi->counter_ftok_incremented)
		{
			udi->counter_ftok_incremented = FALSE;	/* Reset field in child */
			if (JNLPOOL_REG == reg_type)
				shm_counter_halted = jnlpool.jnlpool_ctl->ftok_counter_halted;
			else
				shm_counter_halted = FALSE; /* for receive pool, we can never overflow according to recvpool_init */
			if (!shm_counter_halted)
			{	/* The counter has not overflown till now */
				SET_SOP_ARRAY_FOR_INCR_CNT(sop, sopcnt, SEM_UNDO);
				for ( ; ; )
				{
					status = semop(udi->ftok_semid, sop, sopcnt);
					if (-1 != status)
					{
						udi->counter_ftok_incremented = TRUE;
						break;
					}
					save_errno = errno;
					if (EINTR == save_errno)
						continue;
					if (ERANGE != save_errno)
					{
						assert(FALSE);
						ftok_sem_release(reg, DECR_CNT_FALSE, IMMEDIATE_FALSE);
						rts_error_csa(CSA_ARG(csa) VARLSTCNT(12) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg),
							ERR_SYSCALL, 5, RTS_ERROR_TEXT("semop ftok"), CALLFROM, save_errno);
					}
					assert(JNLPOOL_REG == reg_type);
					if (!jnlpool.jnlpool_ctl->ftok_counter_halted)
						repl_inst_ftok_counter_halted(udi);
					break;
				}
			} else
			{	/* Shared memory indicates the counter had overflown at some point in the past
				 * If so, no need to increment the counter as otherwise we could later incorrectly
				 * delete this ftok semaphore at rundown time when other processes are still
				 * accessing the database/jnlpool/recvpool.
				 */
			}
		}
		if (!ftok_sem_release(reg, DECR_CNT_FALSE, IMMEDIATE_FALSE))
		{
			assert(FALSE);
			save_errno = errno;
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(10) err_code, 0,
				ERR_SYSCALL, 5, RTS_ERROR_TEXT("ftok_sem_release"), CALLFROM, save_errno);
		}
	}
}
