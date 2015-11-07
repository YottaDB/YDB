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

#include "gtm_string.h"
#include "gtm_stdlib.h"
#include "gtm_inet.h"	/* Required for gtmsource.h */
#include "gtm_stdio.h"

#ifdef VMS
# include <descrip.h>	/* required for gtmsource.h */
# include <ssdef.h>
#endif
#ifdef UNIX
# include <signal.h>
# include <stdarg.h>
#endif

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
#include <rtnhdr.h>
#include "mv_stent.h"
#include "outofband.h"
#include "stack_frame.h"
#include "stringpool.h"
#include "hashtab_int4.h"	/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "tp_frame.h"
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
#include "preemptive_db_clnup.h"
#include "compiler.h"		/* needed for MAX_SRCLINE */
#include "show_source_line.h"
#include "trans_code_cleanup.h"
#include "dm_setup.h"
#include "util.h"
#include "tp_restart.h"
#include "dollar_zlevel.h"
#include "error_trap.h"
#include "golevel.h"
#include "send_msg.h"
#include "jobinterrupt_process_cleanup.h"
#include "fix_xfer_entry.h"
#include "change_reg.h"
#include "tp_change_reg.h"
#include "alias.h"
#include "create_fatal_error_zshow_dmp.h"
#include "have_crit.h"
#ifdef UNIX
# include "iormdef.h"
# include "ftok_sems.h"
# include "gtm_putmsg_list.h"
#endif
#ifdef GTM_TRIGGER
# include "gv_trigger.h"
# include "gtm_trigger.h"
#endif

GBLREF	spdesc		stringpool, rts_stringpool, indr_stringpool;
GBLREF	volatile int4	outofband;
GBLREF	volatile bool	std_dev_outbnd;
GBLREF	unsigned char   *restart_pc;
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
GBLREF	mval		dollar_ztrap;
GBLREF	volatile bool	neterr_pending;
GBLREF	xfer_entry_t	xfer_table[];
GBLREF	unsigned short	proc_act_type;
GBLREF	int		mumps_status;
GBLREF	mstr		*err_act;
GBLREF	tp_region	*tp_reg_list;		/* Chained list of regions used in this transaction not cleared on tp_restart */
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
GBLREF	inctn_opcode_t	inctn_opcode;
#ifdef VMS
GBLREF	struct chf$signal_array	*tp_restart_fail_sig;
GBLREF	boolean_t		tp_restart_fail_sig_used;
#endif
GBLREF	int			merge_args;
GBLREF	lvzwrite_datablk	*lvzwrite_block;
GBLREF	volatile boolean_t	dollar_zininterrupt;
GBLREF	boolean_t		ztrap_explicit_null;		/* whether $ZTRAP was explicitly set to NULL in this frame */
GBLREF	dollar_ecode_type	dollar_ecode;			/* structure containing $ECODE related information */
GBLREF	boolean_t		in_gvcst_incr;
GBLREF	gv_namehead		*gv_target;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_currkey;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	sgm_info		*first_sgm_info;
GBLREF	dollar_stack_type	dollar_stack;
GBLREF	mval			*alias_retarg;
#ifdef UNIX
GBLREF	jnl_gbls_t		jgbl;
#endif
GBLREF	io_desc			*gtm_err_dev;
#ifdef GTM_TRIGGER
GBLREF	int			tprestart_state;		/* When triggers restart, multiple states possible.
								   See tp_restart.h */
GBLREF	int4			gtm_trigger_depth;
#endif
#ifdef DEBUG
GBLREF	boolean_t		donot_INVOKE_MUMTSTART;
#endif

error_def(ERR_ASSERT);
error_def(ERR_CTRAP);
error_def(ERR_CTRLC);
error_def(ERR_CTRLY);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_GTMERREXIT);
error_def(ERR_JOBINTRRETHROW);
error_def(ERR_JOBINTRRQST);
error_def(ERR_LABELMISSING);
error_def(ERR_MEMORY);
error_def(ERR_NOEXCNOZTRAP);
error_def(ERR_NOTPRINCIO);
error_def(ERR_OUTOFSPACE);
error_def(ERR_REPEATERROR);
error_def(ERR_RESTART);
error_def(ERR_RTSLOC);
error_def(ERR_SRCLOCUNKNOWN);
error_def(ERR_STACKCRIT);
error_def(ERR_STACKOFLOW);
error_def(ERR_TLVLZERO);
error_def(ERR_TPRETRY);
error_def(ERR_TPRESTNESTERR);
error_def(ERR_TPSTACKCRIT);
error_def(ERR_TPSTACKOFLOW);
error_def(ERR_TPTIMEOUT);
error_def(ERR_UNSOLCNTERR);
error_def(ERR_VMSMEMORY);

boolean_t clean_mum_tstart(void);
void setup_error(sgmnt_addrs *csa, int argcnt, ...);

#ifdef UNIX
/* When we restart generated code after handling an error, verify that we are not in the frame or one created on its
 * behalf that invoked a trigger or spanning node and caused a dynamic TSTART to be done on its behalf. This can happen
 * for example if a trigger is invoked for the first time but gets a compilation or link failure error or if a spanning
 * node fetch or update drives an error. In the trigger case, the relevant error is thrown from gtm_trigger() while no
 * trigger based error handling is in effect so no rollback of the dynamic frame occurs which results in unhandled TPQUIT
 * errors, perhaps interminably. In both the trigger and spanning-node cases, the MUM_TSTART we are about to execute unrolls
 * the C stack preventing any return to the C frame that did the implicit tstart and prevents it from being committed so
 * it must be rolled back.
 */
#define MUM_TSTART_FRAME_CHECK										\
{													\
	if (GTMTRIG_ONLY((0 == gtm_trigger_depth) &&) tp_pointer && tp_pointer->implicit_tstart) 	\
	{												\
		DEBUG_ONLY(donot_INVOKE_MUMTSTART = FALSE);						\
		OP_TROLLBACK(-1);	/* Unroll implicit TP frame */					\
	}												\
}
#else
#define MUM_TSTART_FRAME_CHECK
#endif


/* We ignore errors in the $ZYERROR routine. When an error occurs, we unwind all stack frames upto and including
 * zyerr_frame. MUM_TSTART then transfers control to the $ZTRAP frame.
 */
