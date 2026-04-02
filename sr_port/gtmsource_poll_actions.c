/****************************************************************
 *								*
 * Copyright (c) 2001-2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_time.h"
#include "gtm_string.h"
#include "gtm_unistd.h"
#include "gtm_fcntl.h"

#include "gtm_inet.h"
#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmsource_srv_latch.h"
#include "repl_dbg.h"
#include "repl_log.h"
#include "iosp.h"
#include "repl_shutdcode.h"
#include "gt_timer.h"
#include "gtmsource_heartbeat.h"
#include "jnl.h"
#include "repl_filter.h"
#include "util.h"
#include "repl_comm.h"
#include "eintr_wrappers.h"
#include "gtmio.h"
#include "sgtm_putmsg.h"
#include "copy.h"
#include "wcs_flu.h"
#include "wbox_test_init.h"
#include "dpgbldir.h"
#include "repl_errno.h"
#include <sys/wait.h>

GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	int			gtmsource_sock_fd;
GBLREF	gtmsource_state_t	gtmsource_state;
GBLREF	boolean_t		gtmsource_logstats;
GBLREF	int			gtmsource_log_fd;
GBLREF 	FILE			*gtmsource_log_fp;
GBLREF	int			gtmsource_filter;
GBLREF	time_t			gtmsource_last_flush_time;
GBLREF	volatile time_t		gtmsource_now;
GBLREF	gtmsource_options_t	gtmsource_options;
GBLREF	uint4			log_interval;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
#ifdef UNIX
GBLREF	boolean_t		last_seen_freeze_flag;
#endif

LITREF	char			gtm_release_name[];
LITREF	int4			gtm_release_name_len;

error_def(ERR_REPLWARN);
#ifdef UNIX
error_def(ERR_REPLINSTFREEZECOMMENT);
error_def(ERR_REPLINSTFROZEN);
error_def(ERR_REPLINSTUNFROZEN);
#endif

#define	OUT_LINE	1024 + 1

/* Flush dirty buffers for all open regions after an instance unfreeze.
 * When an instance is frozen, flush timers may be canceled. After unfreeze, if no new updates arrive,
 * dirty buffers could remain unflushed indefinitely -- a durability risk. This function iterates over all
 * open regions (not just gv_cur_region) and flushes any that have canceled timers and dirty buffers.
 * Modeled after the region iteration pattern in jnl_file_close_timer().
 */
