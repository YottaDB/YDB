/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdlib.h"
#include "gtm_inet.h"	/* Required for gtmsource.h */

#ifdef VMS
#include <descrip.h>		/* required for gtmsource.h */
#include <ssdef.h>
#endif
#ifdef UNIX
#include <signal.h>
#endif

#include "gtm_stdio.h"
#include "ast.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "gdskill.h"
#include "gt_timer.h"
#include "filestruct.h"
#include "gtmdbglvl.h"
#include "error.h"
#include "hashtab_mname.h"
#include "io.h"
#include "io_params.h"
#include "jnl.h"
#include "lv_val.h"
#include "rtnhdr.h"
#include "mv_stent.h"
#include "outofband.h"
#include "stack_frame.h"
#include "stringpool.h"
#include "hashtab_int4.h"	/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "tp_frame.h"
#include "tp_timeout.h"
#include "xfer_enum.h"
#include "mlkdef.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "zwrite.h"
#include "cache.h"
#include "cache_cleanup.h"
#include "objlabel.h"
#include "op.h"
#include "dpgbldir.h"
#include "preemptive_ch.h"
#include "show_source_line.h"
#include "trans_code_cleanup.h"
#include "dm_setup.h"
#include "util.h"
#include "tp_restart.h"
#include "dollar_zlevel.h"
#include "error_trap.h"
#include "golevel.h"
#include "getzposition.h"
#include "send_msg.h"
#include "jobexam_process.h"
#include "jobinterrupt_process_cleanup.h"

#ifdef UNIX
#include "ftok_sems.h"
#endif

GBLREF	spdesc		stringpool, rts_stringpool, indr_stringpool;
GBLREF	volatile int4	outofband;
GBLREF	volatile bool	std_dev_outbnd;
GBLREF	volatile bool	compile_time;
GBLREF	int		restart_pc;
GBLREF	int		t_tries;
GBLREF	unsigned char	*restart_ctxt;
GBLREF	unsigned char	*stackwarn, *tpstackwarn;
GBLREF	unsigned char	*stacktop, *tpstacktop;
GBLREF	unsigned char	*msp, *tp_sp;
GBLREF	mv_stent	*mv_chain;
GBLREF	stack_frame	*frame_pointer, *zyerr_frame, *error_frame;
GBLREF	tp_frame	*tp_pointer;
GBLREF	io_desc		*active_device;
GBLREF	lv_val		*active_lv;
GBLREF	io_pair		io_std_device, io_curr_device;
GBLREF	short		dollar_tlevel;
GBLREF	mval		dollar_ztrap;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	volatile bool	neterr_pending;
GBLREF	int		(* volatile xfer_table[])();
GBLREF	unsigned short	proc_act_type;
GBLREF	mval		**ind_result_array, **ind_result_sp;
GBLREF	mval		**ind_source_array, **ind_source_sp;
GBLREF	int		mumps_status;
GBLREF	mstr		*err_act;
GBLREF	tp_region	*tp_reg_list;		/* Chained list of regions used in this transaction not cleared on tp_restart */
GBLREF	void		(*tp_timeout_clear_ptr)(void);
GBLREF	uint4		gtmDebugLevel;		/* Debug level */
GBLREF	uint4		process_id;
GBLREF	jnlpool_addrs	jnlpool;
GBLREF	boolean_t	pool_init;
GBLREF	boolean_t	created_core;
GBLREF	boolean_t	dont_want_core;
GBLREF	mval		dollar_zstatus, dollar_zerror;
GBLREF	mval		dollar_etrap;
GBLREF	volatile int4	gtmMallocDepth;
GBLREF	int4		exi_condition;
#ifdef VMS
GBLREF	struct chf$signal_array	*tp_restart_fail_sig;
GBLREF	boolean_t		tp_restart_fail_sig_used;
#endif
GBLREF	int			merge_args;
GBLREF	lvzwrite_struct		lvzwrite_block;
GBLREF	int			process_exiting;
GBLREF	volatile boolean_t	dollar_zininterrupt;
GBLREF	boolean_t		ztrap_explicit_null;		/* whether $ZTRAP was explicitly set to NULL in this frame */
GBLREF	dollar_ecode_type	dollar_ecode;			/* structure containing $ECODE related information */
GBLREF	boolean_t		in_gvcst_incr;

