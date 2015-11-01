/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#include "gtm_string.h"
#include "gtm_stdio.h"
#include "error.h"
#include "io_params.h"
#include "op.h"
#include "io.h"
#include "jobexam_process.h"
#ifdef UNIX
#  include "jobexam_signal_handler.h"
#endif
#include "send_msg.h"
#include "callg.h"
#include "zshow.h"
#include "util.h"
#include "hashdef.h"
#include "lv_val.h"

#define DEFAULT_DUMP_FILENAME "GTM_JOBEXAM.ZSHOW_DMP"
#define NOCONCEAL_OPTION "NO_CONCEAL"

static readonly mval empty_str_mval = DEFINE_MVAL_LITERAL(MV_STR, 0, 0, 0, 0, 0, 0);
static readonly mval no_conceal_op  = DEFINE_MVAL_LITERAL(MV_STR, 0, 0, sizeof(NOCONCEAL_OPTION) - 1,
							 NOCONCEAL_OPTION, 0, 0);
static unsigned char dumpable_error_dump_file_parms[2] = {iop_newversion, iop_eol};
static unsigned char dumpable_error_dump_file_noparms[1] = {iop_eol};
static unsigned int  jobexam_counter;

GBLREF uint4	process_id;
GBLREF io_pair	io_std_device, io_curr_device;
UNIX_ONLY(GBLREF sigset_t blockalrm;)

error_def(ERR_JOBEXAMFAIL);
error_def(ERR_JOBEXAMDONE);

void jobexam_process(mval *dump_file_name, mval *dump_file_spec)
{
	io_pair			dev_in_use;
#ifdef UNIX
	struct sigaction	new_action, prev_action;
	sigset_t		savemask;

	/* Block out timer calls that might trigger processing that could fail. We especially want to prevent
	   nesting of signal handlers since the longjump() function used by the UNWIND macro is undefined on
	   Tru64 when signal handlers are nested.
	*/
	sigprocmask(SIG_BLOCK, &blockalrm, &savemask);

	/* Setup new signal handler to just drive condition handler which will do the right thing */
	memset(&new_action, 0, sizeof(new_action));
	sigemptyset(&new_action.sa_mask);
	new_action.sa_flags = SA_SIGINFO;
#ifdef __sparc
	new_action.sa_handler = jobexam_signal_handler;
#else
	new_action.sa_sigaction = jobexam_signal_handler;
#endif
	sigaction(SIGBUS, &new_action, &prev_action);
	sigaction(SIGSEGV, &new_action, 0);
#endif
	*dump_file_spec = empty_str_mval;
	dev_in_use = io_curr_device;		/* Save current IO device */
	jobexam_dump(dump_file_name, dump_file_spec);
	/* If any errors occur in job_exam_dump, the condition handler will unwind the stack
	   to this point and return.
	*/
	io_curr_device = dev_in_use;		/* Restore IO device */
#ifdef UNIX
	/* Restore the signal handlers how they were */
	sigaction(SIGBUS, &prev_action, 0);
	sigaction(SIGSEGV, &prev_action, 0);
	/* Let the timers pop again.. */
	sigprocmask(SIG_SETMASK, &savemask, NULL);
#endif
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
	memcpy(dump_file_name, DEFAULT_DUMP_FILENAME, sizeof(DEFAULT_DUMP_FILENAME) - 1);
	dump_file_name_ptr = dump_file_name + sizeof(DEFAULT_DUMP_FILENAME) - 1;
	*dump_file_name_ptr++ = '_';
	dump_file_name_ptr = i2asc(dump_file_name_ptr, process_id);
	*dump_file_name_ptr++ = '_';
	dump_file_name_ptr = i2asc(dump_file_name_ptr, jobexam_counter);
	def_file_name.mvtype = MV_STR;
	def_file_name.str.addr = (char *)dump_file_name;
	def_file_name.str.len = dump_file_name_ptr - dump_file_name;
	/* Call $ZPARSE processing to fill in any blanks, expand concealed logicals, etc. It is the callers
	   responsibility to make sure garbage collection knows about the value in the returned filespec.
	*/
	op_fnzparse(dump_filename_arg, &empty_str_mval, &def_file_name, &empty_str_mval, &no_conceal_op, dump_file_spec);
	/* Parms of file to be created (newversion) */
	parms.mvtype = MV_STR;
	parms.str.addr = (char *)dumpable_error_dump_file_parms;
	parms.str.len = sizeof(dumpable_error_dump_file_parms);
	/* Open, use, and zshow into new file, then close and reset current io device */
	op_open(dump_file_spec, &parms, 0, 0);
	op_use(dump_file_spec, &parms);
	zshowall.mvtype = MV_STR;
	zshowall.str.addr = "*";
	zshowall.str.len = 1;
	op_zshow(&zshowall, ZSHOW_DEVICE, NULL);
	parms.str.addr = (char *)dumpable_error_dump_file_noparms;
	parms.str.len = sizeof(dumpable_error_dump_file_noparms);
	op_close(dump_file_spec, &parms);
	/* Notify operator dump was taken */
	send_msg(VARLSTCNT(5) ERR_JOBEXAMDONE, 3, process_id, dump_file_spec->str.len, dump_file_spec->str.addr);
	REVERT;
}

CONDITION_HANDLER(jobexam_dump_ch)
{
	START_CH;

	/* Operation:
	   1) Flush out message we came here because of to operator console
	   2) Put out our message stating that we screwed up
	   3) Unwind the errant frames so we can return to the user without screwing
	      up the task that got interrupted to do this examine.
	*/
	UNIX_ONLY(util_out_print(0, OPER));
	VMS_ONLY(sig->chf$l_sig_args -= 2);
	VMS_ONLY(callg(send_msg, &sig->chf$l_sig_args));
	send_msg(VARLSTCNT(3) ERR_JOBEXAMFAIL, 1, process_id);

	/* Stop the errors here and return to caller */
	UNWIND(NULL, NULL);
}
