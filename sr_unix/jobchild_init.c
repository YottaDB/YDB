/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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

#include "rtnhdr.h"
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
#include "callg.h"
#include "invocation_mode.h"

#define FILE_NAME_SIZE	255

GBLREF	mval		dollar_zmode;
GBLREF	stack_frame	*frame_pointer;
GBLREF	uint4		process_id;

CONDITION_HANDLER(job_init_ch)
{
	START_CH;
	PRN_ERROR;
	NEXTCH;
}

/*
 * ---------------------------------------------------------
 * Child process test and initialization.
 * If this copy of GTM is a child process,
 * then initialize the child.
 * ---------------------------------------------------------
 */

void jobchild_init(void)
{
	unsigned int	status;
	job_params_type		jparms;

/* Transfer data */
	unsigned char	*transfer_addr;
	rhdtyp		*base_addr;
	unsigned short	i, arg_len;
	char		run_file_name[FILE_NAME_SIZE + 2], *c;
	gcall_args	job_arglist;
	mval		job_args[MAX_ACTUALS];
	error_def	(ERR_RUNPARAMERR);

	static char interactive_mode_buf[] = "INTERACTIVE";
	static char batch_mode_buf[] = "OTHER";

	error_def(ERR_TEXT);
	ESTABLISH(job_init_ch);

/*
 * Check if environment variable ppid - job parent pid
 * exists. If it does not, we are a regular gtm process,
 * else, we are a child process of a job command.
 */
	if ((c = GETENV(CHILD_FLAG_ENV)) && strlen(c))
	{
		/*
		 * We are a Jobbed process.
		 * Get Job parameters and set up environment
		 * to run the Job command
		 */

		/* Clear the environment variable so that subsequent child
		 * mumps processes can start normal initialization. */

		if (PUTENV(CLEAR_CHILD_FLAG_ENV))
		{
			util_out_print("Unable to clear gtmj0 process !UL exiting.", TRUE, process_id);
			rts_error(VARLSTCNT(1) errno);
		}

		/* read parameters into parameter structure */
		ojchildparms(&jparms, &job_arglist, job_args);

		/* Execute the command to be run before executing the actual M routine */
		if (jparms.startup.len)
			SYSTEM(jparms.startup.addr);

		/* Set up job's input, output and error files.  Redirect them, if necessary. */
		/* It is needed since the middle process would not have always done this(under jobpid == TRUE cases) */
		if (!(status = ojchildioset(&jparms)))
			rts_error(VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_LIT("Failed to set STDIN/OUT/ERR for the job"));

		job_addr(&jparms.routine, &jparms.label, jparms.offset, (char **)&base_addr, (char **)&transfer_addr);

		/* Set process priority */
		if (jparms.baspri)
			nice((int) jparms.baspri);
		/* Set up $ZMODE to "BATCH" */
		dollar_zmode.mvtype = MV_STR;
		dollar_zmode.str.addr = &batch_mode_buf[0];
		dollar_zmode.str.len = sizeof(batch_mode_buf) -1;
	} else
	{
		/* If we are not a child, setup a dummy mumps routine */
		if (MUMPS_RUN == invocation_mode)
		{
			mstr	routine, label;
			int	offset;

			arg_len = FILE_NAME_SIZE;
			if (!cli_get_str("INFILE", run_file_name, &arg_len))
				rts_error(VARLSTCNT(1) ERR_RUNPARAMERR);
			lref_parse((uchar_ptr_t)run_file_name, &routine, &label, &offset);
			job_addr(&routine, &label, offset, (char **)&base_addr, (char **)&transfer_addr);
		} else /* direct mode */
		{
			base_addr = make_dmode();
			transfer_addr = (unsigned char *)base_addr + base_addr->ptext_ptr;
		}
		job_arglist.callargs = 0;
		/* Set up $ZMODE to "INTERACTIVE" */
		dollar_zmode.mvtype = MV_STR;
		dollar_zmode.str.addr = &interactive_mode_buf[0];
		dollar_zmode.str.len = sizeof(interactive_mode_buf) -1;
	}
	gtm_init_env(base_addr, transfer_addr);
	if (job_arglist.callargs)
	{
		callg((int(*)())push_parm, &job_arglist);
		frame_pointer->type |= SFT_EXTFUN;
	}
	REVERT;
}
