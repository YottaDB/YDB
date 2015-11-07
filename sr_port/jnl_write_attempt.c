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

#ifdef UNIX
#include "repl_msg.h"			/* needed for gtmsource.h */
#include "gtmsource.h"			/* needed for jnlpool_addrs typedef */
#include "gtmmsg.h"
#include "io.h"                 /* needed by gtmsecshr.h */
#include "gtmsecshr.h"          /* for continue_proc */
#endif
#include "gtm_c_stack_trace.h"

#ifdef UNIX
GBLREF	jnlpool_addrs	jnlpool;
#endif
GBLREF	pid_t		process_id;
GBLREF	uint4		image_count;

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
	exact_check = was_crit && (threshold == jb->freeaddr);	/* see comment in jnl_write_attempt() for why this is needed */
	while (exact_check ? (jb->dskaddr != threshold) : (jb->dskaddr < threshold))
	{
#ifdef UNIX
		if (jb->io_in_prog_latch.u.parts.latch_pid == process_id)
		{
			/* if error condition occurred while doing jnl_qio_start(), then release the lock before waiting */
			/* note that this is done only in UNIX because Unix does synchronous I/O */
			jb->image_count = 0;
			RELEASE_SWAPLOCK(&jb->io_in_prog_latch);
		}
		if (!jb->io_in_prog_latch.u.parts.latch_pid)
			status = jnl_qio_start(jpc);
#elif defined VMS
		if (lib$ast_in_prog())
		{
			if (!jb->io_in_prog)
			{
				assert(jb->blocked == process_id);
				jnl_start_ast(jpc);
				if (jb->now_writer == process_id)
					status = jb->iosb.cond;
			}
			break;		/* no fancy stuff within an AST */
		} else if (!jb->io_in_prog)
		{	/* Note down jpc->new_dskaddr/new_dsk into local variables so we get a consistent copy of these two
			 * variables for checking them later.
			 */
			new_dskaddr = jpc->new_dskaddr;
			new_dsk = jpc->new_dsk;
			status = jnl_qio_start(jpc);
		}
#else
#error UNSUPPORTED PLATFORM
#endif
		if (SS_NORMAL == status)
		{
#			if defined VMS
			/* Check if JNLCNTRL error was signalled by jnl_qio_start(). Note that it does not explicitly
			 * return this error since it in turn calls an AST routine jnl_start_ast that actually has the
			 * qio lock (and hence can look at dskaddr/dsk without any concurrency issues). But jpc will
			 * have two fields new_dskaddr/new_dsk set to what dskaddr/dsk were right after obtaining the
			 * qio lock but before releasing it in case of a JNLCNTRL error. We use those two values to
			 * recheck if this is a JNLCNTRL error situation and if so return that error from here.
			 * Note that we cannot use fields from jpc since they could be set by an AST that pops right
			 * after we check new_dskaddr below but before we fetch the value of new_dsk. So it is important
			 * to use the local variables which we know are a consistent snapshot of jpc->new_dskaddr/new_dsk.
			 * The only consequence of this approach is that in case there is a dskaddr/dsk inconsistency,
			 * it will be detected by the local variables in the next iteration (not the first time around).
			 */
			if ((new_dskaddr % jb->size) != new_dsk)
			{
				assert(gtm_white_box_test_case_enabled
					&& (WBTEST_JNL_FILE_LOST_DSKADDR == gtm_white_box_test_case_number));
				status = ERR_JNLCNTRL;
			}
#			endif
			break;
		}
		UNIX_ONLY(assert(ERR_JNLWRTNOWWRTR != status);)	/* dont have asynchronous jnl writes in Unix */
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
		VMS_ONLY(
			if ((CURRENT_JNL_IO_WRITER(jb) == process_id) && (jpc->qio_active == TRUE) && (jb->iosb.cond == -2))
		        {	/* this an "impossible" condition where the private flag and the io have lost sync */
				GTMASSERT;	/* this should only occur in VMS; secshr_db_clnup should clear the problem */
			}
		)
		if (writer == CURRENT_JNL_IO_WRITER(jb))
		{
			if (!was_crit)
				grab_crit(jpc->region);	/* jnl_write_attempt has an assert about have_crit that this relies on */
			if (VMS_ONLY(0 == writer ||) FALSE == is_proc_alive(writer, jb->image_count))
			{	/* no one home, clear the semaphore; */
				BG_TRACE_PRO_ANY(csa, jnl_blocked_writer_lost);
				jnl_send_oper(jpc, ERR_JNLQIOSALVAGE);
				VMS_ONLY(jb->io_in_prog = 0);
				UNIX_ONLY(COMPSWAP_UNLOCK(&jb->io_in_prog_latch, writer, jb->image_count, LOCK_AVAILABLE, 0));
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
			UNIX_ONLY(continue_proc(writer));
			break;
		}
		break;
	}
	if ((threshold > jb->freeaddr)
		|| (csa->now_crit && ((jb->dskaddr > jb->freeaddr) || (jb->free != (jb->freeaddr % jb->size)))))
	{	/* threshold > jb->freeaddr => somebody decremented jb->freeaddr after we computed threshold, or jnl was switched
		 * jb->dsk != jb->freeaddr % jb->size => out of design condition
		 * jb->dskaddr > jb->freeaddr => out of design condition, or jnl was switched
		 */
		status = ERR_JNLCNTRL;
	}
	return status;
}

