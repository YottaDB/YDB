/****************************************************************
 *								*
 * Copyright (c) 2010-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef GTM_TRIGGER
#include "gtm_stat.h"
#include "gtm_string.h"

#include "gtm_multi_thread.h"
#include "gdsroot.h"			/* for gdsfhead.h */
#include "gdsbt.h"			/* for gdsfhead.h */
#include "gdsfhead.h"			/* For gvcst_protos.h */
#include <rtnhdr.h>
#include "gv_trigger.h"
#include "io.h"
#include "hashtab_str.h"
#include "trigger_trgfile_protos.h"
#include "trigger_delete_protos.h"
#include "trigger.h"			/* needed by "trigger_update_protos.h" for trig_stats_t prototype */
#include "trigger_update_protos.h"
#include "file_input.h"
#include "op.h"				/* for op_tstart */
#include "op_tcommit.h"
#include "gdscc.h"			/* needed for tp.h */
#include "gdskill.h"			/* needed for tp.h */
#include "buddy_list.h"			/* needed for tp.h */
#include "hashtab_int4.h"		/* needed for tp.h */
#include "filestruct.h"			/* needed for jnl.h */
#include "jnl.h"			/* needed for tp.h */
#include "tp.h"
#include "error.h"
#include "tp_restart.h"
#include "mupip_exit.h"
#include "util.h"
#include "stack_frame.h"
#include "mv_stent.h"
#include "tp_frame.h"
#include "t_retry.h"
#include "gtmimagename.h"

#define TRIG_ERROR_RETURN								\
{											\
	DCL_THREADGBL_ACCESS;								\
											\
	SETUP_THREADGBL_ACCESS;								\
	if (lcl_implicit_tpwrap)							\
	{	/* only if we were implicitly wrapped */				\
		assert(dollar_tlevel);							\
		assert(donot_INVOKE_MUMTSTART);						\
		DEBUG_ONLY(donot_INVOKE_MUMTSTART = FALSE);				\
		/* Print $ztrigger/mupip-trigger output before rolling back TP */	\
		TP_ZTRIGBUFF_PRINT;							\
		OP_TROLLBACK(-1);	/* Unroll implicit TP */			\
		REVERT;									\
	}										\
	return TRIG_FAILURE;								\
}

GBLREF	sgm_info		*first_sgm_info;
GBLREF	bool			mupip_error_occurred;
GBLREF	gv_key			*gv_currkey;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	int			tprestart_state;
GBLREF	io_pair			io_curr_device;
GBLREF	gv_namehead		*reset_gv_target;
GBLREF	uint4			dollar_tlevel;
GBLREF	unsigned int		t_tries;
GBLREF	unsigned char		t_fail_hist[CDB_MAX_TRIES];
#ifdef DEBUG
GBLREF	boolean_t		donot_INVOKE_MUMTSTART;
#endif

error_def(ERR_DBROLLEDBACK);
error_def(ERR_ZFILNMBAD);

