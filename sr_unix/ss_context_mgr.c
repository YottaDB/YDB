/****************************************************************
 *								*
 *	Copyright 2009, 2010 Fidelity Information Services, Inc	*
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
#include "repl_sp.h"
#include "gtm_file_stat.h"
#include "util.h"
#include "gtm_caseconv.h"
#include "gt_timer.h"
#include "is_proc_alive.h"
#include "is_file_identical.h"
#include "dbfilop.h"
#include "wcs_flu.h"
#include "jnl.h"
#include "interlock.h"
#include "sleep_cnt.h"
#include "mupip_exit.h"
#include "trans_log_name.h"
#include "gtmimagename.h"
#include "send_msg.h"
#include "gtm_logicals.h"
#include "memcoherency.h"
#ifdef __MVS__
#include "gtm_zos_io.h"
#endif
#include "shmpool.h"
#include "db_snapshot.h"
#include "do_shmat.h"

GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	enum gtmImageTypes	image_type;
GBLREF	uint4			process_id;

boolean_t	ss_create_context(snapshot_context_ptr_t lcl_ss_ctx, int ss_shmcycle)
{
	shm_snapshot_ptr_t		ss_shm_ptr;
	node_local_ptr_t		cnl;
	sgmnt_addrs			*csa;
	int				shdw_fd, status;
	void				*ss_shmaddr;
	ZOS_ONLY(int			realfiletag;)

	error_def(ERR_SYSCALL);
	ZOS_ONLY(error_def(ERR_BADTAG);)
	ZOS_ONLY(error_def(ERR_TEXT);)

	assert(NULL != cs_addrs);
	csa = cs_addrs;
	cnl = csa->nl;

	ss_destroy_context(lcl_ss_ctx); /* Before creating a new one relinquish the existing one */
	lcl_ss_ctx->ss_shmcycle = ss_shmcycle;
	/* SS_MULTI: In case of multiple snapshots, we can't assume that the first index of the snapshot structure in the db
	 * shared memory is the currently active snapshot.
	 */
	assert(1 == MAX_SNAPSHOTS); /* This assert will fail when we increase the MAX_SNAPSHOTS per region */
	ss_shm_ptr = (shm_snapshot_ptr_t)SS_GETSTARTPTR(csa);
	DBG_ENSURE_PTR_WITHIN_SS_BOUNDS(csa, (sm_uc_ptr_t)ss_shm_ptr);
	/* The below memory barrier will ensure that if ever the shmcycle is modified in ss_initiate, we get the up to date
	 * values of the other members in the snapshot structure (copied below).
	 */
	SHM_READ_MEMORY_BARRIER;
	STRCPY(lcl_ss_ctx->shadow_file, ss_shm_ptr->ss_info.shadow_file);
	lcl_ss_ctx->total_blks = ss_shm_ptr->ss_info.total_blks;
	lcl_ss_ctx->shadow_vbn = ss_shm_ptr->ss_info.shadow_vbn;
	lcl_ss_ctx->ss_shm_ptr = ss_shm_ptr;
	lcl_ss_ctx->nl_shmid = cnl->ss_shmid;
	/* Attach to the shared memory created by snapshot */
	if (INVALID_SHMID != lcl_ss_ctx->nl_shmid)
	{
		if (-1 == (sm_long_t)(ss_shmaddr = do_shmat(lcl_ss_ctx->nl_shmid, 0, 0)))
		{
			status = errno;
			/* It's possible that by the time we attach to the shared memory, the identifier has been removed from the
			 * system by INTEG which completed it's processing.
			 */
			assert((EIDRM == status) || (EINVAL == status));
			assert(INVALID_SHMID == lcl_ss_ctx->attach_shmid);
			lcl_ss_ctx->cur_state = SNAPSHOT_SHM_ATTACH_FAIL;
			lcl_ss_ctx->failure_errno = status;
			return FALSE;
		}
		lcl_ss_ctx->attach_shmid = lcl_ss_ctx->nl_shmid;
		lcl_ss_ctx->start_shmaddr = (sm_uc_ptr_t)ss_shmaddr;
		lcl_ss_ctx->bitmap_addr = ((sm_uc_ptr_t)ss_shmaddr + SNAPSHOT_HDR_SIZE);
	} else
	{	/* No ongoing snapshots. Do early return. t_end and tp_tend during validation will identify this change in state
		 * and will turn off snapshot activities
		 */
		lcl_ss_ctx->start_shmaddr = NULL;
		lcl_ss_ctx->bitmap_addr = NULL;
		lcl_ss_ctx->cur_state = SNAPSHOT_NOT_INITED;
		return FALSE;
	}
	OPENFILE(lcl_ss_ctx->shadow_file, O_RDWR, shdw_fd);
	lcl_ss_ctx->shdw_fd = shdw_fd;
	if (FD_INVALID == shdw_fd)
	{
		assert((NULL != ss_shmaddr)
			&& (-1 != lcl_ss_ctx->attach_shmid)); /* shared memory attach SHOULD have succeeded */
		if (-1 == SHMDT(ss_shmaddr))
		{
			status = errno;
			assert(FALSE);
			rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("Error with shmdt"), CALLFROM, status);
		}
		lcl_ss_ctx->attach_shmid = lcl_ss_ctx->nl_shmid = INVALID_SHMID; /* reset shmid since we failed */
		lcl_ss_ctx->cur_state = SHADOW_FIL_OPEN_FAIL;
		lcl_ss_ctx->failure_errno = errno;
		return FALSE;
	}
#	ifdef __MVS__
	if (-1 == gtm_zos_tag_to_policy(shdw_fd, TAG_BINARY, &realfiletag))
		TAG_POLICY_SEND_MSG(ss_shm_ptr->ss_info.shadow_file, errno, realfiletag, TAG_BINARY);
#	endif
	lcl_ss_ctx->cur_state = SNAPSHOT_INIT_DONE;
	return TRUE;
}

boolean_t	ss_destroy_context(snapshot_context_ptr_t lcl_ss_ctx)
{
	int				status;

	error_def(ERR_SYSCALL);

	assert(NULL != lcl_ss_ctx);
	if (FD_INVALID != lcl_ss_ctx->shdw_fd)
	{
		CLOSEFILE_RESET(lcl_ss_ctx->shdw_fd, status);
	}
	if (INVALID_SHMID != lcl_ss_ctx->attach_shmid)
	{
		if (0 != SHMDT((void *)(lcl_ss_ctx->start_shmaddr)))
		{
			status = errno;
			assert(FALSE);
			rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("Error with shmdt"), CALLFROM, status);
		}
	}
	/* Invalidate the context */
	DEFAULT_INIT_SS_CTX(lcl_ss_ctx);
	return TRUE;
}