static void gtmsource_flush_dirty_regions(gtmsource_local_ptr_t gtmsource_local)
{
	gd_addr			*addr_ptr;
	gd_region		*reg, *r_top;
	sgmnt_addrs		*csa;
	node_local_ptr_t	cnl;
	jnl_private_control	*jpc;
	boolean_t		db_needs_flushing, vermismatch;
	gd_region		*save_gv_cur_region;
	sgmnt_addrs		*save_cs_addrs;
	boolean_t		changed_globals;
	fd_type			channel_before;
	uint4			saved_jpc_cycle;
	int			close_status;
	DEBUG_ONLY(boolean_t	skip_flush;)

	for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
	{
		for (reg = addr_ptr->regions, r_top = reg + addr_ptr->n_regions; reg < r_top; reg++)
		{
			if (!reg->open || reg->was_open)
				continue;
			if (!IS_REG_BG_OR_MM(reg))
				continue;
			csa = &FILE_INFO(reg)->s_addrs;
			cnl = csa->nl;
			jpc = csa->jnl;
			/* Check for version mismatch (possible only if DSE is running) */
			if (memcmp(cnl->now_running, gtm_release_name, gtm_release_name_len + 1))
				vermismatch = TRUE;
			else
				vermismatch = FALSE;
			/* Check if flush timers are canceled and buffers are dirty */
			if (!((0 > cnl->wcs_timers) && (cnl->last_wcsflu_tn < csa->ti->curr_tn)))
				continue;
			/* If cnl->donotflush_dbjnl is set, it means mupip recover/rollback was interrupted and
			 * therefore we need not flush shared memory contents to disk as they might be in an
			 * inconsistent state. Moreover, any more flushing will only cause the future
			 * rollback/recover to undo more journal records (PBLKs). In this case, we will go ahead
			 * and remove shared memory (without flushing the contents) in this routine. A reissue of
			 * the recover/rollback command will restore the database to a consistent state.
			 */
			db_needs_flushing = (!cnl->donotflush_dbjnl && !reg->read_only && !vermismatch);
			if (FROZEN_CHILLED(csa) || !db_needs_flushing)
				continue;
#			ifdef DEBUG
			skip_flush = FALSE;
			/* Skip flush operation for specific white-box test cases to avoid assert failures. */
			if ((gtm_white_box_test_case_enabled)
				&& ((WBTEST_ANTIFREEZE_DBBMLCORRUPT == gtm_white_box_test_case_number)
				|| (WBTEST_ANTIFREEZE_OUTOFSPACE == gtm_white_box_test_case_number)))
				skip_flush = TRUE;
			if (skip_flush)
				continue;
#			endif
			/* A shutdown signal can arrive at any point during this code block. If it arrives
			 * during wcs_flu() execution (while we hold crit), the shutdown path will assert
			 * that crit is NOT held. If it arrives during jpc->channel access, accessing shared
			 * memory during shutdown causes failures. If shutdown is signaled, skip entirely.
			 */
			channel_before = NOJNL;
			saved_jpc_cycle = 0;
			/* Check shutdown and jpc validity BEFORE accessing jpc members to avoid failures
			 * during shutdown. During shutdown, shared memory may be detached, making jpc invalid.
			 */
			if ((SHUTDOWN == gtmsource_local->shutdown) || (NULL == jpc))
				return;	/* If shutdown signaled, stop flushing all regions */
			channel_before = jpc->channel;
			/* Save jpc->cycle before wcs_flu(). The wcs_flu() call internally invokes
			 * jnl_ensure_open() -> jnl_file_open() which syncs jpc->cycle with
			 * jpc->jnl_buff->cycle. This would make JNL_FILE_SWITCHED(jpc) return FALSE,
			 * preventing the source server's journal reader (open_newer_gener_jnlfiles)
			 * from detecting journal file switches. Save and restore to preserve reader
			 * state.
			 */
			saved_jpc_cycle = jpc->cycle;
			grab_crit(reg, NOT_APPLICABLE);
			if (SHUTDOWN == gtmsource_local->shutdown)
			{
				rel_crit(reg);
				return;	/* Shutdown signaled -- stop flushing, shutdown handles cleanup */
			}
			/* wcs_flu() uses globals (gv_cur_region, cs_addrs) and asserts cs_addrs matches
			 * gv_cur_region at entry. Temporarily synchronize them with the current region.
			 */
			changed_globals = FALSE;
			if ((reg != gv_cur_region) || (csa != cs_addrs))
			{
				save_gv_cur_region = gv_cur_region;
				save_cs_addrs = cs_addrs;
				gv_cur_region = reg;
				cs_addrs = csa;
				changed_globals = TRUE;
			}
			wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH);
			if (changed_globals)
			{
				gv_cur_region = save_gv_cur_region;
				cs_addrs = save_cs_addrs;
			}
			rel_crit(reg);
			repl_log(gtmsource_log_fp, TRUE, TRUE,
				"Source server flushed dirty buffers for region %s after instance unfreeze\n",
				reg->rname);
			/* Check shutdown and jpc validity AGAIN before accessing jpc members, as shared
			 * memory may have been detached during shutdown.
			 */
			if ((SHUTDOWN == gtmsource_local->shutdown) || (NULL == jpc))
				return;
			/* Close journal file descriptor opened as a side effect of wcs_flu() calling
			 * jnl_ensure_open(). The source server reads journals via its own ctl fd;
			 * leaving this open would leak file descriptors.
			 */
			if ((NOJNL == channel_before) && (NOJNL != jpc->channel))
			{
				fd_type channel_to_close = jpc->channel;
				jpc->channel = NOJNL;
				JNL_FD_CLOSE(channel_to_close, close_status);
			}
			/* Restore jpc->cycle so the source server's journal reader can still detect
			 * journal file switches via JNL_FILE_SWITCHED(jpc). Without this restore,
			 * the cycle sync done by jnl_file_open inside wcs_flu would prevent detecting
			 * switches that happened before this flush but haven't been processed by the
			 * reader yet. See GTMSRC_DO_JNL_FLUSH_IF_POSSIBLE for the identical pattern.
			 */
			jpc->cycle = saved_jpc_cycle;
		}
	}
}

