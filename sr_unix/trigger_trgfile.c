/****************************************************************
 *								*
 *	Copyright 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stat.h"
#include "gtm_string.h"
#include "gdsroot.h"			/* for gdsfhead.h */
#include "gdsbt.h"			/* for gdsfhead.h */
#include "gdsfhead.h"			/* For gvcst_protos.h */
#include "rtnhdr.h"
#include "gv_trigger.h"
#include "trigger_trgfile_protos.h"
#include "trigger_delete_protos.h"
#include "trigger_update_protos.h"
#include "file_input.h"
#include "trigger.h"
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
#include "io.h"

GBLREF	sgm_info		*first_sgm_info;
GBLREF	bool			mupip_error_occurred;
GBLREF	boolean_t		implicit_tstart;	/* see gbldefs.c for comment */
GBLREF	gv_key			*gv_currkey;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	gd_addr			*gd_header;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	short			dollar_trestart;
GBLREF	int			tprestart_state;
GBLREF	io_pair			io_curr_device;
GBLREF	gv_namehead		*reset_gv_target;

LITREF	mval			literal_hasht;

error_def(ERR_TPRETRY);

/* This code is modeled around "updproc_ch" in updproc.c */
CONDITION_HANDLER(trigger_trgfile_tpwrap_ch)
{
	int	rc;

	START_CH;
	if ((int)ERR_TPRETRY == SIGNAL)
	{
		assert(TPRESTART_STATE_NORMAL == tprestart_state);
		tprestart_state = TPRESTART_STATE_NORMAL;
		assert(NULL != first_sgm_info);
		/* This only happens at the outer-most layer so state should be normal now */
		rc = tp_restart(1, !TP_RESTART_HANDLES_ERRORS);
		assert(0 == rc);
		assert(TPRESTART_STATE_NORMAL == tprestart_state);
		reset_gv_target = INVALID_GV_TARGET;	/* see similar code in "trigger_item_tpwrap_ch" for why this is needed */
		UNWIND(NULL, NULL);
	}
	NEXTCH;
}

