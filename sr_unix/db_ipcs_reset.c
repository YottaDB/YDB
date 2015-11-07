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

#include <sys/mman.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <errno.h>

#include "gtm_unistd.h"
#include "gtm_string.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gtmio.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "io.h"
#include "iosp.h"
#include "eintr_wrappers.h"
#include "gtmsecshr.h"
#include "secshr_client.h"
#include "mu_rndwn_file.h"
#include "ftok_sems.h"
#include "ipcrmid.h"
#include "gtmmsg.h"
#include "dbfilop.h"
#include "db_ipcs_reset.h"
#include "jnl.h"
#include "do_semop.h"
#include "anticipatory_freeze.h"

GBLREF uint4		process_id;
GBLREF ipcs_mesg	db_ipcs;
GBLREF gd_region        *gv_cur_region;
GBLREF jnl_gbls_t	jgbl;

error_def (ERR_TEXT);
error_def (ERR_CRITSEMFAIL);
error_def (ERR_DBFILERR);
error_def (ERR_FILEPARSE);

/* mu_rndwn_file gets a database access control semaphore - this counterpart releases it for one region. This module is invoked
 * only by those processes that have previously done the mu_rndwn_file and hence guaranteed to hold the access control semaphore.
 * Because of this, we do not ask for ftok semaphore (ftok_sem_lock) as that would cause an out-of-order request
 * of locks.
 */
