/****************************************************************
 *								*
 *	Copyright 2009, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_fcntl.h"
#include "gtm_stat.h"
#include "gtm_unistd.h"
#include <sys/mman.h>
#include <sys/shm.h>
#include "gtm_permissions.h"
#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_tempnam.h"
#include "gtm_time.h"

#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "iosp.h"
#include "error.h"
#include "cli.h"
#include "eintr_wrappers.h"
#include "gtmio.h"
#include "gtm_file_stat.h"
#include "gtmimagename.h"
#include "interlock.h"
#include "wcs_phase2_commit_wait.h"
#include "util.h"
#include "ipcrmid.h"
#include "shmpool.h"
#include "db_snapshot.h"
#include "ss_lock_facility.h"

GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data		*cs_data;
GBLREF	gd_region		*gv_cur_region;
GBLREF	uint4			process_id;
GBLREF	enum gtmImageTypes	image_type;

error_def(ERR_COMMITWAITSTUCK);
error_def(ERR_SYSCALL);
error_def(ERR_SSFILCLNUPFAIL);
error_def(ERR_SSSHMCLNUPFAIL);

#define CLEANUP_SHADOW_FILE(preserve_snapshot, shadow_file)							 		\
{																\
	int		status;											 		\
	struct stat	stat_buf;												\
	int		save_errno;												\
																\
	if (!preserve_snapshot && (-1 != stat(shadow_file, &stat_buf)))			 					\
	{															\
		status = UNLINK(shadow_file);									 		\
		if (0 != status)												\
		{														\
			save_errno = errno;											\
			if (IS_MUMPS_IMAGE)										 	\
				send_msg(VARLSTCNT(5) ERR_SSFILCLNUPFAIL, 2, LEN_AND_STR(shadow_file), save_errno);	 	\
			else													\
				gtm_putmsg(VARLSTCNT(5) ERR_SSFILCLNUPFAIL, 2, LEN_AND_STR(shadow_file), save_errno);		\
		}														\
	}															\
}																\

#define CLEANUP_SHARED_MEMORY(ss_shmid)												\
{																\
	int		save_errno;												\
	struct shmid_ds ss_shmstat;												\
																\
	if ((INVALID_SHMID != ss_shmid) && (0 == shmctl((int)ss_shmid, IPC_STAT, &ss_shmstat)))					\
	{															\
		if (0 != shmctl((int)ss_shmid, IPC_RMID, &ss_shmstat))								\
		{														\
			save_errno = errno;											\
			if (IS_MUMPS_IMAGE)											\
				send_msg(VARLSTCNT(6) ERR_SSSHMCLNUPFAIL, 3, LEN_AND_LIT("shmctl"), ss_shmid, save_errno);	\
			else													\
				gtm_putmsg(VARLSTCNT(6) ERR_SSSHMCLNUPFAIL, 3, LEN_AND_LIT("shmctl"), ss_shmid, save_errno);	\
		}														\
	}															\
}																\

#define ADJUST_SHARED_MEMORY_FIELDS(cnl, ss_shm_ptr)				\
{										\
	if (0 < cnl->num_snapshots_in_effect)					\
		cnl->num_snapshots_in_effect--;					\
	if (0 == cnl->num_snapshots_in_effect)					\
	{									\
		CLEAR_SNAPSHOTS_IN_PROG(cnl);					\
		cnl->ss_shmid = INVALID_SHMID;					\
	}									\
	SS_DEFAULT_INIT_POOL(ss_shm_ptr);					\
	cnl->ss_shmcycle++;							\
}

void		ss_release(snapshot_context_ptr_t *ss_ctx)
{
	int			status, ss_shmsize;
	long			ss_shmid;
	boolean_t		preserve_snapshot = FALSE;
	sgmnt_addrs		*csa;
	DEBUG_ONLY(sgmnt_data	*csd;)
	node_local_ptr_t	cnl;
	shm_snapshot_ptr_t	ss_shm_ptr;
	snapshot_context_ptr_t	lcl_ss_ctx;
	boolean_t		was_crit;
	struct shmid_ds		ss_shmstat;
	struct stat		stat_buf;
	char			shadow_file[GTM_PATH_MAX];
	ss_proc_status		cur_state;

	assert(NULL != cs_addrs && (NULL != gv_cur_region));
	csa = cs_addrs;
	cnl = csa->nl;
	was_crit = csa->now_crit;
	DEBUG_ONLY(csd = csa->hdr;)

	if (NULL == ss_ctx) /* orphaned cleanup */
	{
		assert(ss_lock_held_by_us(gv_cur_region));
		ss_shm_ptr = (shm_snapshot_ptr_t)(SS_GETSTARTPTR(csa));
		DBG_ENSURE_PTR_WITHIN_SS_BOUNDS(csa, (sm_uc_ptr_t)ss_shm_ptr);
		ss_shmid = ss_shm_ptr->ss_info.ss_shmid;
		CLEANUP_SHADOW_FILE(FALSE, ss_shm_ptr->ss_info.shadow_file);
		CLEANUP_SHARED_MEMORY(ss_shmid);
		ADJUST_SHARED_MEMORY_FIELDS(cnl, ss_shm_ptr);
		return;
	}
	assert(NULL != *ss_ctx);
	lcl_ss_ctx = *ss_ctx;
	cur_state = lcl_ss_ctx->cur_state;
	if (SNAPSHOT_INIT_DONE == cur_state)
	{
		/* Ensure that we don't try to grab_crit and wait for Phase2 commits to complete if we are dying */
		assert(!process_exiting);
		ss_shm_ptr = lcl_ss_ctx->ss_shm_ptr;
		DBG_ENSURE_PTR_WITHIN_SS_BOUNDS(csa, (sm_uc_ptr_t)ss_shm_ptr);
		ss_shmsize = ss_shm_ptr->ss_info.ss_shmsize;
		assert(SNAPSHOTS_IN_PROG(cnl));
		assert(ss_shm_ptr->in_use);
		preserve_snapshot = ss_shm_ptr->preserve_snapshot;
		/* Irrespective of whether there was an error or not, announce GT.M that it should now stop writing before images.
		 * After rel_crit below any process that enters transaction logic will either see cnl->snapshot_in_prog set to FALSE
		 * or a change in cnl->ss_shmid and cnl->ss_shmcycle in which case it will restart itself in t_end or tp_tend.
		 */
		if (!was_crit)
			grab_crit(gv_cur_region);
		assert(cs_data == cs_addrs->hdr);
		if (dba_bg == cs_data->acc_meth)
		{
			/* Now that we have crit, wait for any pending phase2 updates to finish. Since phase2 updates happen
			 * outside of crit, we dont want them to keep writing to the snapshot file even after the snapshot
			 * is complete. This is needed as otherwise a GT.M process might see the value of csa->snapshot_in_prog
			 * as TRUE and before it can proceed any further(starvation, maybe), we went ahead and removed the
			 * snapshot file(below). Now, if the GT.M process resumes execution, it might end up writing
			 * the before image to a temporary file which is no longer available.
			 */
			if (cnl->wcs_phase2_commit_pidcnt && !wcs_phase2_commit_wait(csa, NULL))
			{
				assert(FALSE);
				gtm_putmsg(VARLSTCNT(7) ERR_COMMITWAITSTUCK, 5, process_id, 1,
					   cnl->wcs_phase2_commit_pidcnt, DB_LEN_STR(gv_cur_region));
			}
		}
		ADJUST_SHARED_MEMORY_FIELDS(cnl, ss_shm_ptr);
		if (!was_crit)
			rel_crit(gv_cur_region);
		if (preserve_snapshot)
		{
			assert(lcl_ss_ctx->shadow_vbn >= csd->start_vbn);
			assert(ss_shmsize == (lcl_ss_ctx->shadow_vbn - csd->start_vbn) * DISK_BLOCK_SIZE);
			LSEEKWRITE(lcl_ss_ctx->shdw_fd, 0, (sm_uc_ptr_t)(lcl_ss_ctx->start_shmaddr), ss_shmsize, status);
			if (0 != status)
			{
				util_out_print("!/Failed while writing the snapshot file header. ",
						TRUE);
				assert(FALSE);
			}
		}
	}
	STRCPY(shadow_file, lcl_ss_ctx->shadow_file);
	ss_shmid = lcl_ss_ctx->attach_shmid;
	ss_destroy_context(lcl_ss_ctx);
	/* Do cleanup depending on what state of the snapshot init we are in */
	switch(cur_state)
	{
		case SNAPSHOT_INIT_DONE:
		case AFTER_SHM_CREAT:
			/* Remove shared memory identifier. Note, because of a race condition, it is possible that some GT.M
			 * process is still attached to this identifier. In such a case, the identifier will be marked to be
			 * delted by the OS until number of processes attached to this shared segment becomes zero. GT.M
			 * at the end of each transaction, will try to detach itself from this shared memory segment and will
			 * eventually lead to number of attached processes becoming zero.*/
			CLEANUP_SHARED_MEMORY(ss_shmid);
		/* intentional fall through */
		case AFTER_SHADOW_FIL_CREAT:
			CLEANUP_SHADOW_FILE(preserve_snapshot, shadow_file);
		/* intentional fall through */
		case BEFORE_SHADOW_FIL_CREAT:
			if (NULL != *ss_ctx)
			{
				free(*ss_ctx);
				*ss_ctx = NULL;
			}
			break;
		default:
			assert(FALSE);
			GTMASSERT;
	}
	return;
}