STATICFNDEF boolean_t trigger_trgfile_tpwrap_helper(char *trigger_filename, uint4 trigger_filename_len, boolean_t noprompt)
{
	boolean_t		all_triggers_error;
	char			filename[MAX_FN_LEN + 1];
	uint4			i;
	io_pair			io_save_device;
	io_pair			io_trigfile_device;
	int			len;
	uint4			record_num;
	boolean_t		trigger_error;
	struct stat		statbuf;
	enum cdb_sc		status;
	uint4			trig_stats[NUM_STATS];
	char			*trigger_rec;
	char			*values[NUM_SUBS];
	unsigned short		value_len[NUM_SUBS];

	error_def(ERR_ZFILNMBAD);

	trigger_error = all_triggers_error = FALSE;
	if ((0 == trigger_filename_len) || MAX_FN_LEN < trigger_filename_len)
	{
		util_out_print_gtmio("Invalid file name: !AD", FLUSH, trigger_filename_len, trigger_filename);
		OP_TROLLBACK(0);
		return TRIG_FAILURE;
	}
	strncpy(filename, trigger_filename, trigger_filename_len);
	filename[trigger_filename_len]='\0';
	if ((-1 == Stat(filename, &statbuf)) || !S_ISREG(statbuf.st_mode))
	{
		util_out_print_gtmio("Invalid file name: !AD", FLUSH, trigger_filename_len, trigger_filename);
		OP_TROLLBACK(0);
		return TRIG_FAILURE;
	}
	ESTABLISH_RET(trigger_trgfile_tpwrap_ch, all_triggers_error);
	io_save_device = io_curr_device;
	file_input_init(trigger_filename, trigger_filename_len);
	if (mupip_error_occurred)
	{
		OP_TROLLBACK(0);
		REVERT;
		return TRIG_FAILURE;
	}
	io_trigfile_device = io_curr_device;
	record_num = 0;
	for (i = 0; NUM_STATS > i; i++)
		trig_stats[i] = 0;
	while (0 <= (len = file_input_get(&trigger_rec)))
	{
		io_curr_device = io_save_device;
		record_num++;
		if ((0 != len) && (COMMENT_LITERAL != trigger_rec[0]))
			util_out_print_gtmio("File !AD, Line !UL: ", NOFLUSH, trigger_filename_len, trigger_filename, record_num);
		trigger_error = trigger_update_rec(trigger_rec, (uint4)len, noprompt, trig_stats);
		all_triggers_error |= (TRIG_FAILURE == trigger_error);
		if (all_triggers_error && (TRIG_SUCCESS == trigger_error) && (0 != len) && (COMMENT_LITERAL != trigger_rec[0]))
		{
			assert(0 != trig_stats[STATS_ERROR]);
			trig_stats[STATS_ADDED]++;
			util_out_print_gtmio("No errors", FLUSH);
		}
		io_curr_device = io_trigfile_device;
	}
	if ((-1 == len) && (!io_curr_device.in->dollar.zeof))
	{
		io_curr_device = io_save_device;
		util_out_print_gtmio("File !AD, Line !UL: Line too long", FLUSH, trigger_filename_len, trigger_filename,
			++record_num);
	}
	file_input_close();
	io_curr_device = io_save_device;
	if (all_triggers_error)
	{
		util_out_print_gtmio("=========================================", FLUSH);
		util_out_print_gtmio("!UL trigger file entries matched existing triggers", FLUSH, trig_stats[STATS_UNCHANGED]);
		util_out_print_gtmio("!UL trigger file entries have errors", FLUSH, trig_stats[STATS_ERROR]);
		util_out_print_gtmio("!UL trigger file entries have no errors", FLUSH,
				     trig_stats[STATS_ADDED] + trig_stats[STATS_DELETED] + trig_stats[STATS_MODIFIED]);
		util_out_print_gtmio("=========================================", FLUSH);
		OP_TROLLBACK(0);
		REVERT;
		return TRIG_FAILURE;
	}
	status = op_tcommit();
	assert(cdb_sc_normal == status); /* if retry, an rts_error should have been signalled and we should not be here at all */
	REVERT;
	if ((0 == trig_stats[STATS_ERROR])
		&& (0 != (trig_stats[STATS_ADDED] + trig_stats[STATS_DELETED] + trig_stats[STATS_UNCHANGED]
			  + trig_stats[STATS_MODIFIED])))
	{
		util_out_print_gtmio("=========================================", FLUSH);
		util_out_print_gtmio("!UL triggers added", FLUSH, trig_stats[STATS_ADDED]);
		util_out_print_gtmio("!UL triggers deleted", FLUSH, trig_stats[STATS_DELETED]);
		util_out_print_gtmio("!UL trigger file entries not changed", FLUSH, trig_stats[STATS_UNCHANGED]);
		util_out_print_gtmio("!UL triggers modified", FLUSH, trig_stats[STATS_MODIFIED]);
		util_out_print_gtmio("=========================================", FLUSH);
	}
	else if (0 != trig_stats[STATS_ERROR])
	{
		util_out_print_gtmio("=========================================", FLUSH);
		util_out_print_gtmio("!UL trigger file entries matched existing triggers", FLUSH, trig_stats[STATS_UNCHANGED]);
		util_out_print_gtmio("!UL trigger file entries have errors", FLUSH, trig_stats[STATS_ERROR]);
		util_out_print_gtmio("!UL trigger file entries have no errors", FLUSH,
			       trig_stats[STATS_ADDED] + trig_stats[STATS_DELETED] + trig_stats[STATS_MODIFIED]);
		util_out_print_gtmio("=========================================", FLUSH);
	}
	return TRIG_SUCCESS;
}

boolean_t trigger_trgfile_tpwrap(char *trigger_filename, uint4 trigger_filename_len, boolean_t noprompt)
{
	boolean_t		trigger_error = FALSE;
	mval			ts_mv;
	int			loopcnt;

	ts_mv.mvtype = MV_STR;
	ts_mv.str.len = 0;
	ts_mv.str.addr = NULL;
	implicit_tstart = TRUE;
	op_tstart(TRUE, TRUE, &ts_mv, 0); 	/* 0 ==> save no locals but RESTART OK */
	assert(FALSE == implicit_tstart);	/* should have been reset by op_tstart at very beginning */
	/* The following for loop structure is similar to that in module trigger_update.c (function "trigger_update")
	 * and module gv_trigger.c (function gvtr_db_tpwrap) so any changes here might need to be reflected there as well.
	 */
	for ( loopcnt = 0; ; loopcnt++)
	{
		trigger_error = trigger_trgfile_tpwrap_helper(trigger_filename, trigger_filename_len, noprompt);
		if (!dollar_tlevel)
			break;
		util_out_print_gtmio("RESTART has invalidated this transaction's previous output.  New output follows.", FLUSH);
		/* We expect the above function to return with either op_tcommit or a tp_restart invoked.
		 * In the case of op_tcommit, we expect dollar_tlevel to be 0 and if so we break out of the loop.
		 * In the tp_restart case, we expect a maximum of 4 tries/retries and much lesser usually.
		 * Additionally we also want to avoid an infinite loop so limit the loop to what is considered
		 * a huge iteration count and GTMASSERT if that is reached as it suggests an out-of-design situation.
		 */
		if (TPWRAP_HELPER_MAX_ATTEMPTS < loopcnt)
			GTMASSERT;
	}
	return (TRIG_FAILURE == trigger_error);
}