#define	RUNTIME_ERROR_STR		"Following runtime error"
#define GTMFATAL_ERROR_DUMP_FILENAME	"GTM_FATAL_ERROR"

static readonly mval gtmfatal_error_filename = DEFINE_MVAL_LITERAL(MV_STR, 0, 0, sizeof(GTMFATAL_ERROR_DUMP_FILENAME) - 1,
							 		GTMFATAL_ERROR_DUMP_FILENAME, 0, 0);

boolean_t clean_mum_tstart(void);

boolean_t clean_mum_tstart(void)
{ /* We ignore errors in the $ZYERROR routine. When an error occurs, we unwind all stack frames upto and including
   * zyerr_frame. MUM_TSTART then transfers control to the $ZTRAP frame */

	stack_frame	*save_zyerr_frame, *fp;
	boolean_t	save_check_flag;

	if (NULL != zyerr_frame)
	{
		for (fp = frame_pointer; zyerr_frame; fp = fp->old_frame_pointer)
		{
			assert(fp <= zyerr_frame);
			while (tp_pointer && tp_pointer->fp <= frame_pointer)
				op_trollback(-1);
			op_unwind();
		}
		proc_act_type = 0;
		if (indr_stringpool.base == stringpool.base)
		{ /* switch to run time stringpool */
			indr_stringpool = stringpool;
			stringpool = rts_stringpool;
		}
		return TRUE;
	}
	return (NULL != err_act);
}