boolean_t clean_mum_tstart(void)
{
	stack_frame	*save_zyerr_frame, *fp, *fpprev;
	boolean_t	save_check_flag;

	if (NULL != zyerr_frame)
	{
		while ((NULL != frame_pointer) && (NULL != zyerr_frame))
		{
			GOFRAMES(1, TRUE, FALSE);
		}
		assert(NULL != frame_pointer);
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

#ifdef UNIX
/* Routine to setup an error in util_outbuff as if rts_error had put it there. Used when we morph ERR_TPRETRY
 * to ERR_TPRESTNESTERR. Requires a va_list var containing the args so do this in this separate routine.
 */
void setup_error(sgmnt_addrs *csa, int argcnt, ...)
{
	va_list		var;

	VAR_START(var, argcnt);
	gtm_putmsg_list(csa, argcnt, var);
	va_end(var);
}
#endif

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
	stack_frame		*fp;
	boolean_t		error_in_zyerror;
	boolean_t		repeat_error, etrap_handling, reset_mpc;
	int			level, rc;
	lv_val			*lvptr;
	boolean_t		reserve_sock_dev = FALSE;
#	ifdef UNIX
	unix_db_info		*udi;
	stack_frame		*lcl_error_frame;
	mv_stent		*mvst;
#	endif

	START_CH;
	DBGEHND((stderr, "mdb_condition_handler: Entered with SIGNAL=%d frame_pointer=0x"lvaddr"\n", SIGNAL, frame_pointer));
	if (NULL != gtm_err_dev)
	{
		/* It is possible that we entered here from a bad compile of the OPEN exception handler
		 * for a device.  If gtm_err_dev is still set from the previous mdb_condition_handler
		 * invocation that drove the error handler that occurred during the OPEN command, then its
		 * structures should be released now.
		 */
		if (gtmsocket == gtm_err_dev->type)
			iosocket_destroy(gtm_err_dev);
#		ifdef UNIX
		else
			remove_rms(gtm_err_dev);
#		endif
		gtm_err_dev = NULL;
	}
	if (repeat_error = (ERR_REPEATERROR == SIGNAL)) /* assignment and comparison */
		SIGNAL = dollar_ecode.error_last_ecode;
	preemptive_db_clnup(SEVERITY);
	if (NULL != alias_retarg)
	{	/* An error has occurred while an alias return arg was in-flight. Delivery won't happen now
		 * so we need to remove the extra counts that were added in unw_retarg() and dis-enchant
		 * the alias container itself.
		 */
		assert(alias_retarg->mvtype & MV_ALIASCONT);
		if (alias_retarg->mvtype & MV_ALIASCONT)
		{	/* Protect the refs were are about to make in case ptr got banged up somehow */
			lvptr = (lv_val *)alias_retarg->str.addr;
			assert(LV_IS_BASE_VAR(lvptr));
			DECR_CREFCNT(lvptr);
			DECR_TREFCNT(lvptr);
		}
		alias_retarg->mvtype = 0;	/* Kill the temp var (no longer a container) */
		alias_retarg = NULL;		/* .. and no more in-flight return argument */
	}
	if ((int)ERR_UNSOLCNTERR == SIGNAL)
	{
		/* This is here for linking purposes.  We want to delay the receipt of
		 * network errors in gtm until we are ready to deal with them.  Hence
		 * the transfer table hijinx.  To avoid doing this in the gvcmz routine,
		 * we signal the error and do it here
		 */
		neterr_pending = TRUE;
                FIX_XFER_ENTRY(xf_linefetch, op_fetchintrrpt);
                FIX_XFER_ENTRY(xf_linestart, op_startintrrpt);
                FIX_XFER_ENTRY(xf_forchk1, op_startintrrpt);
                FIX_XFER_ENTRY(xf_forloop, op_forintrrpt);
		CONTINUE;
	}
	MDB_START;
	assert(FALSE == in_gvcst_incr);	/* currently there is no known case where this can be TRUE at this point */
	in_gvcst_incr = FALSE;	/* reset this just in case gvcst_incr/gvcst_put failed to do a good job of resetting */
	/* Ideally merge should have a condition handler to reset followings, but generated code can call other routines
	 * during MERGE command (MERGE command invokes multiple op-codes depending on source vs target). So it is not
	 * easy to establish a condition handler there. Easy solution is following one line code.
	 */
	merge_args = 0;
	TREF(in_zwrite) = FALSE;
	inctn_opcode = inctn_invalid_op;
	if ((SUCCESS != SEVERITY) && (INFO != SEVERITY))
	{
		if (lvzwrite_block)
			/* If lvzwrite_block does not (yet) exist, no harm, no foul */
			lvzwrite_block->curr_subsc = lvzwrite_block->subsc_count = 0;
	}
	if ((int)ERR_TPRETRY == SIGNAL)
	{
#		ifdef UNIX
		lcl_error_frame = error_frame;
		if ((NULL == lcl_error_frame) && dollar_zininterrupt)
		{	/* We are in a $zininterrupt handler AND have a restart request. See if we were in error processing
			 * when we started the interrupt. We can locate this in the zinterrupt mv_stent. Note this loop
			 * does not process the last mv_stent on the stack but since that is the MVST_STORIG entry, this is ok
			 * as we expect to find a ZINTRupt entry long before that.
			 */
			for (mvst = mv_chain; 0 != mvst->mv_st_next; mvst = (mv_stent *)(mvst->mv_st_next + (char *)mvst))
			{
				if (MVST_ZINTR == mvst->mv_st_type)
					break;
			}
			assertpro(MVST_ZINTR == mvst->mv_st_type); /* No zinterrupt block, big problemo */
			lcl_error_frame = mvst->mv_st_cont.mvs_zintr.error_frame_save;
		}
		if (NULL == lcl_error_frame)
#		endif
		{
			VMS_ONLY(assert(FALSE == tp_restart_fail_sig_used));
#			ifdef GTM_TRIGGER
			/* Assert that we never end up invoking the MUM_TSTART macro handler in case of an implicit tstart restart.
			 * See GBLDEF of skip_INVOKE_RESTART and donot_INVOKE_MUMTSTART in gbldefs.c for more information.
			 * Note that it is possible for this macro to be invoked from generated code in a trigger frame (in which
			 * case gtm_trigger/tp_restart ensure control passed to mdb_condition_handler only until the outermost
			 * implicit tstart in which case they return). Assert accordingly.
			 */
			assert(!donot_INVOKE_MUMTSTART || gtm_trigger_depth);
			TRIGGER_BASE_FRAME_UNWIND_IF_NOMANSLAND;
#			endif
			rc = tp_restart(1, TP_RESTART_HANDLES_ERRORS);
			DBGEHND((stderr, "mdb_condition_handler: tp_restart returned with rc=%d. state=%d, and SIGNAL=%d\n",
				 rc, GTMTRIG_ONLY(tprestart_state) NON_GTMTRIG_ONLY(0), error_condition));
#			ifdef GTM_TRIGGER
			if (0 != rc)
			{	/* The only time "tp_restart" will return non-zero is if the error needs to be
				 * rethrown. To accomplish that, we will unwind this handler which will return to
				 * the inner most initiating dm_start() with the return code set to whatever mumps_status
				 * is set to.
				 */
				assert(TPRESTART_STATE_NORMAL != tprestart_state);
				assert(rc == SIGNAL);
				assertpro((SFT_TRIGR & frame_pointer->type) && (0 < gtm_trigger_depth));
				mumps_status = rc;
				DBGEHND((stderr, "mdb_condition_handler: Unwind-return to caller (gtm_trigger)\n"));
				UNWIND(NULL, NULL);
			}
			/* "tp_restart" has succeeded so we have unwound back to the return point but check if the
			 * transaction was initiated by an implicit trigger TSTART. This can occur if an error was
			 * encountered in a trigger before the trigger base-frame was setup. It can occur at any trigger
			 * level if a triggered update is preceeded by a TROLLBACK.
			 */
			if (!(SFT_TRIGR & frame_pointer->type) && tp_pointer && tp_pointer->implicit_tstart)
			{
				mumps_status = rc;
				DBGEHND((stderr, "mdb_condition_handler: Returning to implicit TSTART originator\n"));
				UNWIND(NULL, NULL);
			}
			assert(!donot_INVOKE_MUMTSTART);
#			endif
#			ifdef UNIX
			if (ERR_TPRETRY == SIGNAL)		/* (signal value undisturbed) */
#			elif defined VMS
			if (!tp_restart_fail_sig_used)		/* If tp_restart ran clean */
#			endif
			{
				/* Set mumps program counter back tstart level 1 */
				MUM_TSTART;
			}
#			ifdef VMS
			else
			{	/* Otherwise tp_restart had a signal that we must now deal with -- replace the TPRETRY
				 * information with that saved from tp_restart.
				 * Assert that we have room for these arguments - the array malloc is in tp_restart
				 */
				assert(TPRESTART_ARG_CNT >= tp_restart_fail_sig->chf$is_sig_args);
				memcpy(sig, tp_restart_fail_sig, (tp_restart_fail_sig->chf$l_sig_args + 1) * SIZEOF(int));
				tp_restart_fail_sig_used = FALSE;
			}
#			endif
		}
#		ifdef UNIX
		else
		{
			if (0 == dollar_tlevel)
				SIGNAL = ERR_TLVLZERO;		/* TPRESTART specified but not in TP */
			else
				SIGNAL = ERR_TPRESTNESTERR;	/* Only if actually in TP */
			/* TPRETRY encountered or requested during error handling - treat as nested error to prevent issues
			 * with errors being rethrown during a TP restart. Change the error from TPRETRY to either TLVLZERO or
			 * TPRESTNESTERR as appropriate so we don't give internal use only error name to user and let error
			 * continue through regular processing (both treated as nested error since error_frame non-NULL).
			 */
			setup_error(gv_target ? gv_target->gd_csa : NULL, VARLSTCNT(1) SIGNAL);
		}
#		endif
	}
	/* Ensure gv_target and cs_addrs are in sync. If not, make them so. */
	if (NULL != gv_target)
	{
		csa = gv_target->gd_csa;
		if (NULL != csa)
		{
			if (csa != cs_addrs)
			{
				assert(0 < csa->regcnt);
				/* If csa->regcnt is > 1, it is possible that csa->region is different from the actual gv_cur_region
				 * (before we encountered the runtime error). This is a case of two regions mapping to the same csa.
				 * The only issue with this is that some user-level error messages that have the region name (as
				 * opposed to the database file name) could print incorrect values. But other than that there should
				 * be no issues since finally the csa (corresponding to the physical database file) is what matters
				 * and that is the same for both the regions. Given that the region mismatch potential exists only
				 * until the next global reference which is different from $REFERENCE, we consider this acceptable.
				 */
				gv_cur_region = csa->region;
				assert(gv_cur_region->open);
				assert((dba_mm == gv_cur_region->dyn.addr->acc_meth)
					|| (dba_bg == gv_cur_region->dyn.addr->acc_meth));
				/* The above assert is needed to ensure that change_reg/tp_change_reg (invoked below)
				 * will set cs_addrs, cs_data etc. to non-zero values.
				 */
				if (NULL != first_sgm_info)
					change_reg(); /* updates "cs_addrs", "cs_data", "sgm_info_ptr" and maybe "first_sgm_info" */
				else
				{	/* We are either inside a non-TP transaction or a TP transaction that has done NO database
					 * references. In either case, we do NOT want to setting sgm_info_ptr or first_sgm_info.
					 * Hence use tp_change_reg instead of change_reg below.
					 */
					tp_change_reg(); /* updates "cs_addrs", "cs_data" */
				}
				assert(cs_addrs == csa);
				assert(cs_data == csa->hdr);
				assert(NULL != cs_data);
			}
			/* Fix gv_currkey to null-str in case gv_target points to dir_tree (possible in case of name-level-$order).
			 * This is similar to how we fix gv_currkey for a successful name-level-$order operation (see op_gvorder.c).
			 * Do same in case gv_target points to cs_addrs->hasht_tree so we dont take the fast path in op_gvname
			 * when gv_target is clearly not GVT of a user-visible global.
			 */
			if ((gv_target == csa->dir_tree) GTMTRIG_ONLY(|| (gv_target == csa->hasht_tree)))
			{
				gv_currkey->end = 0;
				gv_currkey->base[0] = 0;
			}
		}
	}
	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
	if (DUMPABLE)
	{	/* Certain conditions we don't want to attempt to create the M-level ZSHOW dump.
		 * 1) Unix: If gtmMallocDepth > 0 indicating memory manager was active and could be reentered.
		 * 2) Unix: If we have a SIGBUS or SIGSEGV (could be likely to occur again
		 *    in the local variable code which would cause immediate shutdown with
		 *    no cleanup).
		 * 3) VMS: If we got an ACCVIO for the same as reason (2).
		 * Note that we will bypass checks 2 and 3 if GDL_ZSHOWDumpOnSignal debug flag is on
		 */
		SET_PROCESS_EXITING_TRUE;	/* So zshow doesn't push stuff on stack to "protect" it when
						 * we potentially already have a stack overflow */
		cancel_timer(0);		/* No interruptions now that we are dying */
		if (!repeat_error UNIX_ONLY(&& (0 == gtmMallocDepth)))
		{
			src_line_d.addr = src_line;	/* Setup entry point buffer for set_zstatus() */
			src_line_d.len = 0;
			SET_ZSTATUS(NULL);
		}
		/* Create the ZSHOW dump file if it can be created */
		create_fatal_error_zshow_dmp(SIGNAL, repeat_error);

		/* If we are about to core/exit on a stack over flow, only do the core part if a debug
		 * flag requests this behaviour. Otherwise, supress the core and just exit.
		 * 2006-03-07 se: If a stack overflow occurs on VMS, it has happened that the stack is no
		 * longer well formed so attempting to unwind it as it does in MUMPS_EXIT causes things
		 * to really become screwed up. For this reason, this niceness of avoiding a dump on a
		 * stack overflow on VMS is being disabled. The dump can be controlled wih set proc/dump
		 * (or not) as desired.
		 * 2008-01-29 (se): Added fatal MEMORY error so we no longer generate a core for it by
		 * default unless the DumpOnStackOFlow flag is turned on. Since this flag is not a user-exposed
		 * interface, I'm avoiding renaming it for now. Note the core avoidance applies to both UNIX
		 * and VMS since stack formation is not at issue in this sort of memory request.
		 * Finally note that in UNIX, ch_cond_core (called by DRIVECH macro which invoked this condition
		 * handler has likely already created the core and set the created_core flag which will prevent
		 * this process from creating another core for the same SIGNAL. We leave this code in here in
		 * case methods exist in the future for this module to be driven without invoking cond_core_ch
		 * first.
		 */
		if (!(GDL_DumpOnStackOFlow & gtmDebugLevel) &&
		    VMS_ONLY((int)ERR_VMSMEMORY == SIGNAL)
		    UNIX_ONLY(((int)ERR_STACKOFLOW == SIGNAL || (int)ERR_STACKOFLOW == arg
			       || (int)ERR_MEMORY == SIGNAL  || (int)ERR_MEMORY == arg)))
		{
#			ifdef VMS
			/* Inside this ifdef, we are definitely here because of ERR_VMSMEMORY. If the conditions
			 * of the above if change, revisit these assmuptions.
			 * For VMSMEMORY error, we have to send the message to the operator log and to the
			 * console ourselves because the MUMP_EXIT method of exiting on a fatal error does
			 * not preserve the substitution parameters for the message making it useless. After
			 * sending the message change the status code so we exit with something other than the
			 * duplicate message.
			 */
			assert(ERR_VMSMEMORY == SIGNAL);
			send_msg(VARLSTCNT(4) ERR_VMSMEMORY, 2, *(int **)(&sig->chf$is_sig_arg1 + 1),	/* BYPASSOK - send_msg */
				 *(int **)(&sig->chf$is_sig_arg1 + 2));
			gtm_putmsg(VARLSTCNT(4) ERR_VMSMEMORY, 2, *(int **)(&sig->chf$is_sig_arg1 + 1),	/* BYPASSOK - gtm_putmsg */
				   *(int **)(&sig->chf$is_sig_arg1 + 2));
			SIGNAL = ERR_GTMERREXIT;	/* Override reason for "stop" */
#			endif
			MUMPS_EXIT;	/* Do a clean exit rather than messy core exit */
		}
		gtm_dump();
		TERMINATE;
	}
#	ifdef GTM_TRIGGER
	assertpro(TPRESTART_STATE_NORMAL == tprestart_state);	/* Can't leave half-restarted transaction around - out of design */
#	endif
	if (active_lv)
	{
		if (!LV_IS_VAL_DEFINED(active_lv) && !LV_HAS_CHILD(active_lv))
			op_kill(active_lv);
		active_lv = (lv_val *)0;
	}
	/*
	 * If error is at least severity "WARNING", do some cleanups. Note: crit is no longer unconditionally
	 * released here. It is now released if NOT in TP (any retry) or if in TP but NOT in the final retry.
	 * But we still cleanup the replication instance file lock and the replication crit lock. We do this
	 * because when starting the final retry GTM only holds the grab_crit locks but not the grab_lock or
	 * ftok_sem_lock locks. The grab_lock is done only at commit time (at which point there cannot be any
	 * programmatic runtime errors possible). Errors inside jnlpool_init (which cause the ftok-sem-lock
	 * to be held) can never be programmatically caused.
	 *
	 * On the other hand, while running in the final retry holding crit, a program can cause an arbitrary
	 * error (e.g. 1/0 error or similar). Hence the distinction. The reasoning is that any of these could
	 * potentially take an arbitrary amount of time and we don't want to be holding critical locks while
	 * doing these. But one can argue that the error trap is nothing but an extension of the transaction
	 * and that if GT.M is fine executing arbitrary application M code in the final retry holding crit,
	 * it should be fine doing the same for the error trap M code as well. So the entire point of not
	 * releasing crit is to avoid indefinite TP retries due to runtime errors. But this means the error
	 * trap should be well coded (i.e. not have any long running commands etc.) to avoid crit hangs
	 * and/or deadlocks.
	 */
	if ((SUCCESS != SEVERITY) && (INFO != SEVERITY))
	{
		ENABLE_AST;
		if ((0 == dollar_tlevel) || (CDB_STAGNATE > t_tries))
		{	/* Only release crit if we are NOT in TP *or* if we are in TP, we aren't in final retry */
			for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
			{
				for (reg_local = addr_ptr->regions, reg_top = reg_local + addr_ptr->n_regions;
				     reg_local < reg_top; reg_local++)
				{
					if (reg_local->open && !reg_local->was_open)
					{
						csa = (sgmnt_addrs *)&FILE_INFO(reg_local)->s_addrs;
						if (csa && csa->now_crit)
						{
							assert(!csa->hold_onto_crit UNIX_ONLY(|| jgbl.onlnrlbk));
							if (csa->hold_onto_crit)
								csa->hold_onto_crit = FALSE; /* Fix it in pro */
							rel_crit(reg_local);
						}
					}
				}
			}
		}
#		ifdef UNIX
		/* Release FTOK lock on the replication instance file if holding it (possible if error in jnlpool_init) */
		assert((NULL == jnlpool.jnlpool_dummy_reg) || jnlpool.jnlpool_dummy_reg->open || !pool_init);
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
#		endif
		/* Release crit lock on journal pool if holding it */
		if (pool_init) /* atleast one region replicated and we have done jnlpool init */
		{
			csa = (sgmnt_addrs *)&FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;
			if (csa && csa->now_crit)
				rel_lock(jnlpool.jnlpool_dummy_reg);
		}
		TREF(in_op_fnnext) = FALSE;			/* in case we were in $NEXT */
		/* Global values that may need cleanups */
		if (INTRPT_OK_TO_INTERRUPT != intrpt_ok_state)
			ENABLE_INTERRUPTS(intrpt_ok_state);	/* If interrupts were deferred, re-enable them now */
	}
#	ifdef GTM_TRIGGER
	/* At this point, we are past the point where the frame pointer is allowed to be resting on a trigger frame
	 * (this is possible in a TPRETRY situation where gtm_trigger must return to gtm_trigger() signaling a
	 * restart is necessary). If we are on a trigger base frame, unwind it so the error is recognized in
	 * the invoker's frame.
	 */
	if (SFT_TRIGR & frame_pointer->type)
	{
		/* Better be an error in here info or success messages want to continue, not be unwound but
		 * we cannot go past this point in a trigger frame or the frame_pointer back reference below
		 * will fail.
		 */
		assert((SUCCESS != SEVERITY) && (INFO != SEVERITY));
		/* These outofband conditions depend on saving the current stack frame info in restart_pc which
		 * is of course no longer valid once the frame is unrolled so they must be avoided. At the time
		 * of this writing, there are no conditions that these should validly be called in this
		 * situation so this check is more for the future.
		 */
		assert(((int)ERR_CTRLY != SIGNAL) && ((int)ERR_CTRLC != SIGNAL) && ((int)ERR_CTRAP != SIGNAL)
		       && ((int)ERR_JOBINTRRQST != SIGNAL) && ((int)ERR_JOBINTRRETHROW != SIGNAL));
		gtm_trigger_fini(TRUE, FALSE);
		DBGEHND((stderr, "mdb_condition_handler: Current trigger frame unwound so error is thrown"
			 " on trigger invoker's frame instead.\n"));
	}
#	endif
 	err_dev = active_device;
	active_device = (io_desc *)NULL;
	dm_action = (frame_pointer->old_frame_pointer->type & SFT_DM)
		|| (TREF(compile_time) && (frame_pointer->type & SFT_DM));
	/* The errors are said to be transcendental when they occur during compilation/execution
	 * of the error trap ({z,e}trap, device exception) or $zinterrupt. The errors in other
	 * indirect code frames (zbreak, zstep, xecute etc.) aren't defined to be trancendental
	 * and will be treated  as if they occured in a regular code frame.
	 */
	trans_action = proc_act_type || (frame_pointer->type & SFT_ZTRAP) || (frame_pointer->type & SFT_DEV_ACT);
	src_line_d.addr = src_line;
	src_line_d.len = 0;
	flush_pio();
	if ((int)ERR_CTRLY == SIGNAL)
	{
		outofband_clear();
		/* Verify not indirect or that context is unchanged before reset context */
		assert(NULL != restart_pc);
		assert((!(SFF_INDCE & frame_pointer->flags)) || (restart_ctxt == frame_pointer->ctxt));
		DBGEHND((stderr, "mdb_condition_handler(1): Resetting frame 0x"lvaddr" mpc/context with restart_pc/ctxt "
			 "0x"lvaddr"/0x"lvaddr" - frame has type 0x%04lx\n", frame_pointer, restart_pc, restart_ctxt,
			 frame_pointer->type));
		frame_pointer->mpc = restart_pc;
		frame_pointer->ctxt = restart_ctxt;
		MUM_TSTART;
	} else  if ((int)ERR_CTRLC == SIGNAL)
	{
		outofband_clear();
		if (!trans_action && !dm_action)
		{	/* Verify not indirect or that context is unchanged before reset context */
			assert(NULL != restart_pc);
			assert((!(SFF_INDCE & frame_pointer->flags)) || (restart_ctxt == frame_pointer->ctxt));
			DBGEHND((stderr, "mdb_condition_handler(2): Resetting frame 0x"lvaddr" mpc/context with restart_pc/ctxt "
				 "0x"lvaddr"/0x"lvaddr" - frame has type 0x%04lx\n", frame_pointer, restart_pc, restart_ctxt,
				 frame_pointer->type));
			frame_pointer->mpc = restart_pc;
			frame_pointer->ctxt = restart_ctxt;
			frame_pointer->flags &= SFF_IMPLTSTART_CALLD_OFF;	/* Frame enterable now with mpc reset */
			GTMTRIG_ONLY(
				DBGTRIGR((stderr, "mdb_condition_handler: turning off SFF_IMPLTSTART_CALLD_OFF (1) in frame "
					  "0x"lvaddr"\n", frame_pointer)));
			if (!(frame_pointer->type & SFT_DM))
				dm_setup();
		} else  if (frame_pointer->type & SFT_DM)
		{
			frame_pointer->ctxt = GTM_CONTEXT(call_dm);
			frame_pointer->mpc = CODE_ADDRESS(call_dm);
			frame_pointer->flags &= SFF_IMPLTSTART_CALLD_OFF;	/* Frame enterable now with mpc reset */
			GTMTRIG_ONLY(
				DBGTRIGR((stderr, "mdb_condition_handler: turning off SFF_IMPLTSTART_CALLD_OFF (1) in frame "
					  "0x"lvaddr"\n", frame_pointer)));
		} else
		{
			/* Do cleanup on indirect frames prior to reset */
			IF_INDR_FRAME_CLEANUP_CACHE_ENTRY_AND_UNMARK(frame_pointer);
			frame_pointer->ctxt = GTM_CONTEXT(pseudo_ret);
			frame_pointer->mpc = CODE_ADDRESS(pseudo_ret);
			frame_pointer->flags &= SFF_IMPLTSTART_CALLD_OFF;	/* Frame enterable now with mpc reset */
			GTMTRIG_ONLY(
				DBGTRIGR((stderr, "mdb_condition_handler: turning off SFF_IMPLTSTART_CALLD_OFF (1) in frame "
					  "0x"lvaddr"\n", frame_pointer)));
		}
		PRN_ERROR;
		if (io_curr_device.out != io_std_device.out)
			dec_err(VARLSTCNT(4) ERR_NOTPRINCIO, 2, io_curr_device.out->trans_name->len,
				io_curr_device.out->trans_name->dollar_io);
		MUM_TSTART;
	} else if ((int)ERR_CTRAP == SIGNAL)
	{
		if (!repeat_error)
			/* This has already been done if we are re-throwing the error */
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
				dollar_ecode.error_last_b_line = SET_ZSTATUS(NULL);
			}
			if (sp_base != rts_stringpool.base)
			{
				rts_stringpool = stringpool;	/* update rts_stringpool */
				stringpool = indr_stringpool;	/* change back */
			}
			assert(NULL != dollar_ecode.error_last_b_line);
			/* Only (re)set restart_pc if we are in the original frame. This is needed to restart execution at the
			 * beginning of the line but only happens when $ZTRAP is in effect. If $ETRAP is in control, control
			 * naturally returns to the caller unless the $ECODE value is not set. In that case, the CTRAP error
			 * will be rethrown one level down as $ETRAP normally does. Note in case of a rethrow, we avoid resetting
			 * mpc/ctxt as those values are only appropriate for the level in which they were saved.
			 */
			if (!repeat_error && ((0 != dollar_ztrap.str.len) || ztrap_explicit_null))
			{	/* Verify not indirect or that context is unchanged before reset context */
				assert(NULL != restart_pc);
				assert((!(SFF_INDCE & frame_pointer->flags)) || (restart_ctxt == frame_pointer->ctxt));
				DBGEHND((stderr, "mdb_condition_handler(3): Resetting frame 0x"lvaddr" mpc/context with restart_pc/"
					 "ctxt 0x"lvaddr"/0x"lvaddr" - frame has type 0x%04lx\n", frame_pointer, restart_pc,
					 restart_ctxt, frame_pointer->type));
				frame_pointer->mpc = restart_pc;
				frame_pointer->ctxt = restart_ctxt;
			}
			err_act = NULL;
			dollar_ecode.error_last_ecode = SIGNAL;
			if (std_dev_outbnd && io_std_device.in && (tt == io_std_device.in->type)
			    && io_std_device.in->error_handler.len)
			{
				proc_act_type = SFT_DEV_ACT;
				err_act = &io_std_device.in->error_handler;
			} else if (!std_dev_outbnd && err_dev && (tt == err_dev->type) && err_dev->error_handler.len)
			{
				proc_act_type = SFT_DEV_ACT;
				err_act = &err_dev->error_handler;
			} else if (NULL != error_frame)
			{	/* a primary error occurred already. irrespective of whether ZTRAP or ETRAP is active now,
				 * we need to consider this as a nested error and trigger nested error processing.
				 */
				goerrorframe();	/* unwind back to error_frame */
				proc_act_type = 0;
			} else if ((0 != dollar_etrap.str.len) || (0 != dollar_ztrap.str.len))
			{
				assert(!ztrap_explicit_null);
				proc_act_type = SFT_ZTRAP;
				err_act = (0 != dollar_etrap.str.len) ? &dollar_etrap.str : &dollar_ztrap.str;
			} else
			{	/* Either $ETRAP is empty-string or ztrap_explicit_null is set
				 *
				 * If ztrap_explicit_null is FALSE
				 *  - Use empty-string $ETRAP for error-handling
				 *
				 * If ztrap_explicit_null is TRUE
				 *  - unwind as many frames as possible until we see a frame where ztrap_explicit_null is
				 *    FALSE and $ZTRAP is not NULL (i.e. we find a $ETRAP handler). In that frame, use $ETRAP
				 *    for error-handling. If no such frame is found, exit after printing the error.
				 */
				etrap_handling = TRUE;
				if (ztrap_explicit_null)
				{
					assert(0 == dollar_etrap.str.len);
					for (level = dollar_zlevel() - 1; level > 0; level--)
					{
						GOLEVEL(level, FALSE);
						assert(level == dollar_zlevel());
						if (ETRAP_IN_EFFECT)
							break;
					}
					if (0 >= level)
					{
						assert(0 == level);
						etrap_handling = FALSE;
						DBGEHND((stderr, "mdb_condition_handler: Unwound to stack start - exiting\n"));
					}
					/* Note that trans_code will set error_frame appropriately for this condition */
				}
				if (SFF_CI & frame_pointer->flags)
				{ 	/* Unhandled errors from called-in routines should return to gtm_ci() with error status */
					mumps_status = SIGNAL;
					MUM_TSTART_FRAME_CHECK;
					MUM_TSTART;
				} else if (etrap_handling)
				{
					proc_act_type = SFT_ZTRAP;
					err_act = &dollar_etrap.str;
				} else
				{
					PRN_ERROR;
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOEXCNOZTRAP);
				}
			}
			if (clean_mum_tstart())
			{
				MUM_TSTART_FRAME_CHECK;
				MUM_TSTART;
			}
		} else  if (frame_pointer->type & SFT_DM)
		{
			frame_pointer->ctxt = GTM_CONTEXT(call_dm);
			frame_pointer->mpc = CODE_ADDRESS(call_dm);
			frame_pointer->flags &= SFF_IMPLTSTART_CALLD_OFF;	/* Frame enterable now with mpc reset */
			GTMTRIG_ONLY(DBGTRIGR((stderr, "mdb_condition_handler: turning off SFF_IMPLTSTART_CALLD (2) in frame 0x"
					       lvaddr"\n", frame_pointer)));
		} else
		{
			/* Do cleanup on indirect frames prior to reset */
			IF_INDR_FRAME_CLEANUP_CACHE_ENTRY_AND_UNMARK(frame_pointer);
			frame_pointer->ctxt = GTM_CONTEXT(pseudo_ret);
			frame_pointer->mpc = CODE_ADDRESS(pseudo_ret);
			frame_pointer->flags &= SFF_IMPLTSTART_CALLD_OFF;	/* Frame enterable now with mpc reset */
			GTMTRIG_ONLY(DBGTRIGR((stderr, "mdb_condition_handler: turning off SFF_IMPLTSTART_CALLD (3) in frame 0x"
					       lvaddr"\n", frame_pointer)));
		}
		PRN_ERROR;
		if (io_curr_device.out != io_std_device.out)
		{
			dec_err(VARLSTCNT(4) ERR_NOTPRINCIO, 2, io_curr_device.out->trans_name->len,
				io_curr_device.out->trans_name->dollar_io);
		}
		MUM_TSTART_FRAME_CHECK;
		MUM_TSTART;
	} else  if ((int)ERR_JOBINTRRQST == SIGNAL)
	{	/* Verify not indirect or that context is unchanged before reset context */
		assert(NULL != restart_pc);
		assert((!(SFF_INDCE & frame_pointer->flags)) || (restart_ctxt == frame_pointer->ctxt));
		DBGEHND((stderr, "mdb_condition_handler(4): Resetting frame 0x"lvaddr" mpc/context with restart_pc/ctxt "
			 "0x"lvaddr"/0x"lvaddr" - frame has type 0x%04lx\n", frame_pointer, restart_pc, restart_ctxt,
			 frame_pointer->type));
		frame_pointer->mpc = restart_pc;
		frame_pointer->ctxt = restart_ctxt;
		assert(!dollar_zininterrupt);
		dollar_zininterrupt = TRUE;	/* Note done before outofband is cleared to prevent nesting */
		outofband_clear();
		proc_act_type = SFT_ZINTR | SFT_COUNT;	/* trans_code will invoke jobinterrupt_process for us */
		MUM_TSTART;
	} else  if ((int)ERR_JOBINTRRETHROW == SIGNAL)
	{ 	/* Job interrupt is rethrown from TC/TRO */
		assert(!dollar_zininterrupt);
		dollar_zininterrupt = TRUE;
		proc_act_type = SFT_ZINTR | SFT_COUNT; /* trans_code will invoke jobinterrupt_process for us */
		MUM_TSTART;
	} else  if ((int)ERR_STACKCRIT == SIGNAL)
	{
		assert(msp > stacktop);
		assert(stackwarn > stacktop);
		cp = stackwarn;
		stackwarn = stacktop;
		push_stck(cp, 0, (void **)&stackwarn, MVST_STCK_SP);
	}
	if (!repeat_error)
		dollar_ecode.error_last_b_line = NULL;
	/* Error from direct mode actions does not set $ZSTATUS and is not restarted (dollar_ecode.error_last_b_line = NULL);
	 * Error from transcendental code does set $ZSTATUS but does not restart the line.
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
			dollar_ecode.error_last_b_line = SET_ZSTATUS(&context);
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
	/* Error from direct mode actions or "transcendental" code does not invoke MUMPS error handling routines */
	if (!dm_action && !trans_action)
	{
		DBGEHND((stderr, "mdb_condition_handler: Handler to dispatch selection checks\n"));
		err_act = NULL;
		dollar_ecode.error_last_ecode = SIGNAL;
		reset_mpc = FALSE;
		if (err_dev && err_dev->error_handler.len && ((int)ERR_TPTIMEOUT != SIGNAL))
		{
			proc_act_type = SFT_DEV_ACT;
			err_act = &err_dev->error_handler;
			reserve_sock_dev = TRUE;
			/* Reset mpc to beginning of the current line (to retry after processing the IO exception handler) */
			reset_mpc = TRUE;
			DBGEHND((stderr, "mdb_condition_handler: dispatching device error handler [%.*s]\n", err_act->len,
				 err_act->addr));
		} else if (NULL != error_frame)
		{	/* A primary error occurred already. irrespective of whether ZTRAP or ETRAP is active now, we need to
			 * consider this as a nested error and trigger nested error processing.
			 */
			goerrorframe();	/* unwind upto error_frame */
			proc_act_type = 0;
			DBGEHND((stderr, "mdb_condition_handler: Have unwound to error frame via goerrorframe() and am "
				 "re-dispatching error frame\n"));
			MUM_TSTART_FRAME_CHECK;
			MUM_TSTART;	/* unwind the current C-stack and restart executing from the top of the current M-stack */
		} else if ((0 != dollar_etrap.str.len) || (0 != dollar_ztrap.str.len))
		{
			assert(!ztrap_explicit_null);
			proc_act_type = SFT_ZTRAP;
			err_act = (0 != dollar_etrap.str.len) ? &dollar_etrap.str : &dollar_ztrap.str;
			DBGEHND((stderr, "mdb_condition_handler: Dispatching %s error handler [%.*s]\n",
				 (0 != dollar_etrap.str.len) ? "$ETRAP" : "$ZTRAP", err_act->len, err_act->addr));
			/* Reset mpc to beginning of the current line (to retry after invoking $ZTRAP) */
			if (0 != dollar_ztrap.str.len)
				reset_mpc = TRUE;
		} else
		{	/* Either $ETRAP is empty string or ztrap_explicit_null is set.
			 *
			 * If ztrap_explicit_null is FALSE
			 *   - Use empty-string $ETRAP for error-handling
			 *
			 * If ztrap_explicit_null is TRUE
			 *   - Unwind as many frames as possible until we see a frame where ztrap_explicit_null is FALSE and
			 *     $ZTRAP is *not* NULL (i.e. we found an $ETRAP). In that frame, use $ETRAP for error-handling.
			 *     If no such frame is found, exit after printing the error.
			 */
			etrap_handling = TRUE;
			if (ztrap_explicit_null)
			{
				GTMTRIG_ONLY(assert(0 == gtm_trigger_depth));	/* Should never happen in a trigger */
				DBGEHND((stderr, "mdb_condition_handler: ztrap_explicit_null set - unwinding till find handler\n"));
				assert(0 == dollar_etrap.str.len);
				for (level = dollar_zlevel() - 1; level > 0; level--)
				{
					GOLEVEL(level, FALSE);
					assert(level == dollar_zlevel());
					if (ETRAP_IN_EFFECT)
						/* Break if found an $ETRAP covered frame */
						break;
				}
				if (0 >= level)
				{
					assert(0 == level);
					etrap_handling = FALSE;
					DBGEHND((stderr, "mdb_condition_handler: Unwound to stack start - exiting\n"));
				}
				/* note that trans_code will set error_frame appropriately for this condition */
			}
			if (SFF_CI & frame_pointer->flags)
			{ 	/* Unhandled errors from called-in routines should return to gtm_ci() with error status */
				mumps_status = SIGNAL;
				DBGEHND((stderr, "mdb_condition_handler: Call in base frame found - returnning to callins\n"));
				MUM_TSTART_FRAME_CHECK;
				MUM_TSTART;
			} else if (etrap_handling)
			{
				proc_act_type = SFT_ZTRAP;
				err_act = &dollar_etrap.str;
				DBGEHND((stderr, "mdb_condition_handler: $ETRAP handler being dispatched [%.*s]\n", err_act->len,
					 err_act->addr));
			}
		}
		/* if the err_act points to the address err_dev->errro_handler, we cannot destroy socket here */
		if (err_dev && (gtmsocket == err_dev->type) && err_dev->newly_created && !reserve_sock_dev)
		{
			assert(err_dev->state != dev_open);
			iosocket_destroy(err_dev);
			err_dev = NULL;
		}
		if (reset_mpc)
		{	/* Reset the mpc such that
			 *   (a) If the current frame is a counted frame, the error line is retried after the error is handled,
			 *   (b) If the current frame is "transcendental" code, set frame to return.
			 *
			 * If we are in $ZYERROR, we don't care about restarting the line that errored since we will
			 * unwind all frames upto and including zyerr_frame.
			 *
			 * If this is a rethrown error (ERR_REPEATERROR) from a child frame, do NOT reset mpc of the current
			 * frame in that case. We do NOT want to retry the current line (after the error has been
			 * processed) because the error did not occur in this line and therefore re-executing the same
			 * line could cause undesirable effects at the M-user level. We will resume normal execution
			 * once the error is handled. Not that it matters, but note that in the case of a rethrown error
			 * (repeat_error is TRUE), we would NOT have noted down dollar_ecode.error_last_b_line so cannot
			 * use that to reset mpc anyways.
			 */
			if ((NULL == zyerr_frame) && !repeat_error)
			{
				DBGEHND((stderr, "mdb_condition_handler: reset_mpc triggered\n"));
				for (fp = frame_pointer; fp; fp = fp->old_frame_pointer)
				{	/* See if this is a $ZINTERRUPT frame. If yes, we want to restart *this* line
					 * at the beginning. Since it is always an indirect frame, we can use the context
					 * pointer to start over. $ETRAP does things somewhat differently in that the current
					 * frame is always returned from.
					 */
					if (SFT_ZINTR & fp->type)
					{
						assert(SFF_INDCE & fp->flags);
						fp->mpc = fp->ctxt;
						fp->flags &= SFF_IMPLTSTART_CALLD_OFF;	/* Frame enterable now with mpc reset */
						GTMTRIG_ONLY(
							DBGTRIGR((stderr, "mdb_condition_handler: turning off SFF_IMPLTSTART_CALLD"
								  " (4) in frame 0x"lvaddr"\n", frame_pointer)));
						break;
					}
					/* Do cleanup on indirect frames prior to reset */
					IF_INDR_FRAME_CLEANUP_CACHE_ENTRY_AND_UNMARK(fp);
					/* mpc points to PTEXT */
					if (ADDR_IN_CODE(fp->mpc, fp->rvector))
					{	/* GT.M specific error trapping retries the line with the error */
						fp->mpc = dollar_ecode.error_last_b_line;
						fp->ctxt = context;
						fp->flags &= SFF_IMPLTSTART_CALLD_OFF;	/* Frame enterable now with mpc reset */
						GTMTRIG_ONLY(
							DBGTRIGR((stderr, "mdb_condition_handler: turning off SFF_IMPLTSTART_CALLD"
								  " (5) in frame 0x"lvaddr"\n", frame_pointer)));
						break;
					} else
					{
						fp->ctxt = GTM_CONTEXT(pseudo_ret);
						fp->mpc = CODE_ADDRESS(pseudo_ret);
						fp->flags &= SFF_IMPLTSTART_CALLD_OFF;	/* Frame enterable now with mpc reset */
						GTMTRIG_ONLY(
							DBGTRIGR((stderr, "mdb_condition_handler: turning off SFF_IMPLTSTART_CALLD"
							  " (6) in frame 0x"lvaddr"\n", frame_pointer)));
					}
				}
			}
		}
		if (clean_mum_tstart())
		{
			if (err_dev)
			{
#				ifdef UNIX
				/* On z/OS, if opening a fifo which is not read only we need to fix the err_dev type to rm */
#				ifdef __MVS__
				if ((dev_open != err_dev->state) && (ff == err_dev->type))
				{
					assert(NULL != err_dev->pair.out);
					if (rm == err_dev->pair.out->type)
					{
						/* Have to massage the device so remove_rms will cleanup the partially
						 * created fifo.  Refer to io_open_try.c for creation of split fifo device.
						 */
						err_dev->newly_created = 1;
						err_dev->type = rm;
						err_dev->dev_sp = err_dev->pair.out->dev_sp;
						err_dev->pair.out->dev_sp = NULL;
					}
				}
#				endif
				if ((dev_open != err_dev->state) && (rm == err_dev->type))
				{
					gtm_err_dev = err_dev;
					/* structures pointed to by err_dev were freed so make sure it's not used again */
					err_dev = NULL;
				}
#				endif
				if (err_dev && (gtmsocket == err_dev->type) && err_dev->newly_created)
				{
					assert(err_dev->state != dev_open);
					assert(reserve_sock_dev);
					gtm_err_dev = err_dev;
					err_dev = NULL;
				}
			}
			MUM_TSTART_FRAME_CHECK;
			MUM_TSTART;
		} else
		{
			/* err_act is null, we can remove the socket device */
			if (err_dev && (gtmsocket == err_dev->type) && err_dev->newly_created && !reserve_sock_dev)
			{
				assert(err_act == NULL);
				assert(err_dev->state != dev_open);
				iosocket_destroy(err_dev);
				err_dev = NULL;
			}
			DBGEHND((stderr, "mdb_condition_handler: clean_mum_tstart returned FALSE\n"));
		}
	} else
	{
		DBGEHND((stderr, "mdb_condition_handler: Transient or direct mode frame -- bypassing handler dispatch\n"));
		if (err_dev)
		{
#			ifdef UNIX
			/* Executed from the direct mode so do the rms check and cleanup if necessary. On z/OS, if opening a fifo
			 * which is not read only we need to fix the type for the err_dev to rm.
			 */
#			ifdef __MVS__
			if ((dev_open != err_dev->state) && (ff == err_dev->type))
			{
				assert(NULL != err_dev->pair.out);
				if (rm == err_dev->pair.out->type)
				{
					/* Have to massage the device so remove_rms will cleanup the partially created fifo */
					err_dev->newly_created = 1;
					err_dev->type = rm;
					err_dev->dev_sp = err_dev->pair.out->dev_sp;
					err_dev->pair.out->dev_sp = NULL;
				}
			}
#			endif
			if ((dev_open != err_dev->state) && (rm == err_dev->type))
			{
				remove_rms(err_dev);
				err_dev = NULL;
			}
#			endif
			if (err_dev && (gtmsocket == err_dev->type) && err_dev->newly_created)
			{
				assert(err_dev->state != dev_open);
				iosocket_destroy(err_dev);
				err_dev = NULL;
			}
		}
	}
	if (((SFT_ZINTR | SFT_COUNT) != proc_act_type) || (0 == dollar_ecode.error_last_b_line))
	{	/* No user console error for $zinterrupt compile problems and if not direct mode. Accomplish
		 * this by bypassing the code inside this if which *will* be executed for most cases
		 */
		DBGEHND((stderr, "mdb_condition_handler: Printing error status\n"));
		/* If we have a transcendental frame, we won't have driven an error handler but neither do we want to
		 * push this error out at this point. The call to trans_code_cleanup() below will null this frame and
		 * any other up to the next counted frame out by changing their mpc to pseudo_ret and will rethrow the
		 * error we have here so there is no need to push this error out here for anything but direct mode.
		 */
		UNIX_ONLY(if (dm_action))
		{
			PRN_ERROR;
			if (TREF(compile_time) && (((int)ERR_LABELMISSING) != SIGNAL))
				show_source_line(TRUE);
		}
	}
	/* Slight divergence in how we handle otherwise unhandled errors on UNIX and VMS. UNIX now has a strict no-unsolicited
	 * output from error messages policy unless dealing with a direct mode frame. This has a dependency on a robust
	 * error_return() routine which VMS does not presently have. Consequently the previous way of doing things still
	 * exists on VMS while on UNIX, a new cleaner (from an unsolicited console output) viewpoint is in place. Note that
	 * while it would be possible to extract out the common code between the two below versions, the result is nowhere
	 * near as clean looking. In this routine, clarity rules the roost.
	 */
