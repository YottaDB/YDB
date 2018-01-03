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

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "gdsbgtr.h"
#include "filestruct.h"
#include "iosp.h"
#include "jnl.h"
#include "lockconst.h"
#include "interlock.h"
#include "sleep_cnt.h"
#include "send_msg.h"
#include "wcs_sleep.h"
#include "is_proc_alive.h"
#include "compswap.h"
#include "is_file_identical.h"
#include "have_crit.h"
#include "wbox_test_init.h"
#include "anticipatory_freeze.h"
#include "repl_msg.h"			/* needed for gtmsource.h */
#include "gtmsource.h"			/* needed for jnlpool_addrs typedef */
#include "gtmmsg.h"
#include "io.h"                 /* needed by gtmsecshr.h */
#include "gtmsecshr.h"          /* for continue_proc */
#include "gtm_c_stack_trace.h"
#include "sleep.h"

#define	ITERATIONS_100K	100000

GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	uint4			process_id;
GBLREF	uint4			image_count;

error_def(ERR_JNLACCESS);
error_def(ERR_JNLCNTRL);
error_def(ERR_JNLFLUSH);
error_def(ERR_JNLFLUSHNOPROG);
error_def(ERR_JNLPROCSTUCK);
error_def(ERR_JNLQIOSALVAGE);
error_def(ERR_JNLWRTDEFER);
error_def(ERR_JNLWRTNOWWRTR);
error_def(ERR_TEXT);

