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
#include "gtm_unistd.h"

#include <errno.h>

#include <rtnhdr.h>
#include "startup.h"
#include "gtm_startup.h"
#include "stack_frame.h"
#include "job_addr.h"
#include "compiler.h"
#include "indir_enum.h"
#include "error.h"
#include "util.h"
#include "cli.h"
#include "job.h"
#include "gcall.h"
#include "jobchild_init.h"
#include "lv_val.h"	/* needed for "callg.h" */
#include "callg.h"
#include "invocation_mode.h"
#include "gtmci.h"
#include "send_msg.h"
#include "have_crit.h"

#define FILE_NAME_SIZE	255

LITDEF char 		interactive_mode_buf[] = "INTERACTIVE";
LITDEF char 		other_mode_buf[] = "OTHER";

GBLREF stack_frame	*frame_pointer;
GBLREF uint4		process_id;

error_def(ERR_RUNPARAMERR);
error_def(ERR_TEXT);
error_def(ERR_SYSCALL);
error_def(ERR_JOBLABOFF);

CONDITION_HANDLER(job_init_ch)
{
	START_CH;
	PRN_ERROR;
	NEXTCH;
}

/* Child process test and initialization. If this copy of GTM is a child process, then initialize the child. */
void jobchild_init(void)
{
	unsigned int	status;
	job_params_type	jparms;
	unsigned char	*transfer_addr;		/* Transfer data */
	rhdtyp		*base_addr;
	unsigned short	i, arg_len;
	char		run_file_name[FILE_NAME_SIZE + 2], *c;
	gcall_args	job_arglist;
	mval		job_args[MAX_ACTUALS];
	mstr		routine, label;
	int		offset;
	int		rc;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ESTABLISH(job_init_ch);
	/* Check if environment variable ppid - job parent pid exists. If it does not, we are a regular
	 * gtm process; else, we are a child process of a job command.
	 */
	if ((c = GETENV(CHILD_FLAG_ENV)) && strlen(c))
	{	/* We are a Jobbed process Get Job parameters and set up environment to run the Job command.
		 * Clear the environment variable so that subsequent child mumps processes can start normal initialization.
		 */
		if (PUTENV(CLEAR_CHILD_FLAG_ENV))
		{
			util_out_print("Unable to clear gtmj0 process !UL exiting.", TRUE, process_id);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) errno);
		}
		/* read parameters into parameter structure */
		ojchildparms(&jparms, &job_arglist, job_args);
		/* Execute the command to be run before executing the actual M routine */
		if (jparms.startup.len)
		{
			rc = SYSTEM(jparms.startup.addr);
			if ((0 != rc))
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TEXT, 2,
						LEN_AND_LIT("STARTUP command failed"));
		}
		if(!job_addr(&jparms.routine, &jparms.label, jparms.offset,
				(char **)&base_addr, (char **)&transfer_addr))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JOBLABOFF);
		/* Set process priority */
		if (jparms.baspri)
		{	/* send message to system log if nice fails */
			if (-1 == nice((int)jparms.baspri))
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("nice"), CALLFROM, errno);
		}
		/* Set up $ZMODE to "OTHER" */
		(TREF(dollar_zmode)).mvtype = MV_STR;
		(TREF(dollar_zmode)).str.addr = (char *)other_mode_buf;
		(TREF(dollar_zmode)).str.len = SIZEOF(other_mode_buf) -1;
		/* Release storage allocated by ojchildparms() */
		if (jparms.directory.len)
			free(jparms.directory.addr);
		if (jparms.gbldir.len)
			free(jparms.gbldir.addr);
		if (jparms.startup.len)
			free(jparms.startup.addr);
		if (jparms.input.len)
			free(jparms.input.addr);
		if (jparms.output.len)
			free(jparms.output.addr);
		if (jparms.error.len)
			free(jparms.error.addr);
		if (jparms.routine.len)
			free(jparms.routine.addr);
		if (jparms.label.len)
			free(jparms.label.addr);
		if (jparms.logfile.len)
			free(jparms.logfile.addr);
	} else
	{	/* If we are not a child, setup a dummy mumps routine */
		if (MUMPS_RUN == invocation_mode)
		{
			arg_len = FILE_NAME_SIZE;
			if (!cli_get_str("INFILE", run_file_name, &arg_len))
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_RUNPARAMERR);
			lref_parse((uchar_ptr_t)run_file_name, &routine, &label, &offset);
			if(!job_addr(&routine, &label, offset, (char **)&base_addr, (char **)&transfer_addr))
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JOBLABOFF);
		} else if (MUMPS_CALLIN & invocation_mode) /* call-in mode */
		{
			base_addr = make_cimode();
			transfer_addr = PTEXT_ADR(base_addr);
		} else /* direct mode */
		{
			base_addr = make_dmode();
			transfer_addr = PTEXT_ADR(base_addr);
		}
		job_arglist.callargs = 0;
		/* Set up $ZMODE to "INTERACTIVE" */
		(TREF(dollar_zmode)).mvtype = MV_STR;
		(TREF(dollar_zmode)).str.addr = (char *)interactive_mode_buf;
		(TREF(dollar_zmode)).str.len = SIZEOF(interactive_mode_buf) -1;
	}
	gtm_init_env(base_addr, transfer_addr);
	if (MUMPS_CALLIN & invocation_mode)
	{
		SET_CI_ENV(ci_ret_code_exit);
	}
	if (job_arglist.callargs)
		callg((INTPTR_T (*)(intszofptr_t cnt, ...))push_parm, (gparam_list *)&job_arglist);
	REVERT;
}
