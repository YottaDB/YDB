/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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
#ifdef UNIX
#include "gtmmsg.h"
GBLREF boolean_t		gtm_environment_init;
#endif

GBLREF	uint4		process_id;

#ifdef VMS
#  define CURRENT_WRITER jb->now_writer
#else
#  define CURRENT_WRITER jb->io_in_prog_latch.latch_pid
#endif

static uint4 jnl_sub_write_attempt(jnl_private_control *jpc, unsigned int *lcnt, uint4 threshold)
{
	sgmnt_addrs		*csa;
	jnl_buffer_ptr_t	jb;
	unsigned int		status;
	boolean_t		was_crit;
	/**** Note static/local */
	static uint4		loop_image_count, writer;	/* assumes calls from one loop at a time */

	error_def(ERR_JNLCNTRL);
	error_def(ERR_JNLFLUSH);
	error_def(ERR_JNLMEMDSK);
	error_def(ERR_JNLPROCSTUCK);
	error_def(ERR_JNLWRTDEFER);

	/*
	 * Some callers of jnl_sub_write_attempt (jnl_flush->jnl_write_attempt, jnl_write->jnl_write_attempt) are in
	 * crit, and some other (jnl_wait->jnl_write_attempt) are not. Callers in crit do not need worry about journal
	 * buffer fields (dskaddr, freeaddr) changing underneath them, but for those not in crit, jnl_sub_write_attempt
	 * might incorrectly return an error status when journal file is swithched. Such callers should check for
	 * journal file switched condition and terminate any loops they are in.
	 */

	jb = jpc->jnl_buff;
	status = ERR_JNLWRTDEFER;
	csa = &FILE_INFO(jpc->region)->s_addrs;
	while (jb->dskaddr < threshold)
	{
#ifdef UNIX
		if (jb->io_in_prog_latch.latch_pid == process_id)
		{
			/* if error condition occurred while doing jnl_qio_start(), then release the lock before waiting */
			/* note that this is done only in UNIX because Unix does synchronous I/O */
			jb->image_count = 0;
			RELEASE_SWAPLOCK(&jb->io_in_prog_latch);
		}
		if (!jb->io_in_prog_latch.latch_pid)
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
			status = jnl_qio_start(jpc);
#else
#error UNSUPPORTED PLATFORM
#endif
		if (SS_NORMAL == status)
		{
			if (jb->free == jb->dsk && jb->dskaddr < threshold) /* there's nothing in the buffer to write, */
				status = ERR_JNLMEMDSK;			    /* but this process still isn't satisfied */
			break;
		}
		if ((writer != CURRENT_WRITER) || (1 == *lcnt))
		{
			writer = CURRENT_WRITER;
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
			if ((CURRENT_WRITER == process_id) && (jpc->qio_active == TRUE) && (jb->iosb.cond == -2))
		        {	/* this an "impossible" condition where the private flag and the io have lost sync */
				GTMASSERT;	/* this should only occur in VMS; secshr_db_clnup should clear the problem */
			}
		)
		if (writer == CURRENT_WRITER)
		{
			if (FALSE == (was_crit = csa->now_crit))
				grab_crit(jpc->region);	/* ??? be sure that this can't cause a deadlock */
			if (VMS_ONLY(0 == writer ||) FALSE == is_proc_alive(writer, jb->image_count))
			{	/* no one home, clear the semaphore; */
				BG_TRACE_PRO_ANY(csa, jnl_blocked_writer_lost);
				VMS_ONLY(jb->io_in_prog = 0);
				UNIX_ONLY(compswap(&jb->io_in_prog_latch, writer, LOCK_AVAILABLE));
				if (FALSE == was_crit)
					rel_crit(jpc->region);
				*lcnt = 1;
				continue;
			}
			if (FALSE == was_crit)
				rel_crit(jpc->region);
			/* this is the interesting case: a process is stuck */
			BG_TRACE_PRO_ANY(csa, jnl_blocked_writer_stuck);
			jpc->status = status;
			jnl_send_oper(jpc, ERR_JNLFLUSH);
			send_msg(VARLSTCNT(3) ERR_JNLPROCSTUCK, 1, CURRENT_WRITER);
			*lcnt = 1;	/* ??? is it necessary to limit this, and if so, how ??? */
			status = ERR_JNLPROCSTUCK;
			break;
		}
		break;
	}
	if ((threshold > jb->freeaddr) || (csa->now_crit && jb->dskaddr > jb->freeaddr))
	{	/* jb->dskaddr > jb->freeaddr => out of design condition, or jnl was switched
		 * threshold > jb->freeaddr => somebody decremented jb->freeaddr after we computed threshold, or jnl was switched
		 */
		assert(!csa->now_crit);
		status = ERR_JNLCNTRL;
	}
	return status;
}

