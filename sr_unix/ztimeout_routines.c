/****************************************************************
 *								*
 * Copyright (c) 2018-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2019-2021 YottaDB LLC and/or its subsidiaries.	*
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
#include "gtm_fcntl.h"  /* Needed for AIX's silly open to open64 translations */
#include "gdsfhead.h"
#include "gdscc.h"
#include "filestruct.h"
#include "buddy_list.h"         /* needed for tp.h */
#include "jnl.h"
#include "tp.h"
#include "send_msg.h"
#include "gtmmsg.h"             /* for gtm_putmsg() prototype */
#include "change_reg.h"
#include "setterm.h"
#include "getzposition.h"
#include "min_max.h"
#include "mvalconv.h"
#ifdef DEBUG
#include "have_crit.h"          /* for the TPNOTACID_CHECK macro */
#endif
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "time.h"
#include "gt_timer.h"
#include "ztimeout_routines.h"
#include "deferred_events.h"
#include "error_trap.h"
#include "indir_enum.h"
#include "zwrite.h"
#include "fix_xfer_entry.h"
#include "gvname_info.h"
#include "op_merge.h"
#include "zshow.h"
#include "outofband.h"
#include "gtm_signal.h"
#include "deferred_events_queue.h"
#include "mv_stent.h"
#include "wbox_test_init.h"
#include "gtmio.h"
#include "compiler.h"
#include "is_equ.h"
#include "gtm_common_defs.h"

GBLREF	stack_frame		*frame_pointer, *error_frame;
GBLREF	spdesc			stringpool;
GBLREF	unsigned short		proc_act_type;
GBLREF	mv_stent		*mv_chain;
GBLREF	int			dollar_truth;
GBLREF	mstr			extnam_str;
GBLREF	dollar_ecode_type	dollar_ecode;
GBLREF	dollar_stack_type	dollar_stack;
GBLREF	boolean_t		ztrap_explicit_null;
GBLREF	boolean_t		dollar_zininterrupt;
GBLREF	int4			outofband;
GBLREF	sigset_t		blockalrm;
GBLREF	spdesc			rts_stringpool;
GBLREF	void			(*ztimeout_clear_ptr)(void);
GBLREF	boolean_t		ydb_white_box_test_case_enabled;
GBLREF	int			ydb_white_box_test_case_number;

LITREF	mval			literal_null, literal_minusone;

error_def(ERR_ZTIMEOUT);

#define ZTIMEOUT_TIMER_ID (TID)&check_and_set_ztimeout
#define ZTIMEOUT_QUEUE_ID &ztimeout_set
#define MAX_FORMAT_LEN	250

#define NULLIFY_VECTOR												\
{														\
	if ((TREF(dollar_ztimeout)).ztimeout_vector.str.len && (TREF(dollar_ztimeout)).ztimeout_vector.str.addr)\
	{													\
		free((TREF(dollar_ztimeout)).ztimeout_vector.str.addr);						\
		memcpy(&(TREF(dollar_ztimeout)).ztimeout_vector, &literal_null, SIZEOF(mval));			\
		ztimeout_vector.str.addr = NULL;								\
		ztimeout_vector.str.len = 0;									\
	}													\
}

