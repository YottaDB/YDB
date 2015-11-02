/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
* ---------------------------------------------------------
 * Parse job parameters
 * ---------------------------------------------------------
 */
#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdlib.h"
#include "job.h"
#include "compiler.h"
#include "gcall.h"
#include "stringpool.h"
#include "op.h"		/* for op_nullexp() */

GBLREF	spdesc stringpool;

/*
 * ------------------------------------------------
 * Get parameters from environment variables into
 * parameter structure
 * ------------------------------------------------
 */
void ojchildparms(job_params_type *jparms, gcall_args *g_args, mval *arglst)
{
	char			*sp, *parmbuf;
	char			parm_string[8];
	int4			argcnt, i;

	error_def(ERR_STRINGOFLOW);

	/* getenv() may use static buffer so for any parms we fetch a value for, rebuffer them so they
	   don't go away. We use malloc'd space because the stringpool garbage collector does not know
	   about these private (to this routine) structures and we can't use "temp stringpool space" as
	   is used in other modules since we will be putting the parms in the stringpool below. The
	   storage allocated here will be released in jobchild_init() when it is done with it.
	*/
	if (jparms->directory.addr = GETENV(CWD_ENV))
	{
		jparms->directory.len = STRLEN(jparms->directory.addr);
		if (0 != jparms->directory.len)
		{
			parmbuf = malloc(jparms->directory.len+1);
			memcpy(parmbuf, jparms->directory.addr, jparms->directory.len+1);
		 	jparms->directory.addr = parmbuf;
	 	}
	} else
		jparms->directory.len = 0;

	if (jparms->gbldir.addr = GETENV(GBLDIR_ENV))
	{
		jparms->gbldir.len = STRLEN(jparms->gbldir.addr);
		if (0 != jparms->gbldir.len)
		{
			parmbuf = malloc(jparms->gbldir.len+1);
			memcpy(parmbuf, jparms->gbldir.addr, jparms->gbldir.len+1);
			jparms->gbldir.addr = parmbuf;
		}
	} else
		jparms->gbldir.len = 0;

	if (jparms->startup.addr = GETENV(STARTUP_ENV))
	{
		jparms->startup.len = STRLEN(jparms->startup.addr);
		if (0 != jparms->startup.len)
		{
			parmbuf = malloc(jparms->startup.len+1);
			memcpy(parmbuf, jparms->startup.addr, jparms->startup.len+1);
			jparms->startup.addr = parmbuf;
		}
	} else
		jparms->startup.len = 0;

	if (jparms->input.addr = GETENV(IN_FILE_ENV))
	{
		jparms->input.len = STRLEN(jparms->input.addr);
		if (0 != jparms->input.len)
		{
			parmbuf = malloc(jparms->input.len+1);
			memcpy(parmbuf, jparms->input.addr, jparms->input.len+1);
			jparms->input.addr = parmbuf;
		}
	} else
		jparms->input.len = 0;

	if (jparms->output.addr = GETENV(OUT_FILE_ENV))
	{
		jparms->output.len = STRLEN(jparms->output.addr);
		if (0 != jparms->output.len)
		{
			parmbuf = malloc(jparms->output.len+1);
			memcpy(parmbuf, jparms->output.addr, jparms->output.len+1);
			jparms->output.addr = parmbuf;
		}
	} else
		jparms->output.len = 0;

	if (jparms->error.addr = GETENV(ERR_FILE_ENV))
	{
		jparms->error.len = STRLEN(jparms->error.addr);
		if (0 != jparms->error.len)
		{
			parmbuf = malloc(jparms->error.len+1);
			memcpy(parmbuf, jparms->error.addr, jparms->error.len+1);
			jparms->error.addr = parmbuf;
		}
	} else
		jparms->error.len = 0;

	if (jparms->routine.addr = GETENV(ROUTINE_ENV))
	{
		jparms->routine.len = STRLEN(jparms->routine.addr);
		if (0 != jparms->routine.len)
		{
			parmbuf = malloc(jparms->routine.len+1);
			memcpy(parmbuf, jparms->routine.addr, jparms->routine.len+1);
			jparms->routine.addr = parmbuf;
		}
	} else
		jparms->routine.len = 0;

	if (jparms->label.addr = GETENV(LABEL_ENV))
	{
		jparms->label.len = STRLEN(jparms->label.addr);
		if (0 != jparms->label.len)
		{
			parmbuf = malloc(jparms->label.len+1);
			memcpy(parmbuf, jparms->label.addr, jparms->label.len+1);
			jparms->label.addr = parmbuf;
		}
	} else
		jparms->label.len = 0;

	if (jparms->logfile.addr = GETENV(LOG_FILE_ENV))
	{
		jparms->logfile.len = STRLEN(jparms->logfile.addr);
		if (0 != jparms->logfile.len)
		{
			parmbuf = malloc(jparms->logfile.len+1);
			memcpy(parmbuf, jparms->logfile.addr, jparms->logfile.len+1);
			jparms->logfile.addr = parmbuf;
		}
	} else
		jparms->logfile.len = 0;

	if (sp = GETENV(OFFSET_ENV))
		jparms->offset = (int)(ATOL(sp));
	else
		jparms->offset = 0;

	if (sp = GETENV(PRIORITY_ENV))
		jparms->baspri = (int)(ATOL(sp));
	else
		jparms->baspri = 0;

	if (!(sp = GETENV(GTMJCNT_ENV)))
		GTMASSERT;

	if (argcnt = (int)(ATOL(sp)))
	{
		ENSURE_STP_FREE_SPACE(argcnt * MAX_TRANS_NAME_LEN);
		g_args->callargs = argcnt + 4;
		g_args->truth = 1;
		g_args->retval = 0;
		g_args->mask = 0;
		g_args->argcnt = argcnt;
		memcpy(parm_string,"gtmj000",8);
		for (i = 0; i < argcnt; i++)
		{
			if (sp = GETENV(parm_string))
			{
				if (!IS_STP_SPACE_AVAILABLE_PRO(STRLEN(sp)))
					rts_error(VARLSTCNT(1) (ERR_STRINGOFLOW));
				arglst[i].str.len = STRLEN(sp);
				arglst[i].str.addr = (char *)stringpool.free;
				memcpy(stringpool.free, sp, arglst[i].str.len);
				stringpool.free += arglst[i].str.len;
				arglst[i].mvtype = MV_STR;
			} else
				op_nullexp(&arglst[i]);
			g_args->argval[i] = &arglst[i];
			if (parm_string[6] == '9')
			{
				if (parm_string[5] == '9')
				{
					parm_string[4] = parm_string[4] + 1;
					parm_string[5] = '0';
				} else
					parm_string[5] = parm_string[5] + 1;
				parm_string[6] = '0';
			} else
				parm_string[6] = parm_string[6] + 1;
		}
	} else
		g_args->callargs = 0;
}
