/****************************************************************
 *								*
 * Copyright (c) 2018-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2023-2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include "io.h"
#include "iosp.h"
#include "iotimer.h"
#include "stringpool.h"
#include "op.h"
#include "gdsroot.h"
#include "gdskill.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gtm_fcntl.h"		/* for AIX's silly open to open64 translations */
#include "gdsfhead.h"
#include "gdscc.h"
#include "filestruct.h"
#include "buddy_list.h"	/* for tp.h */
#include "jnl.h"
#include "tp.h"
#include "send_msg.h"
#include "gtmmsg.h"		/* for gtm_putmsg() prototype */
#include "change_reg.h"
#include "setterm.h"
#include "getzposition.h"
#include "min_max.h"
#include "mvalconv.h"
#include "have_crit.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "time.h"
#include "gt_timer.h"
#include "ztimeout_routines.h"
#include "deferred_events.h"
#include "error_trap.h"
#include "indir_enum.h"
#include "zwrite.h"
#include "xfer_enum.h"
#include "fix_xfer_entry.h"
#include "gvname_info.h"
#include "op_merge.h"
#include "zshow.h"
#include "gtm_signal.h"
#include "deferred_events_queue.h"
#include "wbox_test_init.h"
#include "gtmio.h"
#include "compiler.h"
#include "gtm_common_defs.h"
#include "gtm_time.h"
#ifdef DEBUG_DEFERRED_EVENT
#include "funsvn.h"
#include "nametabtyp.h"
#include "namelook.h"
#endif
#include "is_equ.h"		/* for MV_FORCE_NSTIMEOUT macro */

GBLREF	boolean_t		ydb_white_box_test_case_enabled, ztrap_explicit_null;
GBLREF	dollar_ecode_type	dollar_ecode;
GBLREF	int			dollar_truth, ydb_white_box_test_case_number;
GBLREF	stack_frame		*frame_pointer;
GBLREF	volatile boolean_t	dollar_zininterrupt;
GBLREF	volatile int4		outofband;

LITREF	mval			literal_minusone, literal_null;
#ifdef DEBUG_DEFERRED_EVENT
LITREF unsigned char    svn_index[];
LITREF nametabent       svn_names[];
LITREF svn_data_type    svn_data[];
#endif

STATICDEF mstr			vector;

error_def(ERR_ZTIMEOUT);

#define ZTIMEOUT_TIMER_ID (TID)&check_and_set_ztimeout
#define ZTIMEOUT_QUEUE_ID &ztimeout_set
#define MAX_FORMAT_LEN	250