boolean_t db_ipcs_reset(gd_region *reg)
{
	int			status, semval, save_errno;
	uint4			ustatus;
	sgmnt_data_ptr_t	csd;
	file_control		*fc;
	unix_db_info		*udi;
	gd_region		*temp_region;
	char			sgmnthdr_unaligned[SGMNT_HDR_LEN + 8], *sgmnthdr_8byte_aligned;
	sgmnt_addrs             *csa;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(reg);
	temp_region = gv_cur_region; 	/* save gv_cur_region wherever there is scope for it to be changed */
	gv_cur_region = reg; /* dbfilop needs gv_cur_region */
	udi = NULL;
	if (NULL != reg->dyn.addr->file_cntl)
		udi = FILE_INFO(reg);
	if ((NULL == udi) || (INVALID_SEMID == udi->semid))
	{
		assert(!reg->open);
		gv_cur_region = temp_region;
		return FALSE;
	}
	assert(udi->grabbed_access_sem);
	csa = &udi->s_addrs;
	sgmnthdr_8byte_aligned = &sgmnthdr_unaligned[0];
	sgmnthdr_8byte_aligned = (char *)ROUND_UP2((unsigned long)sgmnthdr_8byte_aligned, 8);
	csd = (sgmnt_data_ptr_t)&sgmnthdr_8byte_aligned[0];
	fc = reg->dyn.addr->file_cntl;
	fc->op = FC_OPEN;
	status = dbfilop(fc);
	gv_cur_region = temp_region;
	if (SS_NORMAL != status)
	{
		gtm_putmsg(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), status);
                return FALSE;
	}
	LSEEKREAD(udi->fd, (off_t)0, csd, SGMNT_HDR_LEN, status);
	csa->hdr = csd;			/* needed for DB_LSEEKWRITE when instance is frozen */
	if (0 != status)
	{
		gtm_putmsg(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), status);
		CLOSEFILE_RESET(udi->fd, status);	/* resets "udi->fd" to FD_INVALID */
		if (0 != status)
			gtm_putmsg(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), status);
		return FALSE;
	}
	assert((udi->semid == csd->semid) || (INVALID_SEMID == csd->semid));
	semval = semctl(udi->semid, DB_COUNTER_SEM, GETVAL);	/* Get the counter semaphore's value */
	assert(1 <= semval);
	if (1 < semval)
	{
		assert(jgbl.onlnrlbk); /* everyone else will have total standalone access and hence no one else can be attached */
		assert(!reg->read_only); /* ONLINE ROLLBACK must be a read/write process */
		if (!reg->read_only)
		{
			if (0 != (save_errno = do_semop(udi->semid, DB_COUNTER_SEM, -1, SEM_UNDO)))
			{
				gtm_putmsg(VARLSTCNT(8) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg), ERR_TEXT, 2,
						RTS_ERROR_TEXT("db_ipcs_reset - write semaphore release"), save_errno);
				return FALSE;
			}
			assert(1 == (semval = semctl(udi->semid, DB_CONTROL_SEM, GETVAL)));
			if (0 != (save_errno = do_semop(udi->semid, DB_CONTROL_SEM, -1, SEM_UNDO)))
			{
				gtm_putmsg(VARLSTCNT(8) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg), ERR_TEXT, 2,
						RTS_ERROR_TEXT("db_ipcs_reset - access control semaphore release"), save_errno);
				return FALSE;
			}
		}
		/* Since the semval is at least 2, we should NOT remove the semaphore as the other process that still exists
		 * will encounter an error during its gds_rundown. During the gds_rundown of the other process it will realize
		 * that it is the only attached process and will take care of we_are_last_writer cases in gds_rundown.
		 */
	} else
	{	/* We are the only one. Remove the semaphore. Logic in db_init knows to handle when the semaphore is removed
		 * and will retry by creating a new one. But, it is important the semid/shmid fields in the file header is reset
		 * BEFORE removing the semaphore as otherwise the waiting process in db_init will notice the semaphore removal
		 * first and will read the file header and can potentially notice the stale semid/shmid values.
		 */
		if (!reg->read_only DEBUG_ONLY(&& !TREF(gtm_usesecshr)))
		{
			csd->semid = INVALID_SEMID;
			csd->shmid = INVALID_SHMID;
			csd->gt_sem_ctime.ctime = 0;
			csd->gt_shm_ctime.ctime = 0;
			DB_LSEEKWRITE(csa, udi->fn, udi->fd, (off_t)0, csd, SGMNT_HDR_LEN, status);
			if (0 != status)
			{
				gtm_putmsg(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), status);
				CLOSEFILE_RESET(udi->fd, status);	/* resets "udi->fd" to FD_INVALID */
				if (0 != status)
					gtm_putmsg(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), status);
				return FALSE;
			}
		} else
		{
			db_ipcs.semid = INVALID_SEMID;
			db_ipcs.shmid = INVALID_SHMID;
			db_ipcs.gt_sem_ctime = 0;
			db_ipcs.gt_shm_ctime = 0;
			if (!get_full_path((char *)DB_STR_LEN(reg), db_ipcs.fn, (unsigned int *)&db_ipcs.fn_len,
					   GTM_PATH_MAX, &ustatus))
			{
				gtm_putmsg(VARLSTCNT(5) ERR_FILEPARSE, 2, DB_LEN_STR(reg), ustatus);
				return FALSE;
			}
			db_ipcs.fn[db_ipcs.fn_len] = 0;
			WAIT_FOR_REPL_INST_UNFREEZE_SAFE(csa);
			if (0 != (status = send_mesg2gtmsecshr(FLUSH_DB_IPCS_INFO, 0, (char *)NULL, 0)))
			{
				gtm_putmsg(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), status);
				CLOSEFILE_RESET(udi->fd, status);	/* resets "udi->fd" to FD_INVALID */
				if (0 != status)
					gtm_putmsg(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), status);
				return FALSE;
			}
		}
		if (0 != sem_rmid(udi->semid))
		{
			save_errno = errno;
			gtm_putmsg(VARLSTCNT(8) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg), ERR_TEXT, 2,
					RTS_ERROR_TEXT("db_ipcs_reset - sem_rmid"), save_errno);
			return FALSE;
		}
	}
	udi->grabbed_access_sem = FALSE;
	udi->counter_acc_incremented = FALSE;
	CLOSEFILE_RESET(udi->fd, status);	/* resets "udi->fd" to FD_INVALID */
	if (0 != status)
		gtm_putmsg(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), status);
	/* Since we created the ftok semaphore in mu_rndwn_file, we release/remove it now. But, since we are exiting, we
	 * do not WAIT for the ftok semaphore if we did not get it in one shot (IPC_NOWAIT). The process that holds the
	 * ftok will eventually release it and so we are guaranteed that when the last process leaves the database, it will
	 * remove the ftok semaphore as well. This means, not able to lock or release the ftok semaphore is not treated
	 * as an error condition.
	 */
	if (ftok_sem_lock(reg, FALSE, TRUE)) /* immediate=TRUE because we don't want to wait while holding access semaphore */
		ftok_sem_release(reg, TRUE, TRUE);
	udi->semid = INVALID_SEMID;
	udi->shmid = INVALID_SHMID;
	udi->gt_sem_ctime = 0;
	udi->gt_shm_ctime = 0;
	return TRUE;
}