int gtmsource_poll_actions(boolean_t poll_secondary)
{
	/* This function should be called only in active mode, but cannot assert for it */

	gtmsource_local_ptr_t	gtmsource_local;
	time_t			now;
	repl_heartbeat_msg_t	overdue_heartbeat;
	char			*time_ptr;
	char			time_str[CTIME_BEFORE_NL + 1];
	char			print_msg[REPL_MSG_SIZE], msg_str[OUT_LINE];
	boolean_t		log_switched = FALSE;
	int			status;
	time_t			temp_time;
	gtm_time4_t		time4;
	int			filter_exit_status, waitpid_res;

	gtmsource_local = jnlpool->gtmsource_local;
	if (SHUTDOWN == gtmsource_local->shutdown)
	{
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Shutdown signalled\n");
		gtmsource_end(); /* Won't return */
	}
	if (jnlpool->jnlpool_ctl->freeze != last_seen_freeze_flag)
	{
		last_seen_freeze_flag = jnlpool->jnlpool_ctl->freeze;
		if (last_seen_freeze_flag)
		{
			sgtm_putmsg(print_msg, REPL_MSG_SIZE, VARLSTCNT(3) ERR_REPLINSTFROZEN, 1,
					jnlpool->repl_inst_filehdr->inst_info.this_instname);
			repl_log(gtmsource_log_fp, TRUE, FALSE, print_msg);
			sgtm_putmsg(print_msg, REPL_MSG_SIZE, VARLSTCNT(3) ERR_REPLINSTFREEZECOMMENT, 1,
					jnlpool->jnlpool_ctl->freeze_comment);
			repl_log(gtmsource_log_fp, TRUE, TRUE, print_msg);
		}
		else
		{
			sgtm_putmsg(print_msg, REPL_MSG_SIZE, VARLSTCNT(3) ERR_REPLINSTUNFROZEN, 1,
					jnlpool->repl_inst_filehdr->inst_info.this_instname);
			repl_log(gtmsource_log_fp, TRUE, TRUE, print_msg);
			/* Flush dirty buffers across all open regions if flush timers were canceled */
			gtmsource_flush_dirty_regions(gtmsource_local);
		}
	}
	if (GTMSOURCE_CHANGING_MODE != gtmsource_state && GTMSOURCE_MODE_PASSIVE_REQUESTED == gtmsource_local->mode)
	{
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Changing mode from ACTIVE to PASSIVE\n");
		gtmsource_state = GTMSOURCE_CHANGING_MODE;
		gtmsource_local->mode = GTMSOURCE_MODE_PASSIVE;
		UNIX_ONLY(gtmsource_local->gtmsource_state = gtmsource_state;)
		/* Force the update on a transition */
		gtmsource_flush_fh(gtmsource_local->read_jnl_seqno, !gtmsource_srv_latch_held_by_us());
		return (SS_NORMAL);
	}
	if (poll_secondary && GTMSOURCE_CHANGING_MODE != gtmsource_state && GTMSOURCE_WAITING_FOR_CONNECTION != gtmsource_state)
	{
		now = gtmsource_now;
		if (gtmsource_is_heartbeat_overdue(&now, &overdue_heartbeat))
		{
			/* Few platforms don't allow unaligned memory access. Passing ack_time to GTM_CTIME(ctime) may
			 * cause sig. time4 and temp_time are used as temporary variable for converting time to string.*/
			GET_LONG(time4, &overdue_heartbeat.ack_time[0]);
			temp_time = time4;
			GTM_CTIME(time_ptr, &temp_time);
			memcpy(time_str, time_ptr, CTIME_BEFORE_NL);
			time_str[CTIME_BEFORE_NL] = '\0';
			SNPRINTF(msg_str, OUT_LINE, "No response received for heartbeat sent at %s with SEQNO "
					GTM64_ONLY("%lu") NON_GTM64_ONLY("%llu") " in %0.f seconds. Closing connection\n",
					time_str, *(seq_num *)&overdue_heartbeat.ack_seqno[0], difftime(now, temp_time));
			sgtm_putmsg(print_msg, REPL_MSG_SIZE, VARLSTCNT(4) ERR_REPLWARN, 2, LEN_AND_STR(msg_str));
			repl_log(gtmsource_log_fp, TRUE, TRUE, print_msg);
			repl_close(&gtmsource_sock_fd);
			SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
			gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
			UNIX_ONLY(gtmsource_local->gtmsource_state = gtmsource_state;)
			return (SS_NORMAL);
		}

		if (GTMSOURCE_IS_HEARTBEAT_DUE(&now) && !heartbeat_stalled)
		{
			gtmsource_send_heartbeat(&now);
			if ((GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state) || (GTMSOURCE_CHANGING_MODE == gtmsource_state)
					|| (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state))
				return (SS_NORMAL);
		}
	}
	if ((GTMSOURCE_SENDING_JNLRECS == gtmsource_state) /* Flush the file header only with an active connection */
			&& (GTMSOURCE_FH_FLUSH_INTERVAL <= difftime(gtmsource_now, gtmsource_last_flush_time)))
	{
		gtmsource_flush_fh(gtmsource_local->read_jnl_seqno, !gtmsource_srv_latch_held_by_us());
		if (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state)
				return (SS_NORMAL);
	}
	if (0 != gtmsource_local->changelog)
	{
		if (gtmsource_local->changelog & REPLIC_CHANGE_LOGINTERVAL)
		{
			repl_log(gtmsource_log_fp, TRUE, TRUE, "Changing log interval from %u to %u\n",
					log_interval, gtmsource_local->log_interval);
			log_interval = gtmsource_local->log_interval;
			gtmsource_reinit_logseqno(); /* will force a LOG on the first send following the interval change */
		}
		if (gtmsource_local->changelog & REPLIC_CHANGE_LOGFILE)
		{
			log_switched = TRUE;
			repl_log(gtmsource_log_fp, TRUE, TRUE, "Changing log file to %s\n", gtmsource_local->log_file);
#ifdef UNIX
			repl_log_init(REPL_GENERAL_LOG, &gtmsource_log_fd, gtmsource_local->log_file);
			repl_log_fd2fp(&gtmsource_log_fp, gtmsource_log_fd);
#elif defined(VMS)
			util_log_open(STR_AND_LEN(gtmsource_local->log_file));
#else
#error unsupported platform
#endif
			STRCPY(gtmsource_options.log_file, jnlpool->gtmsource_local->log_file);
		}
	        if ( log_switched == TRUE )
        	        repl_log(gtmsource_log_fp, TRUE, TRUE, "Change log to %s successful\n", gtmsource_local->log_file);
		gtmsource_local->changelog = 0;
	}
	if (!gtmsource_logstats && gtmsource_local->statslog)
	{
#ifdef UNIX
		gtmsource_logstats = TRUE;
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Begin statistics logging\n");
#else
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Stats logging not supported on VMS\n");
#endif

	} else if (gtmsource_logstats && !gtmsource_local->statslog)
	{
		gtmsource_logstats = FALSE;
		repl_log(gtmsource_log_fp, TRUE, TRUE, "End statistics logging\n");
	}
	if ((gtmsource_filter & ENABLE_FILTER) && ('\0' == gtmsource_local->filter_cmd[0]))
	{
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Disabling filter\n");
		gtmsource_filter &= ~ENABLE_FILTER;
	}
	if ((gtmsource_filter & EXTERNAL_FILTER) && ('\0' == gtmsource_local->filter_cmd[0]))
	{
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Stopping filter\n");
		repl_stop_filter();
		gtmsource_filter &= ~EXTERNAL_FILTER;
	}
	if (gtmsource_filter & EXTERNAL_FILTER)
	{
		WAITPID(gtmsource_local->src_filter_pid, &filter_exit_status, WNOHANG, waitpid_res);
		if (waitpid_res > 0)
			repl_filter_error(jnlpool->jnlpool_ctl->jnl_seqno, repl_errno = EREPL_FILTERNOTALIVE);
	}
	return (SS_NORMAL);
}