CONDITION_HANDLER(mdb_condition_handler)
{
	unsigned char		*cp, *context, *sp_base;
	boolean_t		dm_action;	/* did the error occur on a action from direct mode */
	boolean_t		trans_action;	/* did the error occur during "transcendental" code */
	char			src_line[MAX_ENTRYREF_LEN];
	mstr			src_line_d;
	io_desc			*err_dev;
	tp_region		*tr;
	gd_region		*reg_top, *reg_save, *reg_local;
	gd_addr			*addr_ptr;
	sgmnt_addrs		*csa;
	mval			zpos, dummy_mval;
	stack_frame		*fp;
	boolean_t		error_in_zyerror;
	boolean_t		repeat_error, etrap_handling;
	int			level;

#ifdef UNIX
	unix_db_info		*udi;
#endif

	static unsigned char dumpable_error_dump_file_parms[2] = {iop_newversion, iop_eol};
	static unsigned char dumpable_error_dump_file_noparms[1] = {iop_eol};

	error_def(ERR_NOEXCNOZTRAP);
	error_def(ERR_NOTPRINCIO);
	error_def(ERR_RTSLOC);
	error_def(ERR_SRCLOCUNKNOWN);
	error_def(ERR_LABELMISSING);
	error_def(ERR_STACKOFLOW);
	error_def(ERR_STACKCRIT);
	error_def(ERR_TPSTACKOFLOW);
	error_def(ERR_TPSTACKCRIT);
	error_def(ERR_GTMCHECK);
	error_def(ERR_CTRLC);
	error_def(ERR_CTRLY);
	error_def(ERR_CTRAP);
	error_def(ERR_UNSOLCNTERR);
	error_def(ERR_RESTART);
	error_def(ERR_TPRETRY);
	error_def(ERR_ASSERT);
	error_def(ERR_GTMASSERT);
	error_def(ERR_TPTIMEOUT);
	error_def(ERR_OUTOFSPACE);
	error_def(ERR_REPEATERROR);
	error_def(ERR_TPNOTACID);
	error_def(ERR_JOBINTRRQST);
	error_def(ERR_JOBINTRRETHROW);

	START_CH;
	if (repeat_error = (ERR_REPEATERROR == SIGNAL)) /* assignment and comparison */
		SIGNAL = dollar_ecode.error_last_ecode;
	preemptive_ch(SEVERITY);
	assert((NULL == cs_addrs) || (FALSE == cs_addrs->read_lock));
	if ((int)ERR_UNSOLCNTERR == SIGNAL)
	{
		/* ---------------------------------------------------------------------
		 * this is here for linking purposes.  We want to delay the receipt of
		 * network errors in gtm until we are ready to deal with them.  Hence
		 * the transfer table hijinx.  To avoid doing this in the gvcmz routine,
		 * we signal the error and do it here
		 * ---------------------------------------------------------------------
		 */
		neterr_pending = TRUE;
		xfer_table[xf_linefetch] = op_fetchintrrpt;
		xfer_table[xf_linestart] = op_startintrrpt;
		xfer_table[xf_forchk1] = op_startintrrpt;
		xfer_table[xf_forloop] = op_forintrrpt;
		CONTINUE;
	}
	MDB_START;
	assert(FALSE == in_gvcst_incr);	/* currently there is no known case where this can be TRUE at this point */
	in_gvcst_incr = FALSE;	/* reset this just in case gvcst_incr/gvcst_put failed to do a good job of resetting */
	/*
	 * Ideally merge should have a condition handler to reset followings, but generated code
	 * can call other routines during MERGE command. So it is not easy to establish a condition handler there.
	 * Easy solution is following one line code
	 */
	merge_args = 0;
	if ((SUCCESS != SEVERITY) && (INFO != SEVERITY))
		lvzwrite_block.curr_subsc = lvzwrite_block.subsc_count = 0;
	if ((int)ERR_TPRETRY == SIGNAL)
	{
		/* ----------------------------------------------------
		 * put the restart here for linking purposes.
		 * Utilities use T_RETRY, so calling from there causes
		 * all sorts of linking overlaps.
		 * ----------------------------------------------------
		 */
		VMS_ONLY(assert(FALSE == tp_restart_fail_sig_used);)
		tp_restart(1);
#ifdef UNIX
		if (ERR_TPRETRY == SIGNAL)		/* (signal value undisturbed) */
#elif defined VMS
		if (!tp_restart_fail_sig_used)		/* If tp_restart ran clean */
#else
#error unsupported platform
#endif
		{
			/* ------------------------------------
			 * clean up both stacks, and set mumps
			 * program counter back tstart level 1
			 * ------------------------------------
			 */
			ind_result_sp = ind_result_array;	/* clean up any active indirection pool usages */
			ind_source_sp = ind_source_array;
			MUM_TSTART;
		}
#ifdef VMS
		else
		{	/* Otherwise tp_restart had a signal that we must now deal with -- replace the TPRETRY
			   information with that saved from tp_restart. */
			/* Assert that we have room for these arguments - the array malloc is in tp_restart */
			assert(TPRESTART_ARG_CNT >= tp_restart_fail_sig->chf$is_sig_args);
			memcpy(sig, tp_restart_fail_sig, (tp_restart_fail_sig->chf$l_sig_args + 1) * sizeof(int));
			tp_restart_fail_sig_used = FALSE;
		}
#endif
	}
	if (DUMPABLE)
	{	/* Certain conditions we don't want to attempt to create the M-level ZSHOW dump.
		   1) Unix: If gtmMallocDepth > 0 indicating memory manager was active and could be reentered.
		   2) Unix: If we have a SIGBUS or SIGSEGV (could be likely to occur again
		      in the local variable code which would cause immediate shutdown with
		      no cleanup).
		   3) VMS: If we got an ACCVIO for the same as reason (2).
		   Note that we will bypass checks 2 and 3 if GDL_ZSHOWDumpOnSignal debug flag is on
		*/
		process_exiting = TRUE;		/* So zshow doesn't push stuff on stack to "protect" it when
						   we potentially already have a stack overflow */
		cancel_timer(0);		/* No interruptions now that we are dying */
		if (UNIX_ONLY(0 == gtmMallocDepth && ((SIGBUS != exi_condition && SIGSEGV != exi_condition) ||
						      (GDL_ZSHOWDumpOnSignal & gtmDebugLevel)))
		    VMS_ONLY((SS$_ACCVIO != SIGNAL) || (GDL_ZSHOWDumpOnSignal & gtmDebugLevel)))
		{	/* If dumpable condition, create traceback file of M stack info and such */
			/* Set ZSTATUS so it will be echo'd properly in the dump */
			src_line_d.addr = src_line;
			src_line_d.len = 0;
			if (!repeat_error)
			{
				SET_ZSTATUS(NULL);
			}
			/* On Unix, we need to push out our error now before we potentially overlay it in jobexam_process() */
			UNIX_ONLY(PRN_ERROR);
			/* Create dump file */
			jobexam_process(&gtmfatal_error_filename, &dummy_mval);
		} else
		{
			UNIX_ONLY(PRN_ERROR);
		}

		/* If we are about to core/exit on a stack over flow, only do the core part if a debug
		   flag requests this behaviour. Otherwise, supress the core and just exit.
		   2006-03-07 se: If a stack overflow occurs on VMS, it has happened that the stack is no
		   longer well formed so attempting to unwind it as it does in MUMPS_EXIT causes things
		   to really become screwed up. For this reason, this niceness of avoiding a dump on a
		   stack overflow on VMS is being disabled. The dump can be controlled wih set proc/dump
		   (or not) as desired.
		*/
#ifndef VMS
		if ((int)ERR_STACKOFLOW == SIGNAL && !(GDL_DumpOnStackOFlow & gtmDebugLevel))
		{
			MUMPS_EXIT;	/* Do a clean exit rather than messy core exit */
		}
#endif
		gtm_dump();
		TERMINATE;
	}
	if (active_lv)
	{
		if (!MV_DEFINED(&active_lv->v) && !active_lv->ptrs.val_ent.children)
			op_kill(active_lv);
		active_lv = (lv_val *)0;
	}
	/* -----------------------------------------------------------
	 * Don't release crit:
	 * -- unless SEVERITY is at least "WARNING".
	 * -- until after TP retries have been handled and
	 *    dumpable errors have dumped
	 * NOTE: holding crit till after dump can stop the world for
	 * a while. That is acceptable because:
	 * -- It's better to make other processes wait, to ensure
	 *    dump reflects state at time of error.
	 * -- Dumping above this point prevents a second trip
	 *    through here when an error occurs in rel_crit().
	 * NOTE: Release of crit during "final" TP retry can trigger
	 *    an assert failure (in dbg/bta builds only), if execution
	 *    continues and no TROLLBACK is issued.
	 * -----------------------------------------------------------
	 */
	if ((SUCCESS != SEVERITY) && (INFO != SEVERITY))
	{	/* Note the existence of similar code in op_dmode.c and op_zsystem.c.
		 * Any changes here should be reflected there too. We don't have a macro for this because
		 * 	(a) This code is considered pretty much stable.
		 * 	(b) Making it a macro makes it less readable.
		 */
		if ((CDB_STAGNATE <= t_tries) && (0 < dollar_tlevel))
		{
			assert(CDB_STAGNATE == t_tries);
			t_tries = CDB_STAGNATE - 1;
			getzposition(&zpos);
			send_msg(VARLSTCNT(8) ERR_TPNOTACID, 4, LEN_AND_LIT(RUNTIME_ERROR_STR), zpos.str.len, zpos.str.addr,
						SIGNAL, 0);
		}
		ENABLE_AST
		for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
		{
			for (reg_local = addr_ptr->regions, reg_top = reg_local + addr_ptr->n_regions;
				reg_local < reg_top; reg_local++)
			{
				if (reg_local->open && !reg_local->was_open)
				{
					csa = (sgmnt_addrs *)&FILE_INFO(reg_local)->s_addrs;
					if (csa && (csa->now_crit))
						rel_crit(reg_local);
				}
			}
		}
		UNIX_ONLY(
			/* Release FTOK lock on the replication instance file if holding it (possible if error in jnlpool_init) */
			assert((NULL == jnlpool.jnlpool_dummy_reg) || jnlpool.jnlpool_dummy_reg->open);
			if ((NULL != jnlpool.jnlpool_dummy_reg) && jnlpool.jnlpool_dummy_reg->open)
			{
				udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
				assert(NULL != udi);
				if (NULL != udi)
				{
					if (udi->grabbed_ftok_sem)
						ftok_sem_release(jnlpool.jnlpool_dummy_reg, FALSE, FALSE);
					assert(!udi->grabbed_ftok_sem);
				}
			}
		)
		/* Release crit lock on journal pool if holding it */
		if (pool_init) /* atleast one region replicated and we have done jnlpool init */
		{
			csa = (sgmnt_addrs *)&FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;
			if (csa && csa->now_crit)
				rel_lock(jnlpool.jnlpool_dummy_reg);
		}
	}
	err_dev = active_device;
	active_device = (io_desc *)0;
	ind_result_sp = ind_result_array;	/* clean up any active indirection pool usages */
	ind_source_sp = ind_source_array;
	dm_action = frame_pointer->old_frame_pointer->type & SFT_DM  ||
					compile_time && frame_pointer->type & SFT_DM;
	/* The errors are said to be transcendental when they occur during compilation/execution
	 * of the error trap ({z,e}trap, device exception) or $zinterrupt. The errors in other
	 * indirect code frames (zbreak, zstep, xecute etc.) aren't defined to be trancendental
	 * and will be treated  as if they occured in a regular code frame. */
	trans_action = proc_act_type || (frame_pointer->type & SFT_ZTRAP) || (frame_pointer->type & SFT_DEV_ACT);
	src_line_d.addr = src_line;
	src_line_d.len = 0;
	flush_pio();
	if ((int)ERR_CTRLY == SIGNAL)
	{
		outofband_clear();
		assert(NULL != (unsigned char *)restart_pc);
		frame_pointer->mpc = (unsigned char *)restart_pc;
		frame_pointer->ctxt = restart_ctxt;
		MUM_TSTART;
	} else  if ((int)ERR_CTRLC == SIGNAL)
	{
		outofband_clear();
		if (!trans_action && !dm_action)
		{
			frame_pointer->mpc = (unsigned char *)restart_pc;
			frame_pointer->ctxt = restart_ctxt;
			assert(NULL != frame_pointer->mpc);
			if (!(frame_pointer->type & SFT_DM))
				dm_setup();
		} else  if (frame_pointer->type & SFT_DM)
		{
			frame_pointer->ctxt = CONTEXT(call_dm);
			frame_pointer->mpc = CODE_ADDRESS(call_dm);
		} else
		{
			/* Do cleanup on indirect frames prior to reset */
			IF_INDR_FRAME_CLEANUP_CACHE_ENTRY_AND_UNMARK(frame_pointer);
			frame_pointer->ctxt = CONTEXT(pseudo_ret);
			frame_pointer->mpc = CODE_ADDRESS(pseudo_ret);
		}
		PRN_ERROR;
		if (io_curr_device.out != io_std_device.out)
		{
			dec_err(VARLSTCNT(4) ERR_NOTPRINCIO, 2, io_curr_device.out->trans_name->len,
				io_curr_device.out->trans_name->dollar_io);
		}
		MUM_TSTART;
	} else if ((int)ERR_CTRAP == SIGNAL)
	{
		outofband_clear();
		if (!trans_action && !dm_action && !(frame_pointer->type & SFT_DM))
		{
			sp_base = stringpool.base;
			if (sp_base != rts_stringpool.base)
			{
				indr_stringpool = stringpool;	/* update indr_stringpool */
				stringpool = rts_stringpool;	/* change for set_zstatus */
			}
			if (!repeat_error)
			{
				dollar_ecode.error_last_b_line = SET_ZSTATUS(0);
			}
			if (sp_base != rts_stringpool.base)
			{
				rts_stringpool = stringpool;	/* update rts_stringpool */
				stringpool = indr_stringpool;	/* change back */
			}
			assert(NULL != dollar_ecode.error_last_b_line);
			assert(NULL != (unsigned char *)restart_pc);
			frame_pointer->mpc = (unsigned char *)restart_pc;
			frame_pointer->ctxt = restart_ctxt;
			err_act = NULL;
			dollar_ecode.error_last_ecode = SIGNAL;
			if (std_dev_outbnd && io_std_device.in && io_std_device.in->type == tt &&
				io_std_device.in->error_handler.len)
			{
				proc_act_type = SFT_DEV_ACT;
				err_act = &io_std_device.in->error_handler;
			} else  if (!std_dev_outbnd && err_dev && (err_dev->type == tt) && err_dev->error_handler.len)
			{
				proc_act_type = SFT_DEV_ACT;
				err_act = &err_dev->error_handler;
			} else if (NULL != error_frame)
			{	/* a primary error occurred already. irrespective of whether ZTRAP or ETRAP is active now,
				 * we need to consider this as a nested error and trigger nested error processing.
				 */
				goerrorframe(error_frame);
				assert(error_frame == frame_pointer);
				SET_ERROR_FRAME(frame_pointer);	/* reset dollar_ecode.error_frame to frame_pointer as well as reset
								 * error_frame_mpc, error_frame_ctxt and error_frame->{mpc,ctxt} */
				proc_act_type = 0;
			} else if (0 != dollar_ztrap.str.len)
			{
				proc_act_type = SFT_ZTRAP;
				err_act = &dollar_ztrap.str;
			} else
			{	/* either $ETRAP is empty-string or non-empty.
				 * if non-empty, use $ETRAP for error-handling.
				 * if     empty,
				 * 	if ztrap_explicit_null is FALSE use empty-string $ETRAP for error-handling
				 * 	if ztrap_explicit_null is TRUE  unwind as many frames as possible until we see a frame
				 * 					where ztrap_explicit_null is FALSE and $ZTRAP is NULL.
				 * 					in that frame, use $ETRAP for error-handling.
				 * 					if no such frame is found, exit after printing the error.
				 */
				etrap_handling = TRUE;
				if (ztrap_explicit_null)
				{
					assert(0 == dollar_etrap.str.len);
					for (level = dollar_zlevel() - 1; level > 0; level--)
					{
						golevel(level);
						assert(level == dollar_zlevel());
						if (!ztrap_explicit_null && !dollar_ztrap.str.len)
							break;
					}
					if (0 >= level)
					{
						assert(0 == level);
						etrap_handling = FALSE;
					}
				}
				if (SFF_CI & frame_pointer->flags)
				{ /* Unhandled errors from called-in routines should return to gtm_ci() with error status */
					mumps_status = SIGNAL;
					MUM_TSTART;
				}
				else if (etrap_handling)
				{
					proc_act_type = SFT_ZTRAP;
					err_act = &dollar_etrap.str;
				} else
				{
					PRN_ERROR;
					rts_error(VARLSTCNT(1) ERR_NOEXCNOZTRAP);
				}
			}
			if (clean_mum_tstart())
				MUM_TSTART;
		} else  if (frame_pointer->type & SFT_DM)
		{
			frame_pointer->ctxt = CONTEXT(call_dm);
			frame_pointer->mpc = CODE_ADDRESS(call_dm);
		} else
		{
			/* Do cleanup on indirect frames prior to reset */
			IF_INDR_FRAME_CLEANUP_CACHE_ENTRY_AND_UNMARK(frame_pointer);
			frame_pointer->ctxt = CONTEXT(pseudo_ret);
			frame_pointer->mpc = CODE_ADDRESS(pseudo_ret);
		}
		PRN_ERROR;
		if (io_curr_device.out != io_std_device.out)
		{
			dec_err(VARLSTCNT(4) ERR_NOTPRINCIO, 2, io_curr_device.out->trans_name->len,
				io_curr_device.out->trans_name->dollar_io);
		}
		MUM_TSTART;
	} else  if ((int)ERR_JOBINTRRQST == SIGNAL)
	{
		assert(NULL != (unsigned char *)restart_pc);
		frame_pointer->mpc = (unsigned char *)restart_pc;
		frame_pointer->ctxt = restart_ctxt;
		assert(!dollar_zininterrupt);
		dollar_zininterrupt = TRUE;	/* Note done before outofband is cleared to prevent nesting */
		outofband_clear();
		proc_act_type = SFT_ZINTR | SFT_COUNT;	/* trans_code will invoke jobinterrupt_process for us */
		MUM_TSTART;
	} else  if ((int)ERR_JOBINTRRETHROW == SIGNAL)
	{ /* job interrupt is rethrown from TC/TRO */
		assert(!dollar_zininterrupt);
		dollar_zininterrupt = TRUE;
		proc_act_type = SFT_ZINTR | SFT_COUNT; /* trans_code will invoke jobinterrupt_process for us */
		MUM_TSTART;
	} else  if ((int)ERR_STACKCRIT == SIGNAL)
	{
		assert(msp > stacktop);
		cp = stackwarn;
		stackwarn = stacktop;
		push_stck(cp, 0, (void**)&stackwarn);
		push_stck(stacktop, 0, (void**)&stacktop);
	}
	if (!repeat_error)
		dollar_ecode.error_last_b_line = NULL;
	/* ----------------------------------------------------------------
	 * error from direct mode actions does not set $zstatus and is not
	 * restarted (dollar_ecode.error_last_b_line = NULL); error from transcendental
	 * code does set $zstatus but does not restart the line
	 * ----------------------------------------------------------------
	 */
	if (!dm_action)
	{
		sp_base = stringpool.base;
		if (sp_base != rts_stringpool.base)
		{
			indr_stringpool = stringpool;	/* update indr_stringpool */
			stringpool = rts_stringpool;	/* change for set_zstatus */
		}
		if (!repeat_error)
		{
			dollar_ecode.error_last_b_line = SET_ZSTATUS(&context);
		}
		assert(NULL != dollar_ecode.error_last_b_line);
		if (sp_base != rts_stringpool.base)
		{
			rts_stringpool = stringpool;	/* update rts_stringpool */
			stringpool = indr_stringpool;	/* change back */
		}
	}
	if ((SUCCESS == SEVERITY) || (INFO == SEVERITY))
	{
		PRN_ERROR;
		CONTINUE;
	}
	/* -----------------------------------------------------------------------
	 * This call to clear TP timeout is like the one currently in op_halt, in
	 * case there's another path with a similar need. If so, it would likely
	 * go through here.
	 * -----------------------------------------------------------------------
	 */
	(*tp_timeout_clear_ptr)();

	/* -----------------------------------------------------------------------
	 * Reset the line, or set frame to return if it was "transcendental" code.
	 * If we are in $ZYERROR, we don't care about restarting the line that
	 * errored since we will unwind all frames upto and including zyerr_frame.
	 * -----------------------------------------------------------------------
	 */
	if (FALSE == compile_time && !trans_action && !dm_action && NULL == zyerr_frame)
	{
		for (fp = frame_pointer;  fp;  fp = fp->old_frame_pointer)
		{
			/* See if this is a $ZINTERRUPT frame. If yes, we want to restart *this* line
			   at the beginning. Since it is always an indirect frame, we can use the context
			   pointer to start over. Only do this if $ZTRAP is active though. $ETRAP does
			   things somewhat differently in that the current frame is always returned from.
			*/
			if ((0 < dollar_ztrap.str.len) && (SFT_ZINTR & fp->type))
			{
				assert(SFF_INDCE & fp->flags);
				fp->mpc = fp->ctxt;
				break;
			}
			/* Do cleanup on indirect frames prior to reset */
			IF_INDR_FRAME_CLEANUP_CACHE_ENTRY_AND_UNMARK(fp);

			/* mpc points to PTEXT */
			/*The equality check in the second half of the expression below is to
			  account for the delay-slot in HP-UX for implicit quits. Not an issue here,
			  but added for uniformity. */
			if (ADDR_IN_CODE(fp->mpc, fp->rvector))
			{
				if (dollar_ztrap.str.len > 0)
				{
					/* GT.M specific error trapping
					 * retries the line with the error
					 */
					fp->mpc = dollar_ecode.error_last_b_line;
					fp->ctxt = context;
				}
				break;
			} else
			{
				fp->ctxt = CONTEXT(pseudo_ret);
				fp->mpc = CODE_ADDRESS(pseudo_ret);
			}
		}
	}
	/* ----------------------------------------------------------------
	 * error from direct mode actions or "transcendental" code does not
	 * invoke MUMPS error handling routines
	 * ----------------------------------------------------------------
	 */
	if (!dm_action && !trans_action)
	{
		if (compile_time)
		{	/* This is setting the execution of this frame to instead give
			   an error because the compile of that code failed. If this
			   frame is marked for indr cache cleanup, do that cleanup now
			   and unmark the frame.
			*/
			IF_INDR_FRAME_CLEANUP_CACHE_ENTRY_AND_UNMARK(frame_pointer);
			frame_pointer->mpc = dollar_ecode.error_last_b_line;
			frame_pointer->ctxt = context;
		}
		err_act = NULL;
		dollar_ecode.error_last_ecode = SIGNAL;
		if (err_dev && err_dev->error_handler.len && ((int)ERR_TPTIMEOUT != SIGNAL))
		{
			proc_act_type = SFT_DEV_ACT;
			err_act = &err_dev->error_handler;
		} else if (NULL != error_frame)
		{	/* a primary error occurred already. irrespective of whether ZTRAP or ETRAP is active now, we need to
			 * consider this as a nested error and trigger nested error processing.
			 */
			goerrorframe(error_frame);
			assert(error_frame == frame_pointer);
			SET_ERROR_FRAME(frame_pointer);	/* reset dollar_ecode.error_frame to frame_pointer as well as reset
							 * error_frame_mpc, error_frame_ctxt and error_frame->{mpc,ctxt} */
			proc_act_type = 0;
			MUM_TSTART;	/* unwind the current C-stack and restart executing from the top of the current M-stack */
			assert(FALSE);
		} else if (0 != dollar_ztrap.str.len)
		{
			assert(!ztrap_explicit_null);
			proc_act_type = SFT_ZTRAP;
			err_act = &dollar_ztrap.str;
		} else
		{	/* either $ETRAP is empty-string or non-empty.
			 * if non-empty, use $ETRAP for error-handling.
			 * if     empty,
			 * 	if ztrap_explicit_null is FALSE use empty-string $ETRAP for error-handling
			 * 	if ztrap_explicit_null is TRUE  unwind as many frames as possible until we see a frame
			 * 					where ztrap_explicit_null is FALSE and $ZTRAP is NULL.
			 * 					in that frame, use $ETRAP for error-handling.
			 * 					if no such frame is found, exit after printing the error.
			 */
			etrap_handling = TRUE;
			if (ztrap_explicit_null)
			{
				assert(0 == dollar_etrap.str.len);
				for (level = dollar_zlevel() - 1; level > 0; level--)
				{
					golevel(level);
					assert(level == dollar_zlevel());
					if (!ztrap_explicit_null && !dollar_ztrap.str.len)
						break;
				}
				if (0 >= level)
				{
					assert(0 == level);
					etrap_handling = FALSE;
				}
			}
			if (SFF_CI & frame_pointer->flags)
			{ /* Unhandled errors from called-in routines should return to gtm_ci() with error status */
				mumps_status = SIGNAL;
				MUM_TSTART;
			}
			else if (etrap_handling)
			{
				proc_act_type = SFT_ZTRAP;
				err_act = &dollar_etrap.str;
			}
		}
		if (clean_mum_tstart())
			MUM_TSTART;
	}
	if ((SFT_ZINTR | SFT_COUNT) != proc_act_type || 0 == dollar_ecode.error_last_b_line)
	{	/* No user console error for $zinterrupt compile problems and if not direct mode. Accomplish
		   this by bypassing the code inside this if which *will* be executed for most cases
		*/
		PRN_ERROR;
		if (compile_time && ((int)ERR_LABELMISSING) != SIGNAL)
			show_source_line();
	}
	if (!dm_action && !trans_action && (0 != src_line_d.len))
	{
		if (MSG_OUTPUT)
			dec_err(VARLSTCNT(4) ERR_RTSLOC, 2, src_line_d.len, src_line_d.addr);
	} else
	{
		if (trans_action || dm_action)
		{	/* If true transcendental, do trans_code_cleanup. If our counted frame that
			   is masquerading as a transcendental frame, run jobinterrupt_process_clean
			*/
			if (!(SFT_ZINTR & proc_act_type))
				trans_code_cleanup();
			else
				jobinterrupt_process_cleanup();
			MUM_TSTART;
		} else
		{
			if (MSG_OUTPUT)
				dec_err(VARLSTCNT(1) ERR_SRCLOCUNKNOWN);
		}
	}
	MUMPS_EXIT;
}
