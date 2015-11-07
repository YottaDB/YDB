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

#include <sys/types.h>
#include <signal.h>
#include "gtm_unistd.h"
#include "gtm_string.h"
#include "gtm_stdio.h"

#include "error.h"
#include "io_params.h"
#include "op.h"
#include "io.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "jobexam_process.h"
#ifdef UNIX
#  include "jobexam_signal_handler.h"
#endif
#include "send_msg.h"
#include "callg.h"
#include "zshow.h"
#include "util.h"
#include "mv_stent.h"

#define DEFAULT_DUMP_FILENAME "GTM_JOBEXAM.ZSHOW_DMP"
#define NOCONCEAL_OPTION "NO_CONCEAL"

static readonly mval empty_str_mval = DEFINE_MVAL_LITERAL(MV_STR, 0, 0, 0, 0, 0, 0);
static readonly mval no_conceal_op  = DEFINE_MVAL_LITERAL(MV_STR, 0, 0, SIZEOF(NOCONCEAL_OPTION) - 1,
							 NOCONCEAL_OPTION, 0, 0);
static unsigned char dumpable_error_dump_file_parms[2] = {iop_newversion, iop_eol};
static unsigned char dumpable_error_dump_file_noparms[1] = {iop_eol};
static unsigned int  jobexam_counter;

GBLREF uint4		process_id;
GBLREF io_pair		io_std_device, io_curr_device;
GBLREF mv_stent		*mv_chain;
GBLREF unsigned char    *msp, *stackwarn, *stacktop;
GBLREF boolean_t        created_core;
UNIX_ONLY(GBLREF sigset_t blockalrm;)
DEBUG_ONLY(GBLREF boolean_t ok_to_UNWIND_in_exit_handling;)

error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_JOBEXAMDONE);
error_def(ERR_JOBEXAMFAIL);
error_def(ERR_MEMORY);
error_def(ERR_OUTOFSPACE);
error_def(ERR_STACKCRIT);
error_def(ERR_STACKOFLOW);
error_def(ERR_VMSMEMORY);

void jobexam_process(mval *dump_file_name, mval *dump_file_spec)
{
	mval			*input_dump_file_name;
	io_pair			dev_in_use;
	mv_stent		*new_mv_stent;
	boolean_t		saved_mv_stent;
	char			saved_util_outbuff[OUT_BUFF_SIZE];
	int			saved_util_outbuff_len;
#	ifdef UNIX
	struct sigaction	new_action, prev_action;
	sigset_t		savemask;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* If the input file name is the result of an expression, it is likely being held in the
	 *  same temporary as the output file spec. We can tell if this is true by comparing the
	 *  address of the input and output mvals. If they are the same, make a copy of the input
	 *  filespec in a garbage collection safe mval prior to initializing the output mval
	 *  (which in this case would clear the input mval as well if it had not just been saved).
	 */
	if (dump_file_name == dump_file_spec)
	{	/* Make saved copy of input mval */
		PUSH_MV_STENT(MVST_MVAL);
		new_mv_stent = mv_chain;
		input_dump_file_name = &mv_chain->mv_st_cont.mvs_mval;
		*input_dump_file_name = *dump_file_name;
		saved_mv_stent = TRUE;
	} else
	{	/* Just use input mval as-is */
		input_dump_file_name = dump_file_name;
		saved_mv_stent = FALSE;
	}

#	ifdef UNIX
	/* Block out timer calls that might trigger processing that could fail. We especially want to prevent
	 * nesting of signal handlers since the longjump() function used by the UNWIND macro is undefined on
	 * Tru64 when signal handlers are nested.
	 */
	sigprocmask(SIG_BLOCK, &blockalrm, &savemask);

	/* Setup new signal handler to just drive condition handler which will do the right thing */
	memset(&new_action, 0, SIZEOF(new_action));
	sigemptyset(&new_action.sa_mask);
	new_action.sa_flags = SA_SIGINFO;
#	ifdef __sparc
	new_action.sa_handler = jobexam_signal_handler;
#	else
	new_action.sa_sigaction = jobexam_signal_handler;
#	endif
	sigaction(SIGBUS, &new_action, &prev_action);
	sigaction(SIGSEGV, &new_action, 0);
#	endif
	*dump_file_spec = empty_str_mval;
	dev_in_use = io_curr_device;		/* Save current IO device */
	/* Save text in util_outbuff which can be detrimentally overwritten by ZSHOW.
	 * NOTE: The following code needs to be eventually moved to jobinterrupt_process.c and replaced with
	 * SAVE/RESTORE_UTIL_OUT_BUFFER macros, as follows:
	 *
	 * 	char 			*save_util_outptr;
	 *	va_list			save_last_va_list_ptr;
	 *	boolean_t		util_copy_saved = FALSE;
	 *	DCL_THREADGBL_ACCESS;
	 *
	 * 	SETUP_THREADGBL_ACCESS;
	 *	...
	 * 	SAVE_UTIL_OUT_BUFFER(save_util_outptr, save_last_va_list_ptr, util_copy_saved);
	 *	...
	 *	RESTORE_UTIL_OUT_BUFFER(save_util_outptr, save_last_va_list_ptr, util_copy_saved);
	 */
	saved_util_outbuff_len = 0;
	if (NULL == TREF(util_outptr))
		TREF(util_outptr) = TREF(util_outbuff_ptr);
	if (0 != (saved_util_outbuff_len = (int)(TREF(util_outptr) - TREF(util_outbuff_ptr))))	/* Caution -- assignment */
	{
		assert(0 <= saved_util_outbuff_len);
		assert(saved_util_outbuff_len <= SIZEOF(saved_util_outbuff));
		memcpy(saved_util_outbuff, TREF(util_outbuff_ptr), saved_util_outbuff_len);
	}
	jobexam_dump(input_dump_file_name, dump_file_spec);
	/* If any errors occur in job_exam_dump, the condition handler will unwind the stack to this point and return.  */
	if (0 != saved_util_outbuff_len)
	{	/* Restore util_outbuff values */
		memcpy(TREF(util_outbuff_ptr), saved_util_outbuff, saved_util_outbuff_len);
		TREF(util_outptr) = TREF(util_outbuff_ptr) + saved_util_outbuff_len;
	}
	io_curr_device = dev_in_use;		/* Restore IO device */
	/* If we saved an mval on our stack, we need to pop it off. If there was an error while doing the
	 * jobexam dump, zshow may have left some other mv_stent entries on the stack. Pop them all off with
	 * just a regular POP_MV_STENT macro rather than unw_mv_ent() call because the mv_stent entries
	 * created in zshow_output reference automatic storage that cannot be referenced at this stack
	 * level without potential (C) stack corruption.
	 */
	if (saved_mv_stent)
	{
		assertpro(mv_chain <= new_mv_stent);	/* This violates our assumptions that the mv_stent we pushed onto the
							 * stack should still be there */
		while (mv_chain <= new_mv_stent)
		{
			POP_MV_STENT();
		}
	}
#	ifdef UNIX
	/* Restore the signal handlers how they were */
	sigaction(SIGBUS, &prev_action, 0);
	sigaction(SIGSEGV, &prev_action, 0);
	/* Let the timers pop again.. */
	sigprocmask(SIG_SETMASK, &savemask, NULL);
#	endif
}