void check_and_set_ztimeout(mval * inp_val)
{
	int 		max_read_len;
	char		*vector_ptr = NULL;
	char		*tok_ptr, *tok_ptr2;
	char		*local_str_end, *strtokptr;
	char		*colon_ptr = NULL;
	sigset_t	savemask;
	int4		rc;
	ABS_TIME	cur_time;
	uint8		nsec_timeout;   /* timeout in nanoseconds */
	mval		*interim_ptr;
	mval		ztimeout_vector, ztimeout_seconds;
	boolean_t	is_negative = FALSE;
	static char	*ztimeout_local_str_val;
	static mstr	ztimeout_local_vector;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
<<<<<<< HEAD
	SIGPROCMASK(SIG_BLOCK, &blockalrm, &savemask, rc);
	if (NULL != ztimeout_local_str_val)
	{	/* This is left over from a previous call to "check_and_set_ztimeout()" that encountered an error
		 * midway (e.g. inside "op_commarg()" call below) and so could not get a chance to free memory then
		 * before returning from the function. Free that up now to avoid an accumulating memory leak.
		 * This is why we need "ztimeout_local_str_val" to be a "static" variable.
		 */
		free(ztimeout_local_str_val);
		ztimeout_local_str_val = NULL;
	}
	MV_FORCE_STR(inp_val);	/* needed before checking "inp_val->str" or else it could be uninitialized/garbage */
	ztimeout_local_str_val = (char *)malloc(inp_val->str.len + 1);
	memcpy(ztimeout_local_str_val, inp_val->str.addr, inp_val->str.len);
	local_str_end = ztimeout_local_str_val + inp_val->str.len;
	ztimeout_local_str_val[inp_val->str.len] = '\0';
	colon_ptr = strchr(ztimeout_local_str_val, ':');
=======
	local_str_val = (char *)malloc(inp_val->str.len + 1);
	memcpy(local_str_val, inp_val->str.addr, inp_val->str.len);
	local_str_end = local_str_val + inp_val->str.len;
	local_str_val[inp_val->str.len] = '\0';
	colon_ptr = strchr(local_str_val, ':');
>>>>>>> f33a273c... GT.M V6.3-012
	ztimeout_vector = (TREF(dollar_ztimeout)).ztimeout_vector;
	ztimeout_seconds = (TREF(dollar_ztimeout)).ztimeout_seconds;
	max_read_len = ((NULL == colon_ptr)
			? inp_val->str.len
			: ((colon_ptr == ztimeout_local_str_val) ? (inp_val->str.len - 1) : (local_str_end - colon_ptr - 1)));
	if (MAX_SRCLINE < max_read_len)
		max_read_len = MAX_SRCLINE;
	if (colon_ptr == ztimeout_local_str_val) /* Starting with colon, change the vector only */
	{
		tok_ptr = STRTOK_R(ztimeout_local_str_val, ":", &strtokptr);
		NULLIFY_VECTOR;
		if (NULL != tok_ptr) /* i.e. : is followed by a non-empty string that does not start with $c(0) */
		{
			vector_ptr = (char *)malloc(max_read_len + 1);
			memcpy(vector_ptr, tok_ptr, max_read_len);
			vector_ptr[max_read_len] = '\0';
			ztimeout_vector.str.addr = vector_ptr;
			ztimeout_vector.str.len = max_read_len + 1;
			ztimeout_vector.mvtype = MV_STR;
		}
		nsec_timeout = 0;
	} else
	{	/* Some form of timeout specified */
		MV_FORCE_NUM(inp_val);	/* needed before checking "inp_val->m[1]" or else it could be uninitialized */
		if (inp_val->m[1] < 0) /* Negative timeout specified, cancel the timer */
		{
			TREF(in_ztimeout) = FALSE;
			is_negative = TRUE;
			DBGDFRDEVNT((stderr,"Cancelling the timer with ID : %ld\n", ZTIMEOUT_TIMER_ID));
			/* All negative values transformed to -1 */
			memcpy(&ztimeout_seconds, &literal_minusone, SIZEOF(mval));
			cancel_timer(ZTIMEOUT_TIMER_ID);
		}
		tok_ptr = STRTOK_R(ztimeout_local_str_val, ":", &strtokptr);
		tok_ptr2 = STRTOK_R(NULL, ":", &strtokptr); /* Next token is the vector (if one exists) */
		if (NULL == tok_ptr2)	/* Only timeout specified (Note: timeout:xxx is also considered
					 * as only timeout specified if xxx starts with $c(0)).
					 */
		{
			if (!is_negative) /* Process for positive timeout */
			{
				/* Construct a format for reading in the input */
				ztimeout_seconds.str.addr = ztimeout_local_str_val;
				ztimeout_seconds.str.len = STRLEN(ztimeout_local_str_val);
				ztimeout_seconds.mvtype = MV_STR;
				interim_ptr = &ztimeout_seconds;
				MV_FORCE_NSTIMEOUT(interim_ptr, nsec_timeout, ZTIMEOUTSTR);
				if (colon_ptr == (local_str_end - 1)) /* Form : timeout: */
					NULLIFY_VECTOR;
				/* Done with the contents of ztimeout_seconds */
				ztimeout_seconds.str.addr = NULL;
				ztimeout_seconds.str.len = 0;
			} else
				nsec_timeout = 0;
			/* If only timeout specified or timeout: or timeout:$c(0), retain the old vector */
		} else
		{
			/* Both timeout and vector specified. Change both. */
			/* Change timeout */
			ztimeout_seconds.str.addr = tok_ptr;
			STRNLEN(tok_ptr, MAX_SRCLINE, ztimeout_seconds.str.len);
			ztimeout_seconds.mvtype = MV_STR;
			interim_ptr = &ztimeout_seconds;
			MV_FORCE_NSTIMEOUT(interim_ptr, nsec_timeout, ZTIMEOUTSTR);
			/* Done with the contents of ztimeout_seconds */
			ztimeout_seconds.str.addr = NULL;
			ztimeout_seconds.str.len = 0;
			/* Change vector */
			NULLIFY_VECTOR;
			vector_ptr = (char *)malloc(max_read_len + 1);
			memcpy(vector_ptr, tok_ptr2, max_read_len);
			vector_ptr[max_read_len] = '\0';
			ztimeout_vector.str.addr = vector_ptr;
			ztimeout_vector.str.len = max_read_len + 1;
			ztimeout_vector.mvtype = MV_STR;
		}
	}
	if (NULL != ztimeout_local_str_val)
	{
		free(ztimeout_local_str_val);
		ztimeout_local_str_val = NULL;
	}
	if (ztimeout_vector.str.len)
	{
		ztimeout_local_str_val = ztimeout_vector.str.addr;	/* Keep a pointer so we free it in the next call to
									 * this function in case of errors in "op_commarg".
									 * This avoids an accumulating memory leak.
									 */
		op_commarg(&ztimeout_vector, indir_linetail);
		op_unwind();
		ztimeout_local_str_val = NULL;	/* If we came here it means the M code specified was valid.
						 * This is going to be assigned to TREF(dollar_ztimeout)
						 * so no need to keep track of this for freeing purposes.
						 */
	}
	SIGPROCMASK(SIG_BLOCK, &blockalrm, &savemask, rc);
	(TREF(dollar_ztimeout)).ztimeout_vector = ztimeout_vector;
	(TREF(dollar_ztimeout)).ztimeout_seconds = ztimeout_seconds;
	if (!is_negative)
	{
		if (!nsec_timeout)
		{ /* Immediately transfer control to vector */
			TREF(in_ztimeout) = TRUE;
			cancel_timer(ZTIMEOUT_TIMER_ID);
			start_timer(ZTIMEOUT_TIMER_ID, 0, &ztimeout_expire_now, 0, NULL);
		} else if (0 < nsec_timeout)
		{
			if (!TREF(in_ztimeout))
				TREF(in_ztimeout) = TRUE;
			else	/* Cancel the previous timer and start a new one */
				cancel_timer(ZTIMEOUT_TIMER_ID);
			sys_get_curr_time(&cur_time);
			add_uint8_to_abs_time(&cur_time, nsec_timeout, &(TREF(dollar_ztimeout)).end_time);
			start_timer(ZTIMEOUT_TIMER_ID, nsec_timeout,
						&ztimeout_expire_now, 0, NULL);
			DBGDFRDEVNT((stderr,"Started ztimeout timer with timeout: %d\n",
								nsec_timeout));
		}
	}
	if (vector_ptr)
	{
		DBGDFRDEVNT((stderr,"scanned values %s %s\n", (TREF(dollar_ztimeout)).ztimeout_seconds.str.addr, vector_ptr));
	}
	else
	{
		DBGDFRDEVNT((stderr,"scanned values %s\n", (TREF(dollar_ztimeout)).ztimeout_seconds.str.addr));
	}
	/* Let the timers pop again.. */
	SIGPROCMASK(SIG_SETMASK, &savemask, NULL, rc);
}

