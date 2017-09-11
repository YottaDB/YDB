/****************************************************************
 *								*
 * Copyright (c) 2015-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_unistd.h"
#include "gtm_stdlib.h"

#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include "error.h"
#include "gtm_ipc.h"
#include "gtm_multi_proc.h"
#include "do_shmat.h"
#include "ipcrmid.h"
#include "gtmmsg.h"
#include "iosp.h"
#include "fork_init.h"
#include "getjobnum.h"
#include "eintr_wrappers.h"
#include "interlock.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "filestruct.h"
#include "gdskill.h"
#include "buddy_list.h"
#include "hashtab_int4.h"
#include "jnl.h"
#include "tp.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "mutex.h"

#ifdef DEBUG
#include "is_proc_alive.h"
#endif
#ifdef DEBUG	/* Below are needed in case MUR_DEBUG is defined */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#endif

GBLREF	VSIG_ATOMIC_T	forced_exit;
GBLREF	uint4		process_id;
GBLREF	boolean_t	skip_exit_handler;
GBLREF	uint4		mutex_per_process_init_pid;

error_def(ERR_FORCEDHALT);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);

/* This function invokes "fnptr" with the argument "&parm_array[i]" where "i" ranges from 0 thru "ntasks - 1".
 * At most "max_procs" processes will run parallely at any given point in time with two special exceptions.
 *	max_procs = 0 implies one process runs parallely per region.
 *	max_procs = 1 implies no process-parallelization are used.
 *
 * Returns 0 (SS_NORMAL) if "ntasks" tasks were successfully created and completed (with or without multiple processes).
 *	"ret_array[]" contains individual task invocation exit status in this case.
 * Returns non-zero otherwise. In this case, it waits for all/any created processes to die down before returning.
 *	Also, "ret_array[]" is filled with return status of each "fnptr" task invocation as appropriate.
 *	Caller needs to look at the function return value and "ret_array[]" and issue appropriate error messages.
 *	Note: Although ret_array[i] is of type "void *", if the function "fnptr" returns a pointer, it has to point
 *		to memory that is visible to both the parent and the forked-off children (e.g. cannot point to heap
 *		which is child-specific memory). Safest would be for "fnptr" to return a non-pointer return type.
 *
 * Additionally, one can specify
 *   --> "extra_shm_size" to indicate caller-specific extra space to allocate in the shared memory segment that
 *		"gtm_multi_proc" anyways creates.
 *   --> "init_fnptr" to indicate a caller-specific initialization function to invoke after shared memory creation.
 *		Note: This function is NOT called in case no parallel processes are started.
 *   --> "finish_fnptr" to indicate a caller-specific finish function that is invoked once all parallel processes return
 *		and before "gtm_multi_proc" returns to caller. Note that this invocation happens even if no parallel
 *		processes are invoked internally. As long as "init_fnptr" was invoked, "finish_fnptr" will be invoked.
 */