/* This routine is broken out as another ep so we can do cleanup processing in jobexam_process if
   we trigger the condition handler and unwind.
*/
void jobexam_dump(mval *dump_filename_arg, mval *dump_file_spec)
{
	unsigned char		dump_file_name[50], *dump_file_name_ptr;
	mval			def_file_name, parms, zshowall;

	ESTABLISH(jobexam_dump_ch);

	++jobexam_counter;
	/* Setup default filename/type to use for the parse. Append processid and a counter. */
	MEMCPY_LIT(dump_file_name, DEFAULT_DUMP_FILENAME);
	dump_file_name_ptr = dump_file_name + SIZEOF(DEFAULT_DUMP_FILENAME) - 1;
	*dump_file_name_ptr++ = '_';
	dump_file_name_ptr = i2asc(dump_file_name_ptr, process_id);
	*dump_file_name_ptr++ = '_';
	dump_file_name_ptr = i2asc(dump_file_name_ptr, jobexam_counter);
	def_file_name.mvtype = MV_STR;
	def_file_name.str.addr = (char *)dump_file_name;
	def_file_name.str.len = INTCAST(dump_file_name_ptr - dump_file_name);
	/* Call $ZPARSE processing to fill in any blanks, expand concealed logicals, etc. It is the callers
	 * responsibility to make sure garbage collection knows about the value in the returned filespec.
	 */
	op_fnzparse(dump_filename_arg, &empty_str_mval, &def_file_name, &empty_str_mval, &no_conceal_op, dump_file_spec);
	/* Parms of file to be created (newversion) */
	parms.mvtype = MV_STR;
	parms.str.addr = (char *)dumpable_error_dump_file_parms;
	parms.str.len = SIZEOF(dumpable_error_dump_file_parms);
	/* Open, use, and zshow into new file, then close and reset current io device */
	op_open(dump_file_spec, &parms, 0, 0);
	op_use(dump_file_spec, &parms);
	zshowall.mvtype = MV_STR;
	zshowall.str.addr = "*";
	zshowall.str.len = 1;
	op_zshow(&zshowall, ZSHOW_DEVICE, NULL);
	parms.str.addr = (char *)dumpable_error_dump_file_noparms;
	parms.str.len = SIZEOF(dumpable_error_dump_file_noparms);
	op_close(dump_file_spec, &parms);
	/* Notify operator dump was taken */
	send_msg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_JOBEXAMDONE, 3, process_id, dump_file_spec->str.len, dump_file_spec->str.addr);
	REVERT;
}

CONDITION_HANDLER(jobexam_dump_ch)
{
	boolean_t	save_created_core;

	START_CH;

	/* Operation:
	 * 1) Flush out message we came here because of to operator console
	 * 2) Put out our message stating that we screwed up
	 * 3) Unwind the errant frames so we can return to the user without screwing
	 *    up the task that got interrupted to do this examine.
	 */
#	if defined(DEBUG) && defined(UNIX)
	if (DUMPABLE)
	{	/* For debug UNIX issues, let's make a core if we would have made one in open code */
		save_created_core = created_core;
		gtm_fork_n_core();
		created_core = save_created_core;
	}
#	endif
	UNIX_ONLY(util_out_print(0, OPER));
	VMS_ONLY(sig->chf$l_sig_args -= 2);
	VMS_ONLY(callg(send_msg, &sig->chf$l_sig_args));
	send_msg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_JOBEXAMFAIL, 1, process_id);

	/* Stop the errors here and return to caller */
	UNIX_ONLY(util_out_print("", RESET));	/* Prevent rts_error from flushing this error later */
	DEBUG_ONLY(ok_to_UNWIND_in_exit_handling = TRUE);
	UNWIND(NULL, NULL);
}
