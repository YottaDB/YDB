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

#include "gtm_stdio.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "send_msg.h"
#include "caller_id.h"
#include "sleep_cnt.h"
#include "interlock.h"
#include "add_inter.h"
#include "wcs_backoff.h"
#include "wcs_get_space.h"
#include "wcs_sleep.h"
#include "wcs_flu.h"
#include "wcs_wt.h"
#include "gtm_time.h"
#include "gtm_string.h"
#include "cli.h"
#include "util.h"
#include "wbox_test_init.h"
#include "have_crit.h"
#include "anticipatory_freeze.h"
#include "gtmio.h"

#ifdef DEBUG_FREEZE
GBLREF	boolean_t	caller_id_flag;
#endif
GBLREF	bool		in_mupip_freeze;
GBLREF	uint4		process_id;
GBLREF	boolean_t	debug_mupip;
GBLREF	jnl_gbls_t	jgbl;
GBLREF	gd_region	*gv_cur_region;

# define FREEZE_ID	((0 == user_id) ? FROZEN_BY_ROOT : user_id)
# define FREEZE_MATCH	process_id
# define OWNERSHIP	(in_mupip_freeze ? (csd->freeze == freeze_id) : (csd->image_count == FREEZE_MATCH))
# define NEG_STR(VAL)	((VAL) ? "" : "NO")

#ifdef DEBUG_FREEZE
#define SEND_FREEZEID(STATE, CSA)							\
{											\
	caller_id_flag = FALSE;								\
	send_msg_csa(CSA_ARG(CSA) VARLSTCNT(9) ERR_FREEZEID, 7, LEN_AND_STR(STATE),	\
			DB_LEN_STR(region),						\
			freeze_id, FREEZE_MATCH, caller_id());				\
	caller_id_flag = TRUE;								\
}

error_def(ERR_FREEZEID);
#endif

error_def(ERR_DBFREEZEON);
error_def(ERR_DBFREEZEOFF);

freeze_status	region_freeze(gd_region *region, boolean_t freeze, boolean_t override, boolean_t wait_for_kip,
				uint4 online, boolean_t flush_sync)
{
	freeze_status		rval;

	rval = region_freeze_main(region, freeze, override, wait_for_kip, online, flush_sync);

	if (REG_FREEZE_SUCCESS == rval)
		rval = region_freeze_post(region);

	return rval;
}

