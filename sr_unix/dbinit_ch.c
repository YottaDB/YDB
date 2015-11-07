/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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

CONDITION_HANDLER(dbinit_ch)
{
	START_CH;
	if (SUCCESS == SEVERITY || INFO == SEVERITY)
	{
		PRN_ERROR;
		CONTINUE;
	}
	db_init_err_cleanup(FALSE);
	NEXTCH
}

void db_init_err_cleanup(boolean_t retry_dbinit)
{
	unix_db_info		*udi;
	gd_segment		*seg;
	sgmnt_addrs		*csa;
	int			rc, lcl_new_dbinit_ipc;

	assert(NULL != db_init_region);
	seg = db_init_region->dyn.addr;
	udi = NULL;
	if (NULL != seg->file_cntl)
		udi = FILE_INFO(db_init_region);
	if (NULL != udi)
	{
		if (FD_INVALID != udi->fd && !retry_dbinit)
			CLOSEFILE_RESET(udi->fd, rc);	/* resets "udi->fd" to FD_INVALID */
		assert(FD_INVALID == udi->fd || retry_dbinit);
		csa = &udi->s_addrs;
#		ifdef GTM_CRYPT
		if (NULL != csa->encrypted_blk_contents)
		{
			free(csa->encrypted_blk_contents);
			csa->encrypted_blk_contents = NULL;
		}
#		endif
		if ((NULL != csa->hdr) && (dba_mm == db_init_region->dyn.addr->acc_meth))
		{
			assert((NULL != csa->db_addrs[1]) && (csa->db_addrs[1] > csa->db_addrs[0]));
			munmap((caddr_t)csa->db_addrs[0], (size_t)(csa->db_addrs[1] - csa->db_addrs[0]));
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
		if (udi->new_shm && (INVALID_SHMID != udi->shmid))
		{
			shm_rmid(udi->shmid);
			udi->shmid = INVALID_SHMID;
			udi->new_shm = FALSE;
		}
		if (udi->new_sem && (INVALID_SEMID != udi->semid))
		{
			sem_rmid(udi->semid);
			udi->semid = INVALID_SEMID;
			udi->new_sem = FALSE;
			udi->grabbed_access_sem = FALSE;
			udi->counter_acc_incremented = FALSE;
		}
		if (udi->counter_acc_incremented)
		{
			assert((INVALID_SEMID != udi->semid) && !db_init_region->read_only);
			do_semop(udi->semid, DB_COUNTER_SEM, -1, SEM_UNDO | IPC_NOWAIT);	/* decrement the read-write sem */
			udi->counter_acc_incremented = FALSE;
		}
		if (udi->grabbed_access_sem)
		{
			do_semop(udi->semid, DB_CONTROL_SEM, -1, SEM_UNDO | IPC_NOWAIT); /* release the startup-shutdown sem */
			udi->grabbed_access_sem = FALSE;
		}
		if (udi->grabbed_ftok_sem)
			ftok_sem_release(db_init_region, udi->counter_ftok_incremented, TRUE);
		else if (udi->counter_ftok_incremented)
			do_semop(udi->ftok_semid, DB_COUNTER_SEM, -1, SEM_UNDO | IPC_NOWAIT);
		udi->counter_ftok_incremented =FALSE;
		udi->grabbed_ftok_sem = FALSE;
		if (!IS_GTCM_GNP_SERVER_IMAGE && !retry_dbinit) /* gtcm_gnp_server reuses file_cntl */
		{
			free(seg->file_cntl->file_info);
			free(seg->file_cntl);
			seg->file_cntl = NULL;
		}
	}
	/* Enable interrupts in case we are here with intrpt_ok_state == INTRPT_IN_GVCST_INIT due to an rts error. */
	if (!retry_dbinit)
		ENABLE_INTERRUPTS(INTRPT_IN_GVCST_INIT);
}
