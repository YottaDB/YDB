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

GBLREF	spdesc stringpool;

/*
 * ------------------------------------------------
 * Get parameters from environment variables into
 * parameter structure
 * ------------------------------------------------
 */
void ojchildparms(job_params_type *jparms, gcall_args *g_args, mval *arglst)
{
	char			*sp;
	char			parm_string[8];
	int4			argcnt, i;

	error_def(ERR_STRINGOFLOW);

	if (jparms->directory.addr = GETENV(CWD_ENV))
		jparms->directory.len = strlen(jparms->directory.addr);
	else
		jparms->directory.len = 0;

	if (jparms->gbldir.addr = GETENV(GBLDIR_ENV))
		jparms->gbldir.len = strlen(jparms->gbldir.addr);
	else
		jparms->gbldir.len = 0;

	if (jparms->startup.addr = GETENV(STARTUP_ENV))
		jparms->startup.len = strlen(jparms->startup.addr);
	else
		jparms->startup.len = 0;

	if (jparms->input.addr = GETENV(IN_FILE_ENV))
		jparms->input.len = strlen(jparms->input.addr);
	else
		jparms->input.len = 0;

	if (jparms->output.addr = GETENV(OUT_FILE_ENV))
		jparms->output.len = strlen(jparms->output.addr);
	else
		jparms->output.len = 0;

	if (jparms->error.addr = GETENV(ERR_FILE_ENV))
		jparms->error.len = strlen(jparms->error.addr);
	else
		jparms->error.len = 0;

	if (jparms->routine.addr = GETENV(ROUTINE_ENV))
		jparms->routine.len = strlen(jparms->routine.addr);
	else
		jparms->routine.len = 0;

	if (jparms->label.addr = GETENV(LABEL_ENV))
		jparms->label.len = strlen(jparms->label.addr);
	else
		jparms->label.len = 0;

	if (jparms->logfile.addr = GETENV(LOG_FILE_ENV))
		jparms->logfile.len = strlen(jparms->logfile.addr);
	else
		jparms->logfile.len = 0;

	if (sp = GETENV(OFFSET_ENV))
		jparms->offset = ATOL(sp);
	else
		jparms->offset = 0;

	if (sp = GETENV(PRIORITY_ENV))
		jparms->baspri = ATOL(sp);
	else
		jparms->baspri = 0;

	if (!(sp = GETENV(GTMJCNT_ENV)))
		GTMASSERT;

	if (argcnt = ATOL(sp))
	{
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
				if (stringpool.free + strlen(sp) > stringpool.top)
					rts_error(VARLSTCNT(1) (ERR_STRINGOFLOW));
				arglst[i].str.len = strlen(sp);
				arglst[i].str.addr = (char *)stringpool.free;
				memcpy(stringpool.free, sp, arglst[i].str.len);
				stringpool.free += arglst[i].str.len;
				arglst[i].mvtype = MV_STR;
				g_args->argval[i] = &arglst[i];

			} else
				GTMASSERT;
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