void ztimeout_expire_now(void)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	DBGDFRDEVNT((stderr,"Ztimeout expired, setting xfer handlers\n"));
#ifdef DEBUG
	if (ydb_white_box_test_case_enabled &&
				((WBTEST_ZTIMEOUT_TRACE == ydb_white_box_test_case_number)
					|| (WBTEST_ZTIME_DEFER_CRIT == ydb_white_box_test_case_number)))
	DBGFPF((stderr,"Ztimeout expired, setting xfer handlers\n"));
#endif
	TREF(ztimeout_set_xfer) = xfer_set_handlers(outofband_event, &ztimeout_set, 0, FALSE);
}

void ztimeout_set(int4 dummy_param)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (((0 < dollar_ecode.index) && (ETRAP_IN_EFFECT)) || dollar_zininterrupt ||
					have_crit(CRIT_HAVE_ANY_REG | CRIT_IN_COMMIT))
	{
		TREF(ztimeout_deferred) = TRUE;
		SAVE_XFER_ENTRY(outofband_event, &ztimeout_set, 0);
		DBGDFRDEVNT((stderr, "ztimeout_set : ZTIMEOUT Deferred\n"));
#ifdef DEBUG
	if (ydb_white_box_test_case_enabled &&
				((WBTEST_ZTIMEOUT_TRACE == ydb_white_box_test_case_number)
					|| (WBTEST_ZTIME_DEFER_CRIT == ydb_white_box_test_case_number)))
		DBGFPF((stderr, "ztimeout_set : ZTIMEOUT Deferred\n"));
#endif
		return;
	} else
		DBGDFRDEVNT((stderr, "ztimeout_set: ZTIMEOUT NOT deferred - \n"));
	if (ztimeout != outofband)
	{
		SET_OUTOFBAND(ztimeout);
		DBGDFRDEVNT((stderr, "ztimeout_set: Set the xfer entries for ztimeout\n"));
	} else
	{
		DBGDFRDEVNT((stderr, "ztimeout_set: ztimeout outofband already set\n"));
	}
	TREF(ztimeout_deferred) = FALSE;
}