int	gtm_multi_proc(gtm_multi_proc_fnptr_t fnptr, int ntasks, int max_procs,
					void **ret_array, void *parm_array, int parmElemSize,
					size_t extra_shm_size, gtm_multi_proc_fnptr_t init_fnptr,
					gtm_multi_proc_fnptr_t finish_fnptr)
{
	int			final_ret, rc, rc2, tasknum, shmid, save_errno;
	char			errstr[256];
	size_t			shm_size;
	pid_t			child_pid;
	void			**ret_ptr;
	uchar_ptr_t		parm_ptr;
	multi_proc_shm_hdr_t	*mp_hdr;	/* Pointer to "multi_proc_shm_hdr_t" structure in shared memory */
	intrpt_state_t		prev_intrpt_state;

	assert(!multi_proc_in_use);
	if (!max_procs || (max_procs > ntasks))
		max_procs = ntasks;
	final_ret = 0;
	ret_ptr = &ret_array[0];
	memset(ret_ptr, 0, SIZEOF(void *) * ntasks);	/* initialize return status to SS_NORMAL/0 */
	parm_ptr = (uchar_ptr_t)parm_array;
	if (1 == max_procs)
	{	/* Simplest case. No parallelization. Finish and return */
		for (tasknum = 0; tasknum < ntasks; tasknum++, parm_ptr += parmElemSize, ret_ptr++)
		{
			if (!final_ret)
			{
				rc = (INTPTR_T)(*fnptr)(parm_ptr);
				if (rc)
					final_ret = rc;
			} else
				rc = 0;
			*ret_ptr = (void *)(INTPTR_T)rc;
		}
		if (NULL != finish_fnptr)
		{
			rc = (INTPTR_T)(*finish_fnptr)(NULL);
			if (rc)
			{
				assert(FALSE);
				if (!final_ret)
					final_ret = rc;
			}
		}
		return final_ret;
	}
	if (MULTI_PROC_MAX_PROCS <= max_procs)
	{
		SNPRINTF(errstr, SIZEOF(errstr), "gtm_multi_proc : Cannot fork() more than %d processes : %d processes requested",
			MULTI_PROC_MAX_PROCS - 1, max_procs);
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) MAKE_MSG_TYPE(ERR_TEXT, ERROR), 2, LEN_AND_STR(errstr));
		return -1;
	}
	shm_size = SIZEOF(multi_proc_shm_hdr_t);
	/* Allocate space for return array in shared memory. This will be later copied back to "ret_array" for caller */
	shm_size += (SIZEOF(void *) * ntasks);
	shm_size += extra_shm_size;
	shmid = shmget(IPC_PRIVATE, shm_size, 0600 | IPC_CREAT);
	if (-1 == shmid)
	{
		save_errno = errno;
		SNPRINTF(errstr, SIZEOF(errstr), "shmget() : shmsize=0x%llx", shm_size);
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_STR(errstr), CALLFROM, save_errno);
		return -1;
	}
	multi_proc_shm_hdr = (multi_proc_shm_hdr_t *)do_shmat(shmid, 0, 0);
	if (-1 == (sm_long_t)(multi_proc_shm_hdr))
	{
		save_errno = errno;
		SNPRINTF(errstr, SIZEOF(errstr), "shmat() : shmid=%d shmsize=0x%llx",
												shmid, shm_size);
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_STR(errstr), CALLFROM, save_errno);
		return -1;
	}
	/* Initialize shm hdr */
	mp_hdr = multi_proc_shm_hdr;
	memset(mp_hdr, 0, shm_size);
	mp_hdr->shmid = shmid;
	mp_hdr->parent_pid = process_id;
	mp_hdr->fnptr = fnptr;
	mp_hdr->pvt_ret_array = ret_array;
	mp_hdr->shm_ret_array = (void **)(mp_hdr + 1);
	mp_hdr->parm_array = parm_array;
	mp_hdr->ntasks = ntasks;
	mp_hdr->max_procs = max_procs;
	mp_hdr->parmElemSize = parmElemSize;
	/* Defer interrupts (SIG-15 etc.) while processes are being forked off. Note that the interrupt will invoke
	 * "generic_signal_handler" and cause SET_FORCED_MULTI_PROC_EXIT to be invoked (through the SET_FORCED_EXIT_STATE
	 * macro) which will cause the forked off children to die at a logical point as soon as possible. So we do handle
	 * the external signal even though it is slightly deferred.
	 */
	DEFER_INTERRUPTS(INTRPT_IN_GTM_MULTI_PROC, prev_intrpt_state);
	multi_proc_in_use = TRUE;
	assert(NULL == multi_proc_key);
	rc = (INTPTR_T)(*init_fnptr)((uchar_ptr_t)parm_array);	/* Invoke caller-specific initialization function first */
	if (0 == rc)
	{
		/* Fork off all processes next */
		for (tasknum = 0; tasknum < max_procs; tasknum++)
		{
			if (forced_exit)
			{	/* We got an external signal that wants us to terminate as soon as possible. */
				SET_FORCED_MULTI_PROC_EXIT;	/* signal any forked off children to finish at a logical point */
				gtm_multi_proc_finish(finish_fnptr);	/* wait for forked off pids to finish */
				multi_proc_in_use = FALSE;
				ENABLE_INTERRUPTS(INTRPT_IN_GTM_MULTI_PROC, prev_intrpt_state);
				return -1;
			}
			FORK(child_pid);
			if (-1 == child_pid)
			{
				save_errno = errno;
				SNPRINTF(errstr, SIZEOF(errstr), "fork() : tasknum=%d out of %d total processes",
													tasknum, max_procs);
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
							LEN_AND_STR(errstr), CALLFROM, save_errno);
				SET_FORCED_MULTI_PROC_EXIT;	/* signal any forked off children to finish at a logical point */
				gtm_multi_proc_finish(finish_fnptr);	/* wait for forked off pids to finish */
				multi_proc_in_use = FALSE;
				ENABLE_INTERRUPTS(INTRPT_IN_GTM_MULTI_PROC, prev_intrpt_state);
				multi_proc_in_use = TRUE;
				return -1;
			}
			if (0 == child_pid)
			{	/* The child process should operate as a regular process so re-enable interrupts.
				 * But before that, set "process_id" to a value different from the parent
				 * as the ENABLE_INTERRUPTS macro could end up calling
				 * deferred_signal_handler -> forced_exit_err_display -> gtm_putmsg_csa -> grab_latch
				 * and grab_latch would fail an assert otherwise.
				 */
				getjobnum();    /* set "process_id" to a value different from parent */
				/* Skip exit handler, as otherwise we would reduce reference counts in database
				 * shared memory etc. for each forked off process when they go to gds_rundown when
				 * actually they did not do any db_init (they inherited the db from the parent).
				 * Do it here instead of in the helper in case enabling interrupts causes us to exit.
				 */
				skip_exit_handler = TRUE;
				DEBUG_ONLY(multi_proc_key_exception = TRUE);	/* Allow error messages without a key */
				ENABLE_INTERRUPTS(INTRPT_IN_GTM_MULTI_PROC, prev_intrpt_state);
				DEBUG_ONLY(multi_proc_key_exception = FALSE);
				gtm_multi_proc_helper();	/* Note: does not return */
			}
			mp_hdr->pid[tasknum] = child_pid;
			mp_hdr->procs_created = tasknum + 1;
		}
	}
	rc2 = gtm_multi_proc_finish(finish_fnptr);	/* wait for all forked off processes to finish */
	final_ret = rc ? rc : rc2;
	multi_proc_in_use = FALSE;
	ENABLE_INTERRUPTS(INTRPT_IN_GTM_MULTI_PROC, prev_intrpt_state);
	return final_ret;
}