freeze_status	region_freeze_main(gd_region *region, boolean_t freeze, boolean_t override, boolean_t wait_for_kip,
					uint4 online, boolean_t flush_sync)
{
	uint4			freeze_id, sleep_counter;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	jnl_private_control	*jpc;
	unix_db_info    	*udi;
	uint4			standalone;
	uint4			jnl_status;
	int4			epoch_interval_sav;
	int			dummy_errno, save_errno;
	uint4			was_online;
	char			time_str[CTIME_BEFORE_NL + 2];       /* for GET_CUR_TIME macro */
	boolean_t		cleanup_autorelease, jnl_switch_done, was_crit;
	freeze_status		rval;
	unsigned int		lcnt;

	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	freeze_id = FREEZE_ID;
	csa = &FILE_INFO(region)->s_addrs;
	csd = csa->hdr;
	cnl = csa->nl;
	udi = FILE_INFO(region);
	standalone = udi->grabbed_access_sem;
	was_crit = csa->now_crit;
	if (freeze)
	{
		if (!was_crit)
		{
			if (online && !standalone)
			{	/* Flush db without crit so we have less work to do while holding crit. */
				SET_GBL_JREC_TIME;
				JNL_ENSURE_OPEN_WCS_WTSTART(csa, region, csd->n_bts, NULL, FALSE, dummy_errno);
			}
			grab_crit(region);
		}
		INCR_INHIBIT_KILLS(cnl);
		if (OWNERSHIP)
		{
			DECR_INHIBIT_KILLS(cnl);
			if (!was_crit)
				rel_crit(region);
			return REG_FREEZE_SUCCESS;
		}
		if (!override && csd->freeze)
		{
			DECR_INHIBIT_KILLS(cnl);
			if (!was_crit)
				rel_crit(region);
			return REG_ALREADY_FROZEN;
		}
		/* If override is TRUE we need not wait for KIP to become zero */
		sleep_counter = 1;
		if (!override && wait_for_kip && (0 < csd->kill_in_prog))
		{
			if (!was_crit)
				rel_crit(region);
			/* MUPIP FREEZE/INTEG and BACKUP's DBG qualifier prints extra debug messages while waiting for KIP */
			if (debug_mupip)
			{
				GET_CUR_TIME(time_str);
				util_out_print("!/MUPIP INFO: !AD : Start kill-in-prog wait for database !AD", TRUE,
					       CTIME_BEFORE_NL, time_str, DB_LEN_STR(region));
			}
			do
			{
				if (!was_crit)
					grab_crit(region);
				if (!csd->kill_in_prog)
					break;
				if (!was_crit)
					rel_crit(region);
				wcs_sleep(sleep_counter);
			} while (MAX_CRIT_TRY > sleep_counter++);
			if (debug_mupip)
			{
				GET_CUR_TIME(time_str);
				util_out_print("!/MUPIP INFO: !AD : Done with kill-in-prog wait on region", TRUE,
					       CTIME_BEFORE_NL, time_str);
			}
		}
		/* if can't ever be true when override is true. */
		if (MAX_CRIT_TRY <= sleep_counter)
		{
			DECR_INHIBIT_KILLS(cnl);
			if (!was_crit)
				rel_crit(region);
			return REG_HAS_KIP;
		}
		if (online && !was_crit && !standalone)
		{	/* Flush any remaining writes with crit/without latch so any wcs_flu() below doesn't have to,
			 * so we can keep the time the latch is held to a minimum.
			 */
			JNL_ENSURE_OPEN_WCS_WTSTART(csa, region, csd->n_bts, NULL, FALSE, dummy_errno);
		}
		/* Return value of "grab_latch" does not need to be checked because we pass GRAB_LATCH_INDEFINITE_WAIT as timeout */
		grab_latch(&cnl->freeze_latch, GRAB_LATCH_INDEFINITE_WAIT);
		rval = REG_FREEZE_SUCCESS;
		jnl_switch_done = FALSE;
		jpc = csa->jnl;
		if (online && JNL_ENABLED(csd) && !standalone)
		{	/* We can't switch journal files while frozen, so switch up front to allow as many updates as possible
			 * while frozen.
			 */
			/* Before writing to jnlfile, adjust jgbl.gbl_jrec_time if needed to maintain time order of jnl
			 * records. This needs to be done BEFORE the jnl_ensure_open.
			 */
			ADJUST_GBL_JREC_TIME(jgbl, jpc->jnl_buff);
			jnl_status = jnl_ensure_open(region, csa);
			if (0 == jnl_status)
			{	/* An online freeze can cause a long delay between epochs, but mur_tp_resolve_time()
				 * assumes that we have epochs every jfh->epoch_interval, allowing MAX_EPOCH_DELAY for
				 * wiggle room. It also assumes that this interval relates reasonably to the maximum
				 * journal record time, so we can't just set an arbitrarily large value. Since we just
				 * set the jgbl_rec_time above, and it should be large enough, we can use that, until
				 * it eventually gets too large, at which time we can base it on INT_MAX.
				 * Setting the cs_data->epoch_interval before the journal file switch causes it to
				 * be copied to jfh->epoch_interval, and we restore it below. The restored value will
				 * then appear in the next journal file to which we will switch on the unfreeze.
				 */
				epoch_interval_sav = csd->epoch_interval;
				csd->epoch_interval = MIN(jgbl.gbl_jrec_time, INT_MAX) - MAX_EPOCH_DELAY;
				jnl_switch_done = TRUE;
				if (EXIT_ERR == SWITCH_JNL_FILE(jpc))
					rval = REG_JNL_SWITCH_ERROR;
				csd->epoch_interval = epoch_interval_sav;
			} else
				rval = REG_JNL_OPEN_ERROR;
		}
		if (REG_FREEZE_SUCCESS != rval)
		{
			if (!was_crit)
				rel_crit(region);
			rel_latch(&cnl->freeze_latch);
			return rval;
		}
		SIGNAL_WRITERS_TO_STOP(cnl);
		WAIT_FOR_WRITERS_TO_STOP(cnl, lcnt, MAXWTSTARTWAIT);
		csd->freeze = freeze_id;		/* the order of this line and the next is important */
		csd->image_count = FREEZE_MATCH;
		csa->freeze = TRUE;
		cnl->freeze_online = online;
		DEBUG_ONLY(cnl->freezer_waited_for_kip = wait_for_kip;)
		SIGNAL_WRITERS_TO_RESUME(cnl);
		DECR_INHIBIT_KILLS(cnl);
		if (flush_sync)
		{
			if (!wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH))
				rval = REG_FLUSH_ERROR;
			assert((freeze_id == csd->freeze) && (FREEZE_MATCH == csd->image_count));
		}
		if (!was_crit)
			rel_crit(region);
		if (flush_sync)
			DO_DB_FSYNC_OUT_OF_CRIT_IF_NEEDED(region, csa, jpc, jpc->jnl_buff); /* Do WCSFLU_SYNC_EPOCH out of crit */
#		ifdef DEBUG_FREEZE
		SEND_FREEZEID("FREEZE", csa);
#		endif
		rel_latch(&cnl->freeze_latch);
		send_msg_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_DBFREEZEON, 5, REG_LEN_STR(region), NEG_STR(override), NEG_STR(online),
								NEG_STR(online & CHILLED_AUTORELEASE_MASK));
		return rval;
	}
	/* !freeze */
	/* Return value of "grab_latch" does not need to be checked because we pass GRAB_LATCH_INDEFINITE_WAIT as timeout */
	grab_latch(&cnl->freeze_latch, GRAB_LATCH_INDEFINITE_WAIT);
	/* If there is no freeze, but there is a freeze_online, then there was an autorelease, which needs to be cleaned up
	 * by the normal unfreeze procedure. However, we only do it in MUPIP FREEZE -OFF to ensure that the user gets a warning.
	 */
	cleanup_autorelease = ((0 == csd->freeze) && CHILLED_AUTORELEASE(csa) && in_mupip_freeze);
	if ((0 == csd->freeze) && !cleanup_autorelease)
	{
		rel_latch(&cnl->freeze_latch);
		return REG_FREEZE_SUCCESS;
	}
	if (override || OWNERSHIP || cleanup_autorelease)
	{
		was_online = cnl->freeze_online;
		csd->image_count = 0;		/* the order of this line and the next is important */
		csd->freeze = 0;
		cnl->freeze_online = FALSE;
		csa->freeze = FALSE;
		rel_latch(&cnl->freeze_latch);
#		ifdef DEBUG_FREEZE
		SEND_FREEZEID("UNFREEZE", csa);
#		endif
		rval = REG_FREEZE_SUCCESS;
		if (was_online && !standalone)
		{
			csa->needs_post_freeze_online_clean = TRUE;
		}
		if (flush_sync && !(was_online && JNL_ENABLED(csd)))
		{
			csa->needs_post_freeze_flushsync = TRUE;
		}
		send_msg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_DBFREEZEOFF, 4, REG_LEN_STR(region), NEG_STR(override),
								NEG_STR(cleanup_autorelease));
		return rval;
	} else
		rel_latch(&cnl->freeze_latch);
	return REG_ALREADY_FROZEN;
}

