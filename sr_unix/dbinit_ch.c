/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
GBLREF boolean_t		sem_incremented;
GBLREF boolean_t		new_dbinit_ipc;

CONDITION_HANDLER(dbinit_ch)
{
	unix_db_info	*udi;
	gd_segment	*seg;
	struct shmid_ds	shm_buf;
	sgmnt_addrs	*csa;
	int		rc;

	START_CH;
	if (SUCCESS == SEVERITY || INFO == SEVERITY)
	{
		PRN_ERROR;
		CONTINUE;
	}
	seg = db_init_region->dyn.addr;
	udi = NULL;
	if (NULL != seg->file_cntl)
		udi = FILE_INFO(db_init_region);
	if (NULL != udi)
	{
		if (FD_INVALID != udi->fd)
			CLOSEFILE_RESET(udi->fd, rc);	/* resets "udi->fd" to FD_INVALID */
		assert(FD_INVALID == udi->fd);
		csa = &udi->s_addrs;
		if (NULL != csa->hdr)
		{
			if (dba_mm == db_init_region->dyn.addr->acc_meth)
				munmap((caddr_t)csa->db_addrs[0], (size_t)(csa->db_addrs[1] - csa->db_addrs[0]));
			csa->hdr = (sgmnt_data_ptr_t)NULL;
		}
		if (NULL != csa->jnl)
		{
			free(csa->jnl);
			csa->jnl = NULL;
		}
		if (csa->nl)
		{
			shmdt((caddr_t)csa->nl);
			csa->nl = (node_local_ptr_t)NULL;
		}
		if (new_dbinit_ipc)
		{
			if (INVALID_SHMID != udi->shmid)
			{
				shm_rmid(udi->shmid);
				udi->shmid = INVALID_SHMID;
			}
			assert(INVALID_SEMID != udi->semid);	/* we better have created a semaphore if new_dbinit_ipc is TRUE */
			if (INVALID_SEMID != udi->semid)
			{
				sem_rmid(udi->semid);
				udi->semid = INVALID_SEMID;
			}
		}
		if (sem_incremented)
		{
			if (INVALID_SEMID != udi->semid)
			{
				if (FALSE == db_init_region->read_only)
					do_semop(udi->semid, 1, -1, SEM_UNDO);	/* decrement the read-write sem */
				do_semop(udi->semid, 0, -1, SEM_UNDO);		/* release the startup-shutdown sem */
			}
			sem_incremented = FALSE;
		}

		if (udi->grabbed_ftok_sem)
			ftok_sem_release(db_init_region, TRUE, TRUE);
		if (!IS_GTCM_GNP_SERVER_IMAGE) /* gtcm_gnp_server reuses file_cntl */
		{
			free(seg->file_cntl->file_info);
			free(seg->file_cntl);
			seg->file_cntl = NULL;
		}
	}
	/* Reset intrpt_ok_state to OK_TO_INTERRUPT in case we got called (due to an rts_error) with intrpt_ok_state
	 * being set to INTRPT_IN_GVCST_INIT.
	 * We should actually be calling RESTORE_INTRPT_OK_STATE macro but since we don't have access to local variable
	 * save_intrpt_ok_state, set intrpt_ok_state directly.
	 */
	assert((INTRPT_OK_TO_INTERRUPT == intrpt_ok_state) || (INTRPT_IN_GVCST_INIT == intrpt_ok_state));
	intrpt_ok_state = INTRPT_OK_TO_INTERRUPT;
	NEXTCH;
}