void	gtm_multi_proc_helper(void)
{
	int			nexttask, ntasks, rc;
	multi_proc_shm_hdr_t	*mp_hdr;	/* Pointer to "multi_proc_shm_hdr_t" structure in shared memory */
	void			*parm_array;
	void			**ret_array, **ret_ptr;
	int			parmElemSize;
	uchar_ptr_t		parm_ptr;
	gtm_multi_proc_fnptr_t	fnptr;
	boolean_t		release_latch;

#	ifdef MUR_DEBUG
	fprintf(stderr, "pid = %d : Started\n", process_id);
#	endif
	/* Do process-level reinitialization of a few things (see gtmrecv.c, gtmsource.c for example usage) */
	/* Re-initialize mutex socket, memory semaphore etc. with child's pid if already done by parent */
	if (mutex_per_process_init_pid)
	{
		assert(mutex_per_process_init_pid != process_id);
		mutex_per_process_init();
	}
	/* process-level reinitialization is done */
	mp_hdr = multi_proc_shm_hdr;
	ntasks = mp_hdr->ntasks;
	parm_array = mp_hdr->parm_array;
	parmElemSize = mp_hdr->parmElemSize;
	ret_array = mp_hdr->shm_ret_array;
	fnptr = mp_hdr->fnptr;
	rc = 0;
	while (TRUE)
	{
		GRAB_MULTI_PROC_LATCH_IF_NEEDED(release_latch);
		assert(release_latch);
		nexttask = mp_hdr->next_task;
		if (nexttask < ntasks)
			mp_hdr->next_task = nexttask + 1;
		REL_MULTI_PROC_LATCH_IF_NEEDED(release_latch);
		if (nexttask >= ntasks)
			break;
		parm_ptr = (uchar_ptr_t)parm_array + (parmElemSize * nexttask);
		ret_ptr = &ret_array[nexttask];
		assert(0 == *ret_ptr);	/* should have been initialized at entry into "gtm_multi_proc" */
		if (IS_FORCED_MULTI_PROC_EXIT(mp_hdr))
		{	/* Either parent or sibling parallel process has encountered a signal/error. So stop at logical point */
			rc = ERR_FORCEDHALT;
			*ret_ptr = (void *)(INTPTR_T)rc;
			break;
		}
		rc = (INTPTR_T)(*fnptr)(parm_ptr);
		*ret_ptr = (void *)(INTPTR_T)rc;
		if (0 != rc)
		{	/* Stop the already running threads */
			SET_FORCED_MULTI_PROC_EXIT;	/* signal any forked off children to finish at a logical point */
			break;
		}
		nexttask++;
	}
#	ifdef MUR_DEBUG
	fprintf(stderr, "pid = %d : Completed\n", process_id);
#	endif
	EXIT(rc);
}