freeze_status region_freeze_post(gd_region *region)
{
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	jnl_private_control	*jpc;
	uint4			jnl_status;
	int			dummy_errno;
	boolean_t		was_crit;
	freeze_status		rval;

	csa = &FILE_INFO(region)->s_addrs;
	csd = csa->hdr;
	was_crit = csa->now_crit;
	rval = REG_FREEZE_SUCCESS;
	if (csa->needs_post_freeze_online_clean)
	{
		SET_GBL_JREC_TIME;
		if (!was_crit)
		{	/* Flush db without crit before journal file switch/epoch which require crit. */
			JNL_ENSURE_OPEN_WCS_WTSTART(csa, region, csd->n_bts, NULL, FALSE, dummy_errno);
		}
		if (JNL_ENABLED(csd))
		{	/* Do an extra journal file switch to undo the epoch_interval override above. */
			jpc = csa->jnl;
			if (!was_crit)
				grab_crit(region);
			/* If another freeze managed to sneak in after we dropped the latch, skip the switch. */
			if (!FROZEN(csd))
			{
				ADJUST_GBL_JREC_TIME(jgbl, jpc->jnl_buff);
				jnl_status = jnl_ensure_open(region, csa);
				if (0 == jnl_status)
				{
					if (EXIT_ERR == SWITCH_JNL_FILE(jpc))
						rval = REG_JNL_SWITCH_ERROR;
				} else
					rval = REG_JNL_OPEN_ERROR;
			}
			if (!was_crit)
				rel_crit(region);
		}
		csa->needs_post_freeze_online_clean = FALSE;
	}
	if (csa->needs_post_freeze_flushsync)
	{
		if (!wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH | WCSFLU_SYNC_EPOCH))
			rval = REG_FLUSH_ERROR;
		csa->needs_post_freeze_flushsync = FALSE;
	}
	return rval;
}