STATICFNDEF boolean_t trigger_trgfile_tpwrap_helper(char *trigger_filename, uint4 trigger_filename_len, boolean_t noprompt,
						    boolean_t lcl_implicit_tpwrap)
{
	boolean_t		trigger_error;
	uint4			i;
	io_pair			io_save_device;
	io_pair			io_trigfile_device;
	int			len;
	int4			record_num;
	boolean_t		trigger_status;
	enum cdb_sc		cdb_status;
	uint4			trig_stats[NUM_STATS];
	char			*trigger_rec;
	char			*values[NUM_SUBS];
	unsigned short		value_len[NUM_SUBS];

	if (lcl_implicit_tpwrap)
		ESTABLISH_RET(trigger_tpwrap_ch, TRIG_FAILURE);	/* Return through here is a failure */
	io_save_device = io_curr_device;
	file_input_init(trigger_filename, trigger_filename_len, IOP_REWIND);
	if (mupip_error_occurred)
	{
		assert(!memcmp(&io_curr_device, &io_save_device, SIZEOF(io_curr_device)));
		io_curr_device = io_save_device;	/* just in case in PRO */
		TRIG_ERROR_RETURN;
	}
	io_trigfile_device = io_curr_device;
	record_num = 0;
	for (i = 0; NUM_STATS > i; i++)
		trig_stats[i] = 0;
	trigger_error = FALSE;
	while ((0 == io_curr_device.in->dollar.zeof) && (0 <= (len = file_input_get(&trigger_rec, 0))))
	{
		io_curr_device = io_save_device;
		record_num++;
		if ((0 != len) && (COMMENT_LITERAL != trigger_rec[0]))
			util_out_print_gtmio("File !AD, Line !UL: ", NOFLUSH, trigger_filename_len, trigger_filename, record_num);
		trigger_status = trigger_update_rec(trigger_rec, (uint4)len, noprompt, trig_stats, &io_trigfile_device,
						    &record_num);
		trigger_error |= (TRIG_FAILURE == trigger_status);
		assert(!trigger_error || trig_stats[STATS_ERROR_TRIGFILE]);
		assert(trigger_error || !trig_stats[STATS_ERROR_TRIGFILE]);
		io_curr_device = io_trigfile_device;
	}
	if ((-1 == len) && (!io_curr_device.in->dollar.zeof))
	{
		io_curr_device = io_save_device;
		util_out_print_gtmio("File !AD, Line !UL: Line too long", FLUSH, trigger_filename_len, trigger_filename,
			++record_num);
		io_curr_device = io_trigfile_device;
	}
	file_input_close();
	io_curr_device = io_save_device;
	if (trigger_error)
	{
		util_out_print_gtmio("=========================================", FLUSH);
		util_out_print_gtmio("!UL trigger file entries have errors", FLUSH, trig_stats[STATS_ERROR_TRIGFILE]);
		util_out_print_gtmio("!UL trigger file entries have no errors", FLUSH,
					trig_stats[STATS_NOERROR_TRIGFILE] + trig_stats[STATS_UNCHANGED_TRIGFILE]);
		util_out_print_gtmio("=========================================", FLUSH);
		TRIG_ERROR_RETURN;	/* rollback the trigger transaction due to errors and return */
	}
	if (trig_stats[STATS_ADDED] + trig_stats[STATS_DELETED] + trig_stats[STATS_UNCHANGED_TRIGFILE] + trig_stats[STATS_MODIFIED])
	{
		util_out_print_gtmio("=========================================", FLUSH);
		util_out_print_gtmio("!UL triggers added", FLUSH, trig_stats[STATS_ADDED]);
		util_out_print_gtmio("!UL triggers deleted", FLUSH, trig_stats[STATS_DELETED]);
		util_out_print_gtmio("!UL triggers modified", FLUSH, trig_stats[STATS_MODIFIED]);
		util_out_print_gtmio("!UL trigger file entries did update database trigger content",
					FLUSH, trig_stats[STATS_NOERROR_TRIGFILE]);
		util_out_print_gtmio("!UL trigger file entries did not update database trigger content",
					FLUSH, trig_stats[STATS_UNCHANGED_TRIGFILE]);
		util_out_print_gtmio("=========================================", FLUSH);
	}
	if (lcl_implicit_tpwrap)
	{
		GVTR_OP_TCOMMIT(cdb_status);	/* commit the trigger transaction */
		if (cdb_sc_normal != cdb_status)
			t_retry(cdb_status);	/* won't return */
		REVERT;
	}
	return TRIG_SUCCESS;
}