void check_and_set_ztimeout(mval *inp_val)
{
	boolean_t	only_timeout;
	char		*colon_ptr, *local_str_end, *local_str_val;
	int 		read_len;
	uint8		nsec_timeout;		/* timeout in nanoseconds */
	int4		rc;
	intrpt_state_t	prev_intrpt_state;
	mval		*zt_sec_ptr, ztimeout_seconds, ztimeout_vector;
	sigset_t	savemask;
	ABS_TIME	cur_time, end_time;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(inp_val);
	MV_FORCE_NUMD(inp_val);
	read_len = inp_val->str.len;
	local_str_val = inp_val->str.addr;
	local_str_end = local_str_val + read_len;
	for (colon_ptr = local_str_val; (colon_ptr < local_str_end) && (':' != *colon_ptr); colon_ptr++)
		;
	only_timeout = (colon_ptr >= local_str_end);
	ztimeout_vector = (TREF(dollar_ztimeout)).ztimeout_vector;
	if (!only_timeout && (colon_ptr >= local_str_val))
	{	/* vector change */
		if (ztimeout_vector.str.len && ztimeout_vector.str.addr)
			memcpy(&(TREF(dollar_ztimeout)).ztimeout_vector, &literal_null, SIZEOF(mval));
		if (local_str_end > colon_ptr)
		{	/* there's a vector to process */
			read_len = local_str_end - colon_ptr - 1;
			if ((read_len + 1 > vector.len) || (MAX_SRCLINE < vector.len))
			{	/* don't have room for the new vector, so make room */
				assert((vector.addr == (TREF(dollar_ztimeout)).ztimeout_vector.str.addr)
					|| (NULL == (TREF(dollar_ztimeout)).ztimeout_vector.str.addr));
				if (vector.len)
					free(vector.addr);
				vector.addr = (char *)malloc(read_len + 1);
				vector.len = read_len;
			}
			memcpy(vector.addr, colon_ptr + 1, read_len);
			DEBUG_ONLY(vector.addr[read_len] = 0);			/* actually only needed for dbg printfs */
			ztimeout_vector.str.addr = read_len ? vector.addr : NULL;
			ztimeout_vector.str.len = read_len;
			ztimeout_vector.mvtype = MV_STR;
		}
		if (ztimeout_vector.str.len)
		{	/* make sure the vector is valid code */
			OP_COMMARG_S2POOL(&ztimeout_vector);
			op_unwind();
		} else
		{
			ztimeout_vector.str.addr = NULL;
			ztimeout_vector.str.len = 0;
		}
	}
	(TREF(dollar_ztimeout)).ztimeout_vector = ztimeout_vector;
	if (colon_ptr > local_str_val)
	{	/* some form of timeout specified */
		if (0 > inp_val->m[1]) /* Negative timeout specified, cancel the timer */
		{
#			ifdef DEBUG
			if (WBTEST_ENABLED(WBTEST_ZTIM_EDGE))
			{
				LONG_SLEEP(4);					/* allow prior ztimeout timer to pop */
				DBGFPF((stdout, "# white box sleep over\n"));
			}
#			endif
			assert(INTRPT_IN_EVENT_HANDLING != intrpt_ok_state);
			DEFER_INTERRUPTS(INTRPT_IN_EVENT_HANDLING, prev_intrpt_state);
			ztimeout_clear_timer();
			ENABLE_EVENT_INTERRUPTS(prev_intrpt_state);
			TREF(ztimeout_timer_on) = FALSE;
			DBGDFRDEVNT((stderr, "%d %s: check_and_set_ztimeout - canceling ID : %lX\n",
				__LINE__, __FILE__ , ZTIMEOUT_TIMER_ID));
			/* All negative values transformed to -1 */
			memcpy(&((TREF(dollar_ztimeout)).ztimeout_seconds), &literal_minusone, SIZEOF(mval));
		} else
		{
			ztimeout_seconds.str.addr = local_str_val;
			ztimeout_seconds.str.len = only_timeout ? read_len : colon_ptr - local_str_val;
			ztimeout_seconds.mvtype = MV_STR;
			(TREF(dollar_ztimeout)).ztimeout_seconds = ztimeout_seconds;
			zt_sec_ptr = &ztimeout_seconds;				/* compile of below macro requires explicit ptr */
			MV_FORCE_NSTIMEOUT(zt_sec_ptr, nsec_timeout, ZTIMEOUTSTR);
			assert(INTRPT_IN_EVENT_HANDLING != intrpt_ok_state);
			DEFER_INTERRUPTS(INTRPT_IN_EVENT_HANDLING, prev_intrpt_state);
			ztimeout_clear_timer();
			ENABLE_EVENT_INTERRUPTS(prev_intrpt_state);
			if (0 < nsec_timeout)
			{	/* otherwise, below start_timer expires in 0 time, meaning immediately */
				sys_get_curr_time(&cur_time);
				add_uint8_to_abs_time(&cur_time, nsec_timeout, &(TREF(dollar_ztimeout)).end_time);
			}
			DBGDFRDEVNT((stderr, "%d %s: check_and_set_ztimeout - started timeout: %d nsec\n",
				__LINE__, __FILE__, nsec_timeout));
			TREF(ztimeout_timer_on) = TRUE;
			start_timer(ZTIMEOUT_TIMER_ID, nsec_timeout, &ztimeout_expired, 0, NULL);
			(TREF(dollar_ztimeout)).ztimeout_seconds.m[1] = 0;	/* flags get_ztimeout to calulate time remaining */
		}
	}
	DBGDFRDEVNT((stderr, "%d %s: check_and_set_ztimeout - scanned vector: %s\n",__LINE__, __FILE__,
		     (TREF(dollar_ztimeout)).ztimeout_vector.str.len ? (TREF(dollar_ztimeout)).ztimeout_vector.str.addr : "NULL"));
}

void ztimeout_expired(void)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	DBGDFRDEVNT((stderr, "%d %s: ztimeout expired - setting xfer handlers\n", __LINE__, __FILE__));
#	ifdef DEBUG
	if (ydb_white_box_test_case_enabled && ((WBTEST_ZTIMEOUT_TRACE == ydb_white_box_test_case_number)
			|| (WBTEST_ZTIME_DEFER_CRIT == ydb_white_box_test_case_number)
			|| (WBTEST_ZTIM_EDGE == ydb_white_box_test_case_number)))
		DBGFPF((stderr, "# ztimeout expired, white box case %d setting xfer handlers\n", ydb_white_box_test_case_number));
#	endif
	xfer_set_handlers(ztimeout, 0, FALSE);
}