static uint4 jnl_sub_write_attempt(jnl_private_control *jpc, unsigned int *lcnt, uint4 threshold)
{
	sgmnt_addrs		*csa;
	jnl_buffer_ptr_t	jb;
	unsigned int		status;
	boolean_t		was_crit, exact_check;
	/**** Note static/local */
	static uint4		loop_image_count, writer;	/* assumes calls from one loop at a time */
	uint4			new_dskaddr, new_dsk;
	uint4			dskaddr, freeaddr, free, rsrv_freeaddr;
	uint4			phase2_commit_index1;
	static uint4		stuck_cnt = 0;

	/* Some callers of jnl_sub_write_attempt (jnl_flush->jnl_write_attempt, jnl_write->jnl_write_attempt) are in
	 * crit, and some other (jnl_wait->jnl_write_attempt) are not. Callers in crit do not need worry about journal
	 * buffer fields (dskaddr, freeaddr) changing underneath them, but for those not in crit, jnl_sub_write_attempt
	 * might incorrectly return an error status when journal file is switched. Such callers should check for
	 * journal file switched condition and terminate any loops they are in.
	 */
	jb = jpc->jnl_buff;
	status = ERR_JNLWRTDEFER;
	csa = &FILE_INFO(jpc->region)->s_addrs;
	was_crit = csa->now_crit;
	exact_check = was_crit && (threshold == jb->rsrv_freeaddr); /* see comment in jnl_write_attempt() for why this is needed */
	while (exact_check ? (jb->dskaddr != threshold) : (jb->dskaddr < threshold))
	{
		if (jb->io_in_prog_latch.u.parts.latch_pid == process_id)
		{
			/* if error condition occurred while doing jnl_qio_start(), then release the lock before waiting */
			/* note that this is done only in UNIX because Unix does synchronous I/O */
			jb->image_count = 0;
			RELEASE_SWAPLOCK(&jb->io_in_prog_latch);
		}
		if (!jb->io_in_prog_latch.u.parts.latch_pid)
			status = jnl_qio_start(jpc);
		if (SS_NORMAL == status)
			break;
		assert(ERR_JNLWRTNOWWRTR != status);	/* don't have asynchronous jnl writes in Unix */
		if ((ERR_JNLWRTNOWWRTR != status) && (ERR_JNLWRTDEFER != status))
			return status;
		if ((writer != CURRENT_JNL_IO_WRITER(jb)) || (1 == *lcnt))
		{
			writer = CURRENT_JNL_IO_WRITER(jb);
			loop_image_count = jb->image_count;
			*lcnt = 1;	/* !!! this should be detected and limited by the caller !!! */
			break;
		}
		if (*lcnt <= JNL_MAX_FLUSH_TRIES)
		{
			wcs_sleep(*lcnt);
			break;
		}
		if (writer == CURRENT_JNL_IO_WRITER(jb))
		{
			if (!was_crit)
				grab_crit(jpc->region);	/* jnl_write_attempt has an assert about have_crit that this relies on */
			if (FALSE == is_proc_alive(writer, jb->image_count))
			{	/* no one home, clear the semaphore; */
				BG_TRACE_PRO_ANY(csa, jnl_blocked_writer_lost);
				jnl_send_oper(jpc, ERR_JNLQIOSALVAGE);
				COMPSWAP_UNLOCK(&jb->io_in_prog_latch, writer, jb->image_count, LOCK_AVAILABLE, 0);
				if (!was_crit)
					rel_crit(jpc->region);
				*lcnt = 1;
				continue;
			}
			if (!was_crit)
				rel_crit(jpc->region);
			/* this is the interesting case: a process is stuck */
			BG_TRACE_PRO_ANY(csa, jnl_blocked_writer_stuck);
			jpc->status = status;
			jnl_send_oper(jpc, ERR_JNLFLUSH);
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(3) ERR_JNLPROCSTUCK, 1, writer);
			stuck_cnt++;
			GET_C_STACK_FROM_SCRIPT("JNLPROCSTUCK", process_id, writer, stuck_cnt);
			*lcnt = 1;	/* ??? is it necessary to limit this, and if so, how ??? */
			status = ERR_JNLPROCSTUCK;
			continue_proc(writer);
			break;
		}
		break;
	}
	/* Note: If phase2 jnl commits are still in progress outside of crit, we cannot accurately compare "jb->free" vs
	 * "jb->freeaddr" (because a phase2 commit could be updating them as part of the UPDATE_JBP_RSRV_FREEADDR macro).
	 * "jb->phase2_commit_index1 == jb->phase2_commit_index2" indicates no phase2 commits are in progress.
	 * In that case, go ahead with the free/freeaddr check.
	 */
	rsrv_freeaddr = jb->rsrv_freeaddr;
	dskaddr = jb->dskaddr;
	phase2_commit_index1 = jb->phase2_commit_index1;
	SHM_READ_MEMORY_BARRIER;	/* See "jnl_phase2_cleanup" for comment & corresponding SHM_WRITE_MEMORY_BARRIER */
	freeaddr = jb->freeaddr;
	free = jb->free;
	if ((threshold > rsrv_freeaddr)
		|| (csa->now_crit
			&& ((dskaddr > freeaddr)
				|| ((phase2_commit_index1 == jb->phase2_commit_index2) && ((free != (freeaddr % jb->size)))))))
	{	/* threshold > jb->rsrv_freeaddr => somebody decremented jb->freeaddr after we computed threshold,
		 *	OR jnl was switched (in which case we should not be holding crit)
		 * jb->free != jb->freeaddr % jb->size => out of design condition
		 * jb->dskaddr > jb->freeaddr => out of design condition, or jnl was switched (again we should not be in crit)
		 */
		status = ERR_JNLCNTRL;
		assert((dskaddr > freeaddr) && gtm_white_box_test_case_enabled
				&& (WBTEST_JNL_FILE_LOST_DSKADDR == gtm_white_box_test_case_number)
			|| (JNL_FILE_SWITCHED(jpc) && !was_crit));
	}
	return status;
}