int	gtm_multi_proc_finish(gtm_multi_proc_fnptr_t finish_fnptr)
{
	int			max_procs, tasknum, num_pids_to_wait, num_pids_waited, save_errno;
	int			shmid;
	int			stat;	/* child exit status */
#	ifdef _BSD
        union wait      	wait_stat;
#	else
        int			wait_stat;
#	endif
	pid_t			ret_pid;	/* return value from waitpid */
	int			ret2, final_ret;
	char			errstr[256];
	pid_t			pid;
	multi_proc_shm_hdr_t	*mp_hdr;	/* Pointer to "multi_proc_shm_hdr_t" structure in shared memory */

	assert(multi_proc_in_use);
	mp_hdr = multi_proc_shm_hdr;
	assert(process_id == mp_hdr->parent_pid);	/* assert this function is not invoked by child processes */
	max_procs = mp_hdr->procs_created;
	final_ret = 0;
	num_pids_to_wait = 0;
	for (tasknum = 0; tasknum < max_procs; tasknum++)
	{
		pid = mp_hdr->pid[tasknum];
		if (0 == pid)
			continue;
		num_pids_to_wait++;
	}
	assert(num_pids_to_wait == max_procs);
	/* It is possible the child pids terminate in an arbitrary order. In that case, we don't want to be
	 * stuck doing a WAITPID of the first pid when the second pid has finished since it is possible the
	 * second pid terminated abnormally (e.g. holding a latch) and until we do the WAITPID for that pid
	 * it would be a defunct pid and "is_proc_alive" calls from the first pid will return the second pid
	 * as alive (which is incorrect) potentially causing the first pid to hang eternally waiting for the
	 * same latch. Therefore do WAITPID for an arbitrary child.
	 */
	for (num_pids_waited = 0; num_pids_waited < num_pids_to_wait; )
	{
		WAITPID((pid_t)-1, &stat, 0, ret_pid);
		if (-1 == ret_pid)
		{
			assert(FALSE);
			save_errno = errno;
			SNPRINTF(errstr, SIZEOF(errstr), "waitpid()");	/* BYPASSOK("waitpid") */
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_STR(errstr), CALLFROM, save_errno);
			/* Note: In this case we do not copy mp_hdr->shm_ret_array to mp_hdr->pvt_ret_array because we
			 * do not know what state the latter is in (due to the abrupt error return from waitpid).
			 */
			return -1;
		}
		for (tasknum = 0; tasknum < max_procs; tasknum++)
		{
			pid = mp_hdr->pid[tasknum];
			if (0 == pid)
				continue;
			if (pid == ret_pid)
			{
				mp_hdr->pid[tasknum] = 0;	/* so we do not wait again for this pid */
				mp_hdr->orig_pid[tasknum] = ret_pid;	/* for debugging purposes */
				break;
			}
		}
		assert(FALSE == is_proc_alive(ret_pid, 0));
		if (tasknum == max_procs)
		{	/* This is a child pid that we did not fork off in "gtm_multi_proc".
			 * Skip this and continue waiting for the child pids we did fork off.
			 */
			continue;
		}
		mp_hdr->wait_stat[tasknum] = stat;	/* for debugging purposes */
#		ifdef _BSD
		assert(SIZEOF(wait_stat) == SIZEOF(int4));
		wait_stat.w_status = stat;
#		else
		wait_stat = stat;
#		endif
		if (WIFEXITED(wait_stat))
			ret2 = WEXITSTATUS(wait_stat);
		else if (WIFSIGNALED(wait_stat))
			ret2 = WTERMSIG(wait_stat);
		else
			ret2 = 0;
		if (ret2 && !final_ret)
		{
			final_ret = ret2;
			SET_FORCED_MULTI_PROC_EXIT;	/* Signal any currently-running forked off children
							 * to finish at a logical point. */
		}
		num_pids_waited++;
	}
	mp_hdr->wait_done = TRUE;
#	ifdef DEBUG
	for (tasknum = 0; tasknum < max_procs; tasknum++)
	{
		pid = mp_hdr->pid[tasknum];
		assert(0 == pid);
	}
#	endif
	/* Copy return status of each task from shared memory to private memory (needed by caller of "gtm_multi_proc") */
	memcpy(mp_hdr->pvt_ret_array, mp_hdr->shm_ret_array, (SIZEOF(void *) * mp_hdr->ntasks));
	if (NULL != finish_fnptr)
	{
		ret2 = (INTPTR_T)(*finish_fnptr)(NULL);
		if (ret2 && !final_ret)
			final_ret = ret2;
	}
	shmid = mp_hdr->shmid;
	ret2 = shm_rmid(shmid);
	if (0 != ret2)
	{
		assert(FALSE);
		save_errno = errno;
		SNPRINTF(errstr, SIZEOF(errstr), "shm_rmid() : shmid=%d", shmid);
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8)
					ERR_SYSCALL, 5, LEN_AND_STR(errstr), CALLFROM, save_errno);
		if (!final_ret)
		{
			final_ret = ret2;
			assert(FALSE);
		}
	}
	return final_ret;
}
