/****************************************************************
 *								*
 * Copyright (c) 2001-2025 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_ipc.h"

#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include "gtm_unistd.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "error.h"
#include "filestruct.h"
#include "gvcst_protos.h"
#include "jnl.h"
#include "do_semop.h"
#include "mmseg.h"
#include "ipcrmid.h"
#include "util.h"
#include "ftok_sems.h"
#include "gtmimagename.h"
#include "gtmio.h"
#include "have_crit.h"

GBLREF gd_region		*db_init_region;
GBLREF uint4			process_id;

error_def(ERR_VERMISMATCH);

CONDITION_HANDLER(dbinit_ch)
{
	START_CH(TRUE);
	db_init_err_cleanup(FALSE);
	NEXTCH
}

void db_init_err_cleanup(boolean_t retry_dbinit)
{
	unix_db_info		*udi;
	gd_segment		*seg;
	sgmnt_addrs		*csa;
	DEBUG_ONLY(struct shmid_ds		shm_buf;)
	int			rc, lcl_new_dbinit_ipc;
	boolean_t		ftok_counter_halted, access_counter_halted, decrement_ftok_counter;
	boolean_t		shm_owner, have_access, have_ftok, we_are_last_user;

	/* Here, we can not rely on the validity of csa->hdr because this function can be triggered anywhere in db_init().Because
	 * we don't have access to file header, we can not know if counters are disabled so we go by our best guess, not disabled,
	 * during cleanup.
	 */
	assert(NULL != db_init_region);
	seg = db_init_region->dyn.addr;
	udi = NULL;
	if (NULL != seg->file_cntl)
		udi = FILE_INFO(db_init_region);
	if (NULL != udi)
	{
		csa = &udi->s_addrs;
#		ifdef _AIX
		if ((NULL != csa->hdr) && (dba_mm == db_init_region->dyn.addr->acc_meth))
		{
			assert((NULL != csa->db_addrs[1]) && (csa->db_addrs[1] > csa->db_addrs[0]));
			munmap((caddr_t)csa->db_addrs[0], (size_t)(csa->db_addrs[1] - csa->db_addrs[0]));
		}
#		endif
		if (NULL != csa->jnl)
		{
			free(csa->jnl);
			csa->jnl = NULL;
		}
		/* If shared memory is not available or if this is a VERMISMATCH error situation (where we do not know the exact
		 * position of csa->nl->ftok_counter_halted or if it even exists in the other version), we have to be pessimistic
		 * and assume the counters are halted. This avoids prematurely removing the semaphores.
		 */
		if ((NULL != csa->nl) && ((int)ERR_VERMISMATCH != SIGNAL))
		{
			ftok_counter_halted = csa->nl->ftok_counter_halted;
			access_counter_halted = csa->nl->access_counter_halted;
			if (csa->now_crit)
			{	/* Ensure that we don't hold crit before detaching shared memory */
				assert(!csa->now_crit);
				rel_crit(db_init_region);
			}
			shmdt((caddr_t)csa->nl);
			csa->nl = (node_local_ptr_t)NULL;
		} else
		{
			ftok_counter_halted = TRUE;
			access_counter_halted = TRUE;
		}
		if (udi->shm_created && (INVALID_SHMID != udi->shmid))
		{
			assert((-1 != shmctl(udi->shmid, IPC_STAT, &shm_buf)) && (0 == shm_buf.shm_nattch));
			shm_owner = TRUE;
			assert(INVALID_SHMID == csa->mlkhash_shmid);
			shm_rmid(udi->shmid);
			udi->shmid = INVALID_SHMID;
			udi->shm_created = FALSE;
		} else
			shm_owner = FALSE;
		/* The following code along with the shm_owner logic above should be maintained in concert with the safe_mode
		 * and we_are_last_user variables in gds_rundown.
		 */
		have_ftok = udi->grabbed_ftok_sem;
		/* Since we_are_last_user only happens if we created the shm, vermismatch not possible. Ignore inst frozen since
		 * this is deleting a database file that only this process has ever used (and only abortively during
		 * initialization).
		 */
		we_are_last_user = shm_owner && have_ftok && !ftok_counter_halted;
		if (we_are_last_user && IS_AUTODELETE_REG(db_init_region) && !retry_dbinit)
		{
			DBGRDB((stderr, "%s:%d:%s: process id %d autodeleting autodb file %s for region %s\n", __FILE__, __LINE__,
						__func__, process_id, (db_init_region)->dyn.addr->fname, (db_init_region)->rname));
			rc = UNLINK((char *)(db_init_region)->dyn.addr->fname);
			assert(0 == rc);
		}
		if (udi->sem_created && (INVALID_SEMID != udi->semid))
		{
			sem_rmid(udi->semid);
			udi->semid = INVALID_SEMID;
			udi->sem_created = FALSE;
			udi->grabbed_access_sem = FALSE;
			udi->counter_acc_incremented = FALSE;
		} else
			assert(!we_are_last_user);
		if (udi->counter_acc_incremented && !access_counter_halted)
		{
			assert((INVALID_SEMID != udi->semid) && !db_init_region->read_only);
			/* decrement the read-write sem */
			do_semop(udi->semid, DB_COUNTER_SEM, -DB_COUNTER_SEM_INCR, SEM_UNDO | IPC_NOWAIT);
			udi->counter_acc_incremented = FALSE;
		}
		if (udi->grabbed_access_sem)
		{
			do_semop(udi->semid, DB_CONTROL_SEM, -1, SEM_UNDO | IPC_NOWAIT); /* release the startup-shutdown sem */
			udi->grabbed_access_sem = FALSE;
		}
		if (FD_INVALID != udi->fd && !retry_dbinit)
			CLOSEFILE_RESET(udi->fd, rc);	/* resets "udi->fd" to FD_INVALID */
		assert(FD_INVALID == udi->fd || retry_dbinit);

		decrement_ftok_counter = udi->counter_ftok_incremented
						? (ftok_counter_halted ? DECR_CNT_SAFE : DECR_CNT_TRUE)
						: DECR_CNT_FALSE;
		if (udi->grabbed_ftok_sem)
		{
			ftok_sem_release(db_init_region, decrement_ftok_counter, TRUE);
			assert(FALSE == udi->counter_ftok_incremented);
		} else if (udi->counter_ftok_incremented)
			do_semop(udi->ftok_semid, DB_COUNTER_SEM, -DB_COUNTER_SEM_INCR, SEM_UNDO | IPC_NOWAIT);
		/* Below reset needed for "else if" case above but do it for "if" case too (in pro) just in case */
		udi->counter_ftok_incremented = FALSE;
		udi->grabbed_ftok_sem = FALSE;
		if (!IS_GTCM_GNP_SERVER_IMAGE && !retry_dbinit) /* gtcm_gnp_server reuses file_cntl */
		{
			free(seg->file_cntl->file_info);
			free(seg->file_cntl);
			seg->file_cntl = NULL;
		}

	}
	/* Enable interrupts in case we are here with intrpt_ok_state == INTRPT_IN_GVCST_INIT due to an rts error.
	 * Normally we would have the new state stored in "prev_intrpt_state" but that is not possible here because
	 * the corresponding DEFER_INTERRUPTS happened in gvcst_init.c (a different function) so we have an assert
	 * there that the previous state was INTRPT_OK_TO_INTERRUPT and use that instead of prev_intrpt_state here.
	 */
	if (!retry_dbinit)
	{
		db_init_region->file_initialized = FALSE;
		ENABLE_INTERRUPTS(INTRPT_IN_GVCST_INIT, INTRPT_OK_TO_INTERRUPT);
	}
}