boolean_t trigger_trgfile_tpwrap(char *trigger_filename, uint4 trigger_filename_len, boolean_t noprompt)
{
	boolean_t		trigger_status = TRIG_FAILURE;
	mval			ts_mv;
	int			loopcnt, utilbuff_len;
	struct stat		statbuf;
	DEBUG_ONLY(unsigned int	lcl_t_tries;)
	enum cdb_sc		failure;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ts_mv.mvtype = MV_STR;
	ts_mv.str.len = 0;
	ts_mv.str.addr = NULL;
	/* Do sanity checks on the filename and the file's accessibility. */
	assert('\0' == trigger_filename[trigger_filename_len]); /* should have been made sure by caller */
	if (-1 == Stat(trigger_filename, &statbuf))
	{
		DEBUG_ONLY(TREF(gtmio_skip_tlevel_assert) = TRUE;)
		util_out_print_gtmio("Invalid file name: !AD: !AZ",
				FLUSH, trigger_filename_len, trigger_filename, STRERROR(errno));
		DEBUG_ONLY(TREF(gtmio_skip_tlevel_assert) = FALSE;)
		TP_ZTRIGBUFF_PRINT;	/* Print $ztrigger/mupip-trigger output before returning */
		return TRUE;	/* Failure */
	} else if (!S_ISREG(statbuf.st_mode))
	{
		DEBUG_ONLY(TREF(gtmio_skip_tlevel_assert) = TRUE;)
		util_out_print_gtmio("Invalid file name: !AD: Not a proper input file",
						FLUSH, trigger_filename_len, trigger_filename);
		DEBUG_ONLY(TREF(gtmio_skip_tlevel_assert) = FALSE;)
		TP_ZTRIGBUFF_PRINT;	/* Print $ztrigger/mupip-trigger output before returning */
		return TRUE;	/* Failure */
	}
	if (0 == dollar_tlevel)
	{	/* If not already wrapped in TP, wrap it now implicitly */
		assert(!donot_INVOKE_MUMTSTART);
		DEBUG_ONLY(donot_INVOKE_MUMTSTART = TRUE);
		/* Note down dollar_tlevel before op_tstart. This is needed to determine if we need to break from the for-loop
		 * below after a successful op_tcommit of the $ZTRIGGER operation. We cannot check that dollar_tlevel is zero
		 * since the op_tstart done below can be a nested sub-transaction
		 */
		op_tstart(IMPLICIT_TSTART, TRUE, &ts_mv, 0); /* 0 ==> save no locals but RESTART OK */
		/* Note down length of unprocessed util_out buffer */
		ASSERT_SAFE_TO_UPDATE_THREAD_GBLS;
		assert(NULL != TREF(util_outptr));
		utilbuff_len = INTCAST(TREF(util_outptr) - TREF(util_outbuff_ptr));
		assert(OUT_BUFF_SIZE >= utilbuff_len);
		/* The following for loop structure is similar to that in module trigger_update.c (function "trigger_update")
		 * and module gv_trigger.c (function gvtr_db_tpwrap) so any changes here might need to be reflected there as well.
		 */
		for (loopcnt = 0; ; loopcnt++)
		{
			assert(donot_INVOKE_MUMTSTART);	/* Make sure still set */
			DEBUG_ONLY(lcl_t_tries = t_tries);
			TREF(ztrigbuffLen) = utilbuff_len;	/* reset ztrig buffer at start of each try/retry */
			TREF(util_outptr) = TREF(util_outbuff_ptr); /* Signal any unflushed text from previous try as gone */
			trigger_status = trigger_trgfile_tpwrap_helper(trigger_filename, trigger_filename_len, noprompt, TRUE);
			if (0 == dollar_tlevel)
				break;
			assert(0 < t_tries);
			assert((CDB_STAGNATE == t_tries) || (lcl_t_tries == t_tries - 1));
			failure = LAST_RESTART_CODE;
			assert(((cdb_sc_onln_rlbk1 != failure) && (cdb_sc_onln_rlbk2 != failure))
				|| !gv_target || !gv_target->root);
			assert((cdb_sc_onln_rlbk2 != failure) || !IS_GTM_IMAGE || TREF(dollar_zonlnrlbk));
			if (cdb_sc_onln_rlbk2 == failure)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_DBROLLEDBACK);
			/* else if (cdb_sc_onln_rlbk1 == status) we don't need to do anything other than trying again. Since this
			 * is ^#t global, we don't need to GVCST_ROOT_SEARCH before continuing with the next restart because the
			 * trigger load logic already takes care of doing INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED before doing the
			 * actual trigger load
			 */
			/* We expect the above function to return with either op_tcommit or a tp_restart invoked.
			 * In the case of op_tcommit, we expect dollar_tlevel to be 0 and if so we break out of the loop.
			 * In the tp_restart case, we expect a maximum of 4 tries/retries and much lesser usually.
			 * Additionally we also want to avoid an infinite loop so limit the loop to what is considered
			 * a huge iteration count and assertpro if that is reached as it suggests an out-of-design situation.
			 */
			assertpro(TPWRAP_HELPER_MAX_ATTEMPTS >= loopcnt);
		}
	} else
	{
		trigger_status = trigger_trgfile_tpwrap_helper(trigger_filename, trigger_filename_len, noprompt, FALSE);
		assert(0 < dollar_tlevel);
	}
	return (TRIG_FAILURE == trigger_status);
}
#endif /* GTM_TRIGGER */