uint4 jnl_write_attempt(jnl_private_control *jpc, uint4 threshold)
{
	jnl_buffer_ptr_t	jb;
	unsigned int		lcnt, prev_lcnt, cnt, proc_stuck_cnt;
	sgmnt_addrs		*csa;
	unsigned int		status;
	boolean_t		was_crit, jnlfile_lost;

	error_def(ERR_JNLCNTRL);
	error_def(ERR_JNLMEMDSK);
	error_def(ERR_JNLPROCSTUCK);
	error_def(ERR_JNLWRTDEFER);
	error_def(ERR_JNLFLUSHNOPROG);
	error_def(ERR_JNLFLUSH);
	error_def(ERR_TEXT);

	jb = jpc->jnl_buff;
	csa = &FILE_INFO(jpc->region)->s_addrs;

	assert(!csa->now_crit || threshold <= jb->freeaddr);
	for (prev_lcnt = lcnt = cnt = 1, proc_stuck_cnt = 0; (csa->now_crit || NOJNL != jpc->channel) && (jb->dskaddr < threshold);
	     lcnt++, prev_lcnt = lcnt, cnt++)
	{
		status = jnl_sub_write_attempt(jpc, &lcnt, threshold);
		if (!csa->now_crit && JNL_FILE_SWITCHED(jpc->region))
		{
			jpc->status = SS_NORMAL;
			return SS_NORMAL;
		}
		if (SS_NORMAL == status)
		{
			if (JNL_FLUSH_PROG_TRIES > lcnt)
			{
				proc_stuck_cnt = 0;
				continue;
			}
			jpc->status = SS_NORMAL;
			jnl_send_oper(jpc, ERR_JNLFLUSH);
			send_msg(VARLSTCNT(8) ERR_JNLFLUSHNOPROG, 2, JNL_LEN_STR(csa->hdr),
				 ERR_TEXT, 2, LEN_AND_LIT("Could not flush all the buffered journal data"));
			GTMASSERT; /* too many attempts to flush journal data */
		}
		if ((ERR_JNLCNTRL == status) || (ERR_JNLMEMDSK == status))
		{
			assert(FALSE);
			/* Take a DUMP for debugging */
			/********************** Disable till gtm_fork_n_core doesn't mess with signals in the parent
			UNIX_ONLY(
				if (JNL_ENABLED(csa->hdr))
					gtm_fork_n_core(); CMNT_START do it just once; jnl_file_lost will close journaling CMNT_END
				else;
			)
			*********************************************************************************************/
			if (was_crit = csa->now_crit) /* CAUTION : Assigment */
				jb->blocked = 0;
			else
				grab_crit(jpc->region);	/* ??? is this subject to any possible deadlocks ??? */
			jnlfile_lost = FALSE;
			if (jb->dskaddr > jb->freeaddr || threshold > jb->freeaddr || jb->free != (jb->freeaddr % jb->size))
			{ /* if it's possible to recover from JNLCNTRL, or JNLMEMDSK errors, do it here.
			   * jnl_file_lost is disruptive - Vinaya, June 05, 2001 */
				jnlfile_lost = TRUE;
				jnl_file_lost(jpc, status);
			}
			if (!was_crit)
				rel_crit(jpc->region);
			if (!jnlfile_lost)
				continue;
			else
				return status;
		}
		if (prev_lcnt != lcnt)
		{
			assert(1 == lcnt);
			if (ERR_JNLWRTDEFER == status)
			{
				/* Change of writer */
				if (JNL_FLUSH_PROG_TRIES <= cnt)
				{
					send_msg(VARLSTCNT(8) ERR_JNLFLUSHNOPROG, 2, JNL_LEN_STR(csa->hdr),
						ERR_TEXT, 2, LEN_AND_LIT("No progress even with multiple writers"));
					GTMASSERT;
				}
				proc_stuck_cnt = 0;
			} else if (ERR_JNLPROCSTUCK == status && (JNL_FLUSH_PROG_FACTOR <= ++proc_stuck_cnt))
			{
				send_msg(VARLSTCNT(8) ERR_JNLFLUSHNOPROG, 2, JNL_LEN_STR(csa->hdr), ERR_TEXT, 2,
					LEN_AND_LIT("Progress prevented by a process stuck flushing journal data"));
				UNIX_ONLY(
					if (gtm_environment_init)
					{
						/* We do this, because we often do not check syslogs */
						gtm_putmsg(VARLSTCNT(8) ERR_JNLFLUSHNOPROG, 2, JNL_LEN_STR(csa->hdr), ERR_TEXT, 2,
							LEN_AND_LIT("Progress prevented by a process stuck flushing journal data"));
						proc_stuck_cnt = 0;
						continue;
					}
				)
				GTMASSERT;
			}
		}
	}
	return SS_NORMAL;
}