#	ifdef UNIX
	if (trans_action || dm_action)
	{	/* If true transcendental, do trans_code_cleanup(). If our counted frame is
		 * masquerading as a transcendental frame, run jobinterrupt_process_cleanup().
		 */
		DBGEHND((stderr, "mdb_condition_handler: trans_code_cleanup() or jobinterrupt_process_cleanup being "
			 "dispatched\n"));
		if (!(SFT_ZINTR & proc_act_type))
			trans_code_cleanup();
		else
			jobinterrupt_process_cleanup();
		MUM_TSTART_FRAME_CHECK;
		MUM_TSTART;
	}
#	elif VMS	/* VMS only */
	if (!dm_action && !trans_action && (0 != src_line_d.len))
	{
		if (MSG_OUTPUT)
			dec_err(VARLSTCNT(4) ERR_RTSLOC, 2, src_line_d.len, src_line_d.addr);
	} else
	{
		if (trans_action || dm_action)
		{	/* If true transcendental, do trans_code_cleanup(). If our counted frame is
			 * masquerading as a transcendental frame, run jobinterrupt_process_cleanup().
			 */
			DBGEHND((stderr, "mdb_condition_handler: trans_code_cleanup() or jobinterrupt_process_cleanup being "
				 "dispatched\n"));
			if (!(SFT_ZINTR & proc_act_type))
				trans_code_cleanup();
			else
				jobinterrupt_process_cleanup();
			MUM_TSTART_FRAME_CHECK;
			MUM_TSTART;
		} else if (MSG_OUTPUT)
		{	/* If a message about the location is needed, it should be possible to pull the location
			 * out of the $STACK array. If it exists, use it instead.
			 */
			if ((NULL != dollar_stack.array) && (0 < dollar_stack.index))
			{	/* Error entry exists */
				src_line_d = dollar_stack.array[dollar_stack.index - 1].place_str;
				assert(src_line_d.len);
				assert(src_line_d.addr);
				dec_err(VARLSTCNT(4) ERR_RTSLOC, 2, src_line_d.len, src_line_d.addr);
			} else
				dec_err(VARLSTCNT(1) ERR_SRCLOCUNKNOWN);
		}
	}
#	else
#	 error "Unsupported platform"
#	endif
	DBGEHND((stderr, "mdb_condition_handler: Condition not handled -- defaulting to process exit\n"));
	MUMPS_EXIT;
}
