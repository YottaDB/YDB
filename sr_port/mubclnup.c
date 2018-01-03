/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_fcntl.h"
#include "gtm_unistd.h"

#include "gtm_string.h"
#include "stringpool.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "mupipbckup.h"
#include "gdscc.h"
#include "gdskill.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "util.h"
#include "gtmmsg.h"
#include "memcoherency.h"
#include "shmpool.h"
#include "interlock.h"
#include "add_inter.h"

#include "ftok_sems.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmio.h"

GBLREF 	spdesc 			stringpool;
GBLREF 	tp_region 		*grlist;
GBLREF	tp_region		*halt_ptr;
GBLREF	bool			online;
GBLREF  bool            	error_mupip;
GBLREF	boolean_t		backup_interrupted;

GBLREF	backup_reg_list		*mu_repl_inst_reg_list;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	boolean_t		jnlpool_init_needed;

error_def(ERR_FORCEDHALT);

void mubclnup(backup_reg_list *curr_ptr, clnup_stage stage)
{
	sgmnt_addrs	*csa;
	backup_reg_list *ptr, *next;
	uint4		status;
	boolean_t	had_lock;
	unix_db_info	*udi;
	int		rc;

	assert(stage >= need_to_free_space && stage < num_of_clnup_stage);

	free(stringpool.base);

	switch(stage)
	{
	case need_to_rel_crit:
		for (ptr = (backup_reg_list *)grlist; ptr != NULL && ptr != curr_ptr && ptr != (backup_reg_list *)halt_ptr;)
		{
			if (keep_going == ptr->not_this_time)
			{
				csa = &FILE_INFO(ptr->reg)->s_addrs;
				DECR_INHIBIT_KILLS(csa->nl);
				rel_crit(ptr->reg);
			}
			ptr = ptr->fPtr;
		}
		curr_ptr = (backup_reg_list *)halt_ptr;
		/* Intentional Fall Through */
	case need_to_del_tempfile:
		for (ptr = (backup_reg_list *)grlist; ptr != NULL && ptr != curr_ptr;)
		{
			assert(3 == num_backup_proc_status);   /* Ensure there are only 3 possible values for "ptr->not_this_time".
								* The assert below and the following if check rely on this. */
			assert((keep_going == ptr->not_this_time)
				|| (give_up_before_create_tempfile == ptr->not_this_time)
				|| (give_up_after_create_tempfile == ptr->not_this_time));
			if (give_up_before_create_tempfile != ptr->not_this_time)
			{
				free(ptr->backup_hdr);
				if (online)
				{	/* Stop temporary file from growing if we made it active */
					if (keep_going == ptr->not_this_time)
					{
						csa = &FILE_INFO(ptr->reg)->s_addrs;
						csa->nl->nbb = BACKUP_NOT_IN_PROGRESS;
						/* Make sure all running processes have a chance to see this backup
						   state change so they won't be trying to flush when we go to delete
						   the temporary files (mostly an issue on VMS).

						   This operation notifies other processes by:
						   1) Using a compswap lock with builtin memory barriers so other
						      processors know the memory state change.
						   2) Processes obtaining the lock after we release it will do their
						      own memory barrier operation and see the change.
						   3) By grabbing the lock, we are assured that anyone else getting the
						      lock after us will also be checking the errno flag AFTER getting the
						      lock (see backup_buffer_flush()) and see no flush is necessary.
						*/
						if (!(had_lock = shmpool_lock_held_by_us(ptr->reg)))
							shmpool_lock_hdr(ptr->reg);

						if (backup_interrupted && 0 == csa->shmpool_buffer->backup_errno)
							/* Needs a non-zero value to stop the backup */
							csa->shmpool_buffer->backup_errno = ERR_FORCEDHALT;
						if (!had_lock)
							shmpool_unlock_hdr(ptr->reg);
					}
					/* get rid of the temporary file */
					if (ptr->backup_fd > 2)
					{
						CLOSEFILE_RESET(ptr->backup_fd, rc);	/* resets "ptr" to FD_INVALID */
						UNLINK(ptr->backup_tempfile);
					}
				} else	/* defreeze the databases */
					region_freeze(ptr->reg, FALSE, FALSE, FALSE, FALSE, FALSE);
			}
			ptr = ptr->fPtr;
		}

		/* Intentional fall through */
	case need_to_free_space:
		for (ptr = (backup_reg_list *)grlist; ptr != NULL;)
		{
			next = ptr->fPtr;
			if (keep_going != ptr->not_this_time)
				error_mupip = TRUE;
			if (NULL != ptr->backup_file.addr)
				free(ptr->backup_file.addr);
			free(ptr);
			ptr = next;
		}
		break;
	default:
		assert(FALSE && stage);
		break;					/* NOTREACHED */
	}
	/* Release FTOK lock on the replication instance file if holding it */
	assert((NULL == jnlpool) || (NULL == jnlpool->jnlpool_dummy_reg) || (NULL != mu_repl_inst_reg_list) || jnlpool_init_needed);
	if ((NULL != mu_repl_inst_reg_list) && (NULL != jnlpool) && (NULL != jnlpool->jnlpool_dummy_reg)
			&& jnlpool->jnlpool_dummy_reg->open)
	{
		udi = FILE_INFO(jnlpool->jnlpool_dummy_reg);
		assert(NULL != udi);
		if (NULL != udi)
		{	/* See gv_rundown.c comment for why ftok_sem_release 2nd parameter is FALSE below */
			if (udi->grabbed_ftok_sem)
				ftok_sem_release(jnlpool->jnlpool_dummy_reg, FALSE, TRUE);
			assert(!udi->grabbed_ftok_sem);
		}
	}
	return;
}