void ztimeout_set(int4 dummy_param)
{	/* attempts to redirect the transfer table to the timeout event at the next opportunity, when we have a current mpc */
	intrpt_state_t	prev_intrpt_state;
#	ifdef DEBUG_DEFERRED_EVENT
	int		index;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(INTRPT_IN_EVENT_HANDLING == intrpt_ok_state);
	assert(ztimeout == outofband);
	if (dollar_zininterrupt || ((0 < dollar_ecode.index) && ETRAP_IN_EFFECT)
		|| (jobinterrupt == (TREF(save_xfer_root_ptr))->ev_que.fl->outofband))
	{	/* not a good time, so save it */
		outofband = no_event;
		TAREF1(save_xfer_root, ztimeout).event_state = queued;
		SAVE_XFER_QUEUE_ENTRY(ztimeout, 0);
		DBGDFRDEVNT((stderr, "%d %s: ztimeout_set - ZTIMEOUT queued; dec_indx %d, et: %d intrpt: %d\n",
			__LINE__, __FILE__, dollar_ecode.index, ETRAP_IN_EFFECT, dollar_zininterrupt));
#		ifdef DEBUG
#		ifdef DEBUG_DEFERRED_EVENT
		if (ETRAP_IN_EFFECT)
		{
			index = namelook(svn_index, svn_names, "ZSTATUS", SIZEOF("ZSTATUS"));
			op_zwritesvn(svn_data[index].opcode);
		}
#		endif
		if (ydb_white_box_test_case_enabled && ((WBTEST_ZTIMEOUT_TRACE == ydb_white_box_test_case_number)
				|| (WBTEST_ZTIME_DEFER_CRIT == ydb_white_box_test_case_number)))
			DBGFPF((stderr, "# ztimeout_set : white box case %d ZTIMEOUT Deferred\n", ydb_white_box_test_case_number));
#		endif
		return;
	}
	DBGDFRDEVNT((stderr, "%d %s: ztimeout_set - NOT deferred\n", __LINE__, __FILE__));
	outofband = ztimeout;
	TAREF1(save_xfer_root, ztimeout).event_state = pending;
	DEFER_INTO_XFER_TAB;
	DBGDFRDEVNT((stderr, "%d %s: ztimeout_set - pending xfer entries for ztimeout\n", __LINE__, __FILE__));
#	ifdef DEBUG
	if (ydb_white_box_test_case_enabled && (WBTEST_ZTIM_EDGE == ydb_white_box_test_case_number))
		DBGFPF((stderr, "# ztimeout_set: white box case %d set the xfer entries for ztimeout\n",
			ydb_white_box_test_case_number));
#	endif
}

void ztimeout_action(void)
{	/* Driven at recognition point of ztimeout by async_action) */
	intrpt_state_t	prev_intrpt_state;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(INTRPT_IN_EVENT_HANDLING != intrpt_ok_state);
	DEFER_INTERRUPTS(INTRPT_IN_EVENT_HANDLING, prev_intrpt_state);
	assert(ztimeout == outofband);
	assert(pending == TAREF1(save_xfer_root, ztimeout).event_state);
	DBGDFRDEVNT((stderr, "%d %s: ztimeout_action - driving the ztimeout vector\n", __LINE__, __FILE__));
	DBGEHND((stderr, "ztimeout_action: Resetting frame 0x"lvaddr" mpc/context with restart_pc/ctxt 0x"lvaddr "/0x"lvaddr
		" - frame has type 0x%04lx\n", frame_pointer, frame_pointer->restart_pc,
		frame_pointer->restart_ctxt, frame_pointer->type));
	ztimeout_clear_timer();
	DBGDFRDEVNT((stderr, "%d %s: ztimeout_action - changing pending to event_state: %d\n", __LINE__, __FILE__,
		TAREF1(save_xfer_root, ztimeout).event_state));
	frame_pointer->mpc = frame_pointer->restart_pc;
	frame_pointer->ctxt = frame_pointer->restart_ctxt;
	ENABLE_INTERRUPTS(INTRPT_IN_EVENT_HANDLING, prev_intrpt_state);
	rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZTIMEOUT);
}

void ztimeout_clear_timer(void)
{	/* called by ztimeout_action just before transfer to actual ztimeout action */
	save_xfer_entry	*entry;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	SHOWTIME(asccurtime);
	DBGDFRDEVNT((stderr, "%d %s: ztimeout_clear_timer - clearing ztimeout @ %s while %sin use\n", __LINE__, __FILE__,
		asccurtime, TREF(ztimeout_timer_on) ? "": "*NOT* "));
	assert(INTRPT_IN_EVENT_HANDLING == intrpt_ok_state);
	entry = &TAREF1(save_xfer_root, ztimeout);
	if (queued == entry->event_state)
	{
		REMOVE_XFER_QUEUE_ENTRY(ztimeout);
		entry->event_state = not_in_play;
	}
	if (pending == entry->event_state)
	{
#		ifdef DEBUG
		if (ydb_white_box_test_case_enabled && (WBTEST_ZTIM_EDGE == ydb_white_box_test_case_number))
			DBGFPF((stderr, "# ztimeout_clear_timer - white box case %d resetting the xfer entries for ztimeout\n",
				ydb_white_box_test_case_number));
		assert(ztimeout == outofband);
#		endif
	}
	entry->event_state = active;		/* required by the routine invoked on the next line */
	(void)xfer_reset_if_setter(ztimeout);
	if (TREF(ztimeout_timer_on))
	{
		cancel_timer(ZTIMEOUT_TIMER_ID);
		TREF(ztimeout_timer_on) = FALSE;
		DBGDFRDEVNT((stderr, "%d %s: ztimeout_clear_timer - state: %d\n", __LINE__, __FILE__,
		     entry->event_state));
	}
	entry->event_state = not_in_play;
}