uint4 jnl_write_attempt(jnl_private_control *jpc, uint4 threshold)
{
	jnl_buffer_ptr_t	jb;
	uint4			prev_freeaddr;
	unsigned int		lcnt, prev_lcnt, cnt;
	sgmnt_addrs		*csa;
	jnlpool_addrs_ptr_t	save_jnlpool;
	unsigned int		status;
	boolean_t		was_crit, jnlfile_lost, exact_check;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	jb = jpc->jnl_buff;
	csa = &FILE_INFO(jpc->region)->s_addrs;
	save_jnlpool = jnlpool;
	if (csa->jnlpool && (csa->jnlpool != jnlpool))
		jnlpool = csa->jnlpool;
	was_crit = csa->now_crit;

	/* If holding crit and input threshold matches jb->rsrv_freeaddr, then we need to wait in the loop as long as dskaddr
	 * is not EQUAL to threshold. This is because if dskaddr is lesser than threshold we need to wait. If ever it
	 * becomes greater than threshold, it is an out-of-design situation (since dskaddr has effectively become > rsrv_freeaddr)
	 * and so we need to trigger "jnl_file_lost" which is done in "jnl_sub_write_attempt" so it is important to invoke
	 * that routine (in the for loop below). Hence the need to do an exact match instead of a < match. If not holding
	 * crit or input threshold does not match jb->rsrv_freeaddr, then dskaddr becoming GREATER than threshold is a valid
	 * condition so we should do a (dskaddr < threshold), not a (dskaddr != threshold) check in that case.
	 */
	exact_check = was_crit && (threshold == jb->rsrv_freeaddr);
	assert(!was_crit || threshold <= jb->rsrv_freeaddr);
	/* Check that we either own crit on the current region or we don't own crit on ANY region. This is relied upon by
	 * the grab_crit calls (done in jnl_write_attempt and jnl_sub_write_attempt) to ensure no deadlocks are possible.
	 */
	assert(was_crit || (0 == have_crit(CRIT_HAVE_ANY_REG)));
	for (prev_lcnt = lcnt = cnt = 1;
		(was_crit || (NOJNL != jpc->channel)) && (exact_check ? jb->dskaddr != threshold : jb->dskaddr < threshold);
		lcnt++, prev_lcnt = lcnt, cnt++)
	{
		prev_freeaddr = jb->freeaddr;
		if (prev_freeaddr < threshold)
		{
			JNL_PHASE2_CLEANUP_IF_POSSIBLE(csa, jb); /* phase2 commits in progress. Clean them up if possible */
			if (prev_freeaddr == jb->freeaddr)
			{	/* No cleanup happened implies process in phase2 commit is still alive.
				 * Give it some time to finish its job. Not sleeping here could result in a spinloop
				 * below (due to the "continue" below under the "SS_NORMAL == status" if check).
				 */
				BG_TRACE_PRO_ANY(csa, jnl_phase2_cleanup_if_possible);
				SLEEP_USEC(1, FALSE);
				if (!was_crit && (0 == (lcnt % ITERATIONS_100K)))
				{	/* We do not have crit and have slept a while (100K iterations of 1-micro-second each
					 * == a total of ~ 100 milli-seconds). See if crit can be obtained that way the
					 * JNL_PHASE2_CLEANUP_IF_POSSIBLE macro will attempt "jnl_phase2_salvage" if needed.
					 * An example scenario where this is needed is if a process is in "gds_rundown"->"jnl_wait"
					 * and does not hold crit but has written journal records after those written by another
					 * process which was kill -9ed in phase2 of its jnl commit. Not doing this check would
					 * cause the process in gds_rundown to be indefinitely stuck in "jnl_wait".
					 */
					if (grab_crit_immediate(jpc->region, OK_FOR_WCS_RECOVER_TRUE))
					{
						JNL_PHASE2_CLEANUP_IF_POSSIBLE(csa, jb); /* phase2 commits in progress.
											  * Clean them up if possible.
											  */
						rel_crit(jpc->region);
					}
				}
			}
		}
		status = jnl_sub_write_attempt(jpc, &lcnt, threshold);
		if (JNL_FILE_SWITCHED(jpc))
		{	/* If we are holding crit, the journal file switch could happen in the form of journaling getting
			 * turned OFF (due to disk space issues etc.)
			 */
			jpc->status = SS_NORMAL;
			if (save_jnlpool != jnlpool)
				jnlpool = save_jnlpool;
			return SS_NORMAL;
		}
		if (SS_NORMAL == status)
		{
			/* In Unix, writes are synchronous so SS_NORMAL status return implies we have completed a jnl
			 * write and "jb->dskaddr" is closer to "threshold" than it was in the previous iteration.
			 * A sleep at this point will only slow things down unnecessarily. Hence no sleep if Unix.
			 */
			continue;
		}
		if ((ERR_JNLCNTRL == status) || (ERR_JNLACCESS == status)
			|| (csa->now_crit
				&& (ERR_JNLWRTDEFER != status) && (ERR_JNLWRTNOWWRTR != status) && (ERR_JNLPROCSTUCK != status)))
		{	/* If JNLCNTRL or if holding crit and not waiting for some other writer
			 * better turn off journaling and proceed with database update to avoid a database hang.
			 */
			if (was_crit)
				jb->blocked = 0;
			else
			{
				assertpro(0 == have_crit(CRIT_HAVE_ANY_REG));
				grab_crit(jpc->region);	/* jnl_write_attempt has an assert about have_crit that this relies on */
			}
			jnlfile_lost = FALSE;
			assert((gtm_white_box_test_case_enabled
				&& (WBTEST_JNL_FILE_LOST_DSKADDR == gtm_white_box_test_case_number))
			       || TREF(gtm_test_fake_enospc) || WBTEST_ENABLED(WBTEST_RECOVER_ENOSPC));
			if (JNL_ENABLED(csa->hdr))
			{	/* We ignore the return value of jnl_file_lost() since we always want to report the journal
				 * error, whatever its error handling method is.  Also, an operator log will be sent by some
				 * callers (t_end()) only if an error is returned here, and the operator log is wanted in
				 * those cases.
				 */
				jnl_file_lost(jpc, status);
				jnlfile_lost = TRUE;
			}
			/* Else journaling got closed concurrently by another process by invoking "jnl_file_lost"
			 * just before we got crit. Do not invoke "jnl_file_lost" again on the same journal file.
			 * Instead continue and next iteration will detect the journal file has switched and terminate.
			 */
			if (!was_crit)
				rel_crit(jpc->region);
			if (!jnlfile_lost)
				continue;
			else
			{
				if (save_jnlpool != jnlpool)
					jnlpool = save_jnlpool;
				return status;
			}
		}
		if (ERR_JNLWRTDEFER == status)
		{
			if (DBG_ASSERT(!csa->jnlpool || (csa->jnlpool == jnlpool)) IS_REPL_INST_FROZEN_JPL(csa->jnlpool))
			{	/* Check if the write was deferred because the instance is frozen.
				 * In that case, wait until the freeze is lifted instead of wasting time spinning on the latch
				* in jnl_qio.
				*/
				WAIT_FOR_REPL_INST_UNFREEZE(csa);
			}
		}
		if ((ERR_JNLWRTDEFER != status) && (ERR_JNLWRTNOWWRTR != status) && (ERR_JNLPROCSTUCK != status))
		{	/* If holding crit, then jnl_sub_write_attempt would have invoked jnl_file_lost which would have
			 * caused the JNL_FILE_SWITCHED check at the beginning of this for loop to succeed and return from
			 * this function so we should never have gotten here. Assert accordingly. If not holding crit,
			 * wait for some crit holder to invoke jnl_file_lost. Until then keep sleep looping indefinitely.
			 * The sleep in this case is not time-limited because the callers of jnl_write_attempt (particularly
			 * jnl_wait) do not check its return value so they assume success returns from this function. It is
			 * non-trivial to change the interface and code of all callers to handle the error situation so we
			 * instead choose to sleep indefinitely here until some crit process encounters the same error and
			 * triggers jnl_file_lost processing which will terminate the loop due to the JNL_FILE_SWITCHED check.
			 */
			assert(!csa->now_crit);
			wcs_sleep(lcnt);
		} else if (prev_lcnt != lcnt)
		{
			assert(1 == lcnt);
			if ((ERR_JNLWRTDEFER == status) && (JNL_FLUSH_PROG_TRIES <= cnt))
			{	/* Change of writer */
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_JNLFLUSHNOPROG, 2, JNL_LEN_STR(csa->hdr),
					ERR_TEXT, 2, LEN_AND_LIT("No progress even with multiple writers"));
				cnt = 0;
			}
		}
	}
	if (save_jnlpool != jnlpool)
		jnlpool = save_jnlpool;
	return SS_NORMAL;
}
