/****************************************************************
 *								*
 * Copyright (c) 2009-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "gtm_time.h"
#ifdef DEBUG
#include "gtm_ipc.h"
#endif

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
#include "memcoherency.h"
#ifdef __MVS__
#include "gtm_zos_io.h"
#endif
#include "shmpool.h"
#include "db_snapshot.h"
#include "do_shmat.h"
#include "have_crit.h"

GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	enum gtmImageTypes	image_type;
GBLREF	uint4			process_id;
GBLREF	int			process_exiting;
#ifdef DEBUG
GBLREF	volatile int		in_os_signal_handler;
#endif

error_def(ERR_SYSCALL);

boolean_t	ss_create_context(snapshot_context_ptr_t lcl_ss_ctx, int ss_shmcycle)
{
	shm_snapshot_ptr_t		ss_shm_ptr;
	node_local_ptr_t		cnl;
	sgmnt_addrs			*csa;
	int				shdw_fd, status;
	void				*ss_shmaddr;
	ZOS_ONLY(int			realfiletag;)

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
			assert(SHM_REMOVED(status));
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
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("Error with shmdt"), CALLFROM, status);
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
	int		save_shdw_fd, status;
	long		save_attach_shmid;
	intrpt_state_t	prev_intrpt_state;

	/* If we are exiting, don't bother with SHMDT as all of this is going away anyways. This is also necessary
	 * in the case we reach here while inside a signal handler as shmdt() is not async-signal safe.
	 */
	if (process_exiting)
		return TRUE;
	/* The "shmdt" call (inside the SHMDT macro) done below is not async-signal-safe so assert that we are not
	 * inside an os invoked signal handler if ever we come to this function.
	 */
	assert(!in_os_signal_handler);
	assert(NULL != lcl_ss_ctx);
	/* Note: CLOSEFILE_RESET can invoke "eintr_handling_check()" after the "close()" call succeeds. And that
	 * can in turn recurse into "ss_destroy_context()" which would call CLOSEFILE_RESET again on the same fd
	 * and fail because the fd has already been closed. An example call stack of the recursion is
	 *	t_end -> ss_create_context -> ss_destroy_context -> CLOSEFILE_RESET -> eintr_handling_check
	 *		-> deferred_signal_handler -> check_for_deferred_timers -> timer_handler
	 *		-> jnl_file_close_timer -> ss_destroy_context -> CLOSEFILE_RESET
	 * To avoid this duplicate close, save a copy of "lcl_ss_ctx->shdw_fd" (in a local variable), then
	 * clear it before invoking the CLOSEFILE_RESET macro. Additionally (and more importantly), disallow timer
	 * signals in a short window where we take a local copy and clear the global copy. This ensures the local copy
	 * is reliable and has not been tampered with by a timer interrupt.
	 */
	DEFER_INTERRUPTS(INTRPT_IN_SS_INITIATE, prev_intrpt_state);
	save_shdw_fd = lcl_ss_ctx->shdw_fd;
	lcl_ss_ctx->shdw_fd = FD_INVALID;
	ENABLE_INTERRUPTS(INTRPT_IN_SS_INITIATE, prev_intrpt_state);
	if (FD_INVALID != save_shdw_fd)
		CLOSEFILE_RESET(save_shdw_fd, status);
#	ifdef DEBUG
	if (ydb_white_box_test_case_enabled && (WBTEST_FAKE_SS_SHMDT_WINDOW == ydb_white_box_test_case_number))
	{
		FPRINTF(stdout, "About to delete shm at %x\n", lcl_ss_ctx->start_shmaddr);
		if (INVALID_SHMID == lcl_ss_ctx->attach_shmid)
			lcl_ss_ctx->attach_shmid = 0 ;	/* ensure / fake that it's not invalid */
	}
#	endif
	/* Just like we needed to save the "shdw_fd" in a local variable above, we also need to save "attach_shmid" in a local
	 * variable below and clear the shared/global copy before attempting the SHMDT. Otherwise, it is possible a timer
	 * interrupt happens just before the SHMDT and as part of that "jnl_file_close_timer" gets invoked and does the actual
	 * SHMDT in a nested call of "ss_destroy_context" and returns back to the outer "ss_destroy_context" which will then
	 * attempt the SHMDT but will see a failure because the shmdt has already happened in a nested call.
	 * Just like in the "shdw_fd" case, we need to disable timer interrupts in a short window where we take a local copy
	 * and clear the global copy. This ensures the local copy is reliable and has not been tampered with by a timer interrupt.
	 */
	DEFER_INTERRUPTS(INTRPT_IN_SS_INITIATE, prev_intrpt_state);
	save_attach_shmid = lcl_ss_ctx->attach_shmid;
	lcl_ss_ctx->attach_shmid = INVALID_SHMID;
	ENABLE_INTERRUPTS(INTRPT_IN_SS_INITIATE, prev_intrpt_state);
	if (INVALID_SHMID != save_attach_shmid)
	{
		if (0 != SHMDT((void *)(lcl_ss_ctx->start_shmaddr)))
		{
			char	buf[128];

			status = errno;
			assert(FALSE);
			SNPRINTF(buf, sizeof(buf), "Shared segment address: 0x"lvaddr, lcl_ss_ctx->start_shmaddr);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(12) ERR_SYSCALL, 5, LEN_AND_LIT("shmdt()"), CALLFROM,
				      ERR_TEXT, 2, STRLEN(buf), buf, status);
		}
	}
	/* Invalidate the context */
	DEFAULT_INIT_SS_CTX(lcl_ss_ctx);
	return TRUE;
}