/* Driven at recognition point of ztimeout by outofband_action() */
void ztimeout_action(void)
{
	DBGDFRDEVNT((stderr, "ztimeout_action driving the ztimeout vector\n"));
	DBGEHND((stderr, "ztimeout_action: Resetting frame 0x"lvaddr" mpc/context with restart_pc/ctxt "
                         "0x"lvaddr"/0x"lvaddr" - frame has type 0x%04lx\n", frame_pointer, frame_pointer->restart_pc,
			 frame_pointer->restart_ctxt, frame_pointer->type));
	ztimeout_clear_timer();
	frame_pointer->mpc = frame_pointer->restart_pc;
	frame_pointer->ctxt = frame_pointer->restart_ctxt;
	rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZTIMEOUT);
}

void ztimeout_process()
{
	mv_stent        *mv_st_ent;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Compile and push new (counted) frame onto the stack to drive the ztimeout vector */
	assert((SFT_ZTIMEOUT | SFT_COUNT) == proc_act_type);
	op_commarg(&(TREF(dollar_ztimeout)).ztimeout_vector, indir_linetail);
	/* Below in sync with jobinterrupt_process */
	frame_pointer->type = proc_act_type;    /* The mark of zorro.. */
        proc_act_type = 0;
        /* Now we need to preserve our current environment. This MVST_ZINTR mv_stent type will hold
         * the items deemed necessary to preserve. All other items are the user's responsibility.
         *
         * Initialize the mv_stent elements processed by stp_gcol which can be called for either the
         * op_gvsavtarg() or extnam items. This initialization keeps stp_gcol from attempting to
         * process unset fields with garbage in them as valid mstr address/length pairs.
         */
        PUSH_MV_STENT(MVST_ZTIMEOUT);
        mv_st_ent = mv_chain;
        mv_st_ent->mv_st_cont.mvs_zintr.savtarg.str.len = 0;
        mv_st_ent->mv_st_cont.mvs_zintr.savextref.len = 0;
        mv_st_ent->mv_st_cont.mvs_zintr.saved_dollar_truth = dollar_truth;
        op_gvsavtarg(&mv_st_ent->mv_st_cont.mvs_zintr.savtarg);
        if (extnam_str.len)
        {
                ENSURE_STP_FREE_SPACE(extnam_str.len);
                mv_st_ent->mv_st_cont.mvs_zintr.savextref.addr = (char *)stringpool.free;
                memcpy(mv_st_ent->mv_st_cont.mvs_zintr.savextref.addr, extnam_str.addr, extnam_str.len);
                stringpool.free += extnam_str.len;
                assert(stringpool.free <= stringpool.top);
        }
        mv_st_ent->mv_st_cont.mvs_zintr.savextref.len = extnam_str.len;
        /* save/restore $ECODE/$STACK over this invocation */
        mv_st_ent->mv_st_cont.mvs_zintr.error_frame_save = error_frame;
        memcpy(&mv_st_ent->mv_st_cont.mvs_zintr.dollar_ecode_save, &dollar_ecode, SIZEOF(dollar_ecode));
        memcpy(&mv_st_ent->mv_st_cont.mvs_zintr.dollar_stack_save, &dollar_stack, SIZEOF(dollar_stack));
        NULLIFY_ERROR_FRAME;
        ecode_init();
        /* If we interrupted a Merge, ZWrite, or ZShow, save the state info in an mv_stent that will be restored when this
         * frame returns. Note that at this time, return from a ztimeout does not "return" to the interrupt
         * point but rather restarts the line of M code we were running *OR* at the most recent save point (set by
         * op_restartpc or equivalent).
         */
        PUSH_MVST_MRGZWRSV_IF_NEEDED;
        return;
}


/* Clear the timer */
void ztimeout_clear_timer(void)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	boolean_t ztimeout_check = FALSE;

	TREF(ztimeout_deferred) = FALSE;
	if (TREF(in_ztimeout))
	{
		DBGDFRDEVNT((stderr, "ztimeout_clear_timer: Clearing the ztimeout timer\n"));
		cancel_timer(ZTIMEOUT_TIMER_ID);
		REMOVE_QUEUE_ENTRY(ZTIMEOUT_QUEUE_ID);
		TREF(in_ztimeout) = FALSE;
		/* -----------------------------------------------------
		 * Should clear xfer settings only if set them.
		 * -----------------------------------------------------
	 	*/
		if (TREF(ztimeout_set_xfer))
		{
			ztimeout_check = xfer_reset_if_setter(outofband_event);
			DBGDFRDEVNT((stderr, "ztimeout_clear_timer: Resetting the xfer entries for ztimeout\n"));
			assert(ztimeout_check);
			TREF(ztimeout_set_xfer) = FALSE;
		}
	}
}