uint4 jnl_write_attempt(jnl_private_control *jpc, uint4 threshold)
{
	jnl_buffer_ptr_t	jb;
	unsigned int		lcnt, prev_lcnt, cnt;
	sgmnt_addrs		*csa;
	unsigned int		status;
	boolean_t		was_crit, jnlfile_lost, exact_check;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	jb = jpc->jnl_buff;
	csa = &FILE_INFO(jpc->region)->s_addrs;
	was_crit = csa->now_crit;

	/* If holding crit and input threshold matches jb->freeaddr, then we need to wait in the loop as long as dskaddr
	 * is not EQUAL to threshold. This is because if dskaddr is lesser than threshold we need to wait. If ever it
	 * becomes greater than threshold, it is an out-of-design situation (since dskaddr has effectively become > freeaddr)
	 * and so we need to trigger "jnl_file_lost" which is done in "jnl_sub_write_attempt" so it is important to invoke
	 * that routine (in the for loop below). Hence the need to do an exact match instead of a < match. If not holding
	 * crit or input threshold does not match jb->freeaddr, then dskaddr becoming GREATER than threshold is a valid
	 * condition so we should do a (dskaddr < threshold), not a (dskaddr != threshold) check in that case.
	 */
	exact_check = was_crit && (threshold == jb->freeaddr);
	assert(!was_crit || threshold <= jb->freeaddr);
	/* Check that we either own crit on the current region or we DONT own crit on ANY region. This is relied upon by
	 * the grab_crit calls (done in jnl_write_attempt and jnl_sub_write_attempt) to ensure no deadlocks are possible.
	 */
	assert(was_crit || (0 == have_crit(CRIT_HAVE_ANY_REG)));
	for (prev_lcnt = lcnt = cnt = 1;
		(was_crit || (NOJNL != jpc->channel)) && (exact_check ? jb->dskaddr != threshold : jb->dskaddr < threshold);
		lcnt++, prev_lcnt = lcnt, cnt++)
	{
		status = jnl_sub_write_attempt(jpc, &lcnt, threshold);
		if (JNL_FILE_SWITCHED(jpc))
		{	/* If we are holding crit, the journal file switch could happen in the form of journaling getting
			 * turned OFF (due to disk space issues etc.)
			 */
			jpc->status = SS_NORMAL;
			return SS_NORMAL;
		}
		if (SS_NORMAL == status)
		{
			/* In VMS, jnl writes are asynchronous. The above call to "jnl_sub_write_attempt" has returned
			 * SS_NORMAL status. This means the jnl qio lock is not in use by anyone else and is up for grabs.
			 * We would have scheduled a jnl qio write through a sys$dclast call. We have no control of when
			 * the AST routine "jnl_start_ast" will actually get control and start the write. Until then
			 * we dont want to keep reinvoking "jnl_sub_write_attempt" in a hard spin loop. So sleep.
			 * In Unix, writes are synchronous so SS_NORMAL status return implies we have completed a jnl
			 * write and "jb->dskaddr" is closer to "threshold" than it was in the previous iteration.
			 * A sleep at this point will only slow things down unnecessarily. Hence no sleep if Unix.
			 */
			VMS_ONLY(wcs_sleep(lcnt);)
			continue;
		}
		if ((ERR_JNLCNTRL == status) || (ERR_JNLACCESS == status)
			|| (csa->now_crit
				&& (ERR_JNLWRTDEFER != status) && (ERR_JNLWRTNOWWRTR != status) && (ERR_JNLPROCSTUCK != status)))
		{	/* If JNLCNTRL or if holding crit and not waiting for some other writer (or self in VMS)
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
			if (jb->free_update_pid)
			{
				FIX_NONZERO_FREE_UPDATE_PID(csa, jb);
			} else
			{
				assert((gtm_white_box_test_case_enabled
					&& (WBTEST_JNL_FILE_LOST_DSKADDR == gtm_white_box_test_case_number))
				       UNIX_ONLY(|| TREF(gtm_test_fake_enospc) || WBTEST_ENABLED(WBTEST_RECOVER_ENOSPC)));
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
			}
			if (!was_crit)
				rel_crit(jpc->region);
			if (!jnlfile_lost)
				continue;
			else
				return status;
		}
#		ifdef UNIX
		if ((ERR_JNLWRTDEFER == status) && IS_REPL_INST_FROZEN)
		{	/* Check if the write was deferred because the instance is frozen.
			 * In that case, wait until the freeze is lifted instead of wasting time spinning on the latch
			 * in jnl_qio.
			 */
			 WAIT_FOR_REPL_INST_UNFREEZE(csa);
		}
#		endif
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
	return SS_NORMAL;
}
