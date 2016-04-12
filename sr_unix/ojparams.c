/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "job.h"
#include "min_max.h"
#include "io.h"
#include "iosocketdef.h"

/*
 * ---------------------------------------------------------
 * Parse job parameters
 * ---------------------------------------------------------
 */

static readonly char definput[] = "/dev/null";

static char *defoutbuf;
static char *deferrbuf;

MSTR_CONST(defoutext, ".mjo");
MSTR_CONST(deferrext, ".mje");

LITREF jp_datatype	job_param_datatypes[];

GBLREF	mval			dollar_zgbldir;
GBLREF	d_socket_struct		*socket_pool;

error_def		(ERR_PARFILSPC);

/*
 * ------------------------------------------------
 * Parse the parameter buffer and extract all
 * parameters for job command.
 * Save them in job_params structure.
 * ------------------------------------------------
 */

void ojparams (char *p, job_params_type *job_params)
{
	unsigned char		ch;
	int4			status;
	mstr_len_t		handle_len;

		/* Initializations */
	job_params->baspri = 0;
	job_params->input.len = 0;
	job_params->output.len = 0;
	job_params->error.len = 0;
	job_params->gbldir.len = 0;
	job_params->startup.len = 0;
	job_params->directory.len = 0;
	job_params->directory.addr = 0;
	job_params->cmdline.len = 0;
	job_params->cmdline.addr = 0;
	job_params->passcurlvn = FALSE;

		/* Process parameter list */
	while (*p != jp_eol)
	{
		switch (ch = *p++)
		{
		case jp_default:
			if (*p != 0)
			{
				job_params->directory.len = (int)((unsigned char) *p);
				job_params->directory.addr = (p + 1);
			}
			break;

		case jp_error:
			if (*p != 0)
			{
				job_params->error.len = (int)((unsigned char) *p);
				job_params->error.addr = (p + 1);
			}
			break;

		case jp_gbldir:
			if (*p != 0)
			{
				job_params->gbldir.len = (int)((unsigned char) *p);
				job_params->gbldir.addr = (p + 1);
			}
			break;

		case jp_input:
			if (*p != 0)
			{
				job_params->input.len = (int)((unsigned char) *p);
				job_params->input.addr = p + 1;
			}
			break;

		case jp_output:
			if (*p != 0)
			{
				job_params->output.len = (int)((unsigned char) *p);
				job_params->output.addr = p + 1;
			}
			break;

		case jp_priority:
			job_params->baspri = (int4)(*((int4 *)p));
			break;

		case jp_startup:
			if (*p != 0)
			{
				job_params->startup.len = (int)((unsigned char) *p);
				job_params->startup.addr = p + 1;
			}
			break;

		case jp_cmdline:
			if(*p != 0)
			{
				job_params->cmdline.len = (int)((unsigned char) *p);
				job_params->cmdline.addr = p + 1;
			}
			break;

		case jp_passcurlvn:
			job_params->passcurlvn = TRUE;
			break;
		case jp_account:
		case jp_detached:
		case jp_image:
		case jp_logfile:
		case jp_noaccount:
		case jp_nodetached:
		case jp_noswapping:
		case jp_process_name:
		case jp_schedule:
		case jp_swapping:
			break;
		default:
		        assertpro(ch != ch);
		}

		switch (job_param_datatypes[ch])
		{
		case jpdt_nul:
			break;

		case jpdt_num:
			p += SIZEOF(int4);
			break;

		case jpdt_str:
			p += ((int)((unsigned char)*p)) + 1;
			break;
		default:
			assertpro((jpdt_nul == job_param_datatypes[ch])
				|| (jpdt_num == job_param_datatypes[ch])
				|| (jpdt_str == job_param_datatypes[ch]));
		}
	}

/* Defaults and Checks */

/*
 * Input file
 */
	if (0 == job_params->input.len)
	{
		job_params->input.len = STRLEN(definput);
		job_params->input.addr = definput;
	}
	else if (IS_JOB_SOCKET(job_params->input.addr, job_params->input.len))
	{
		handle_len = JOB_SOCKET_HANDLE_LEN(job_params->input.len);
		if ((NULL == socket_pool) || (-1 == iosocket_handle(JOB_SOCKET_HANDLE(job_params->input.addr),
									&handle_len, FALSE, socket_pool)))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARFILSPC, 4, 5, "INPUT",
				job_params->input.len, job_params->input.addr);
	}
	else
		if (!(status = ojchkfs (job_params->input.addr,
		  job_params->input.len, TRUE)))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARFILSPC, 4, 5, "INPUT",
			job_params->input.len, job_params->input.addr);

/*
 * Output file
 */
	if (0 == job_params->output.len)
	{
		if (!defoutbuf)
			defoutbuf = malloc(MAX_FILSPC_LEN);
		memcpy (&defoutbuf[0], job_params->routine.addr,
		  job_params->routine.len);
		memcpy (&defoutbuf[job_params->routine.len],
		  defoutext.addr, defoutext.len);
		if (*defoutbuf == '%')
			*defoutbuf = '_';
		job_params->output.len = job_params->routine.len
		  + defoutext.len;
		job_params->output.addr = &defoutbuf[0];
	}
	else if (IS_JOB_SOCKET(job_params->output.addr, job_params->output.len))
	{
		handle_len = JOB_SOCKET_HANDLE_LEN(job_params->output.len);
		if ((NULL == socket_pool) || (-1 == iosocket_handle(JOB_SOCKET_HANDLE(job_params->output.addr),
									&handle_len, FALSE, socket_pool)))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARFILSPC, 4, 5, "OUTPUT",
				job_params->output.len, job_params->output.addr);
	}
	else
		if (!(status = ojchkfs (job_params->output.addr,
		  job_params->output.len, FALSE)))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARFILSPC, 4, 6,
				"OUTPUT", job_params->output.len,
				job_params->output.addr);
/*
 * Error file
 */
	if (0 == job_params->error.len)
	{
		if (!deferrbuf)
			deferrbuf = malloc(MAX_FILSPC_LEN);
		memcpy (&deferrbuf[0], job_params->routine.addr,
		  job_params->routine.len);
		memcpy (&deferrbuf[job_params->routine.len],
		  deferrext.addr, deferrext.len);
		if (*deferrbuf == '%')
			*deferrbuf = '_';
		job_params->error.len = job_params->routine.len
		  + deferrext.len;
		job_params->error.addr = &deferrbuf[0];
	}
	else if (IS_JOB_SOCKET(job_params->error.addr, job_params->error.len))
	{
		handle_len = JOB_SOCKET_HANDLE_LEN(job_params->error.len);
		if ((NULL == socket_pool) || (-1 == iosocket_handle(JOB_SOCKET_HANDLE(job_params->error.addr),
									&handle_len, FALSE, socket_pool)))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARFILSPC, 4, 5, "ERROR",
				job_params->error.len, job_params->error.addr);
	}
	else
		if (!(status = ojchkfs (job_params->error.addr,
		  job_params->error.len, FALSE)))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARFILSPC, 4, 5, "ERROR",
			  job_params->error.len,
			  job_params->error.addr);
/*
 * Global Directory
 */
	if (0 == job_params->gbldir.len)
	{
		assert(MAX_JOBPARM_LEN > dollar_zgbldir.str.len);
		job_params->gbldir.len = dollar_zgbldir.str.len;
		job_params->gbldir.addr = dollar_zgbldir.str.addr;
	}
	else
		if (!(status = ojchkfs (job_params->gbldir.addr,
		  job_params->gbldir.len, FALSE)))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARFILSPC, 4, 6, "GBLDIR",
			  job_params->gbldir.len, job_params->gbldir.addr);
/*
 * Startup
 */
	if (job_params->startup.len)
		if (!(status = ojchkfs (job_params->startup.addr,
		  job_params->startup.len, TRUE)))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARFILSPC, 4, 7, "STARTUP",
			  job_params->startup.len, job_params->startup.addr);
/*
 * Default Directory
 */
	if (job_params->directory.len)
		if (!(status = ojchkfs (job_params->directory.addr,
		  job_params->directory.len, FALSE)))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARFILSPC, 4, 7, "DEFAULT",
			  job_params->directory.len, job_params->directory.addr);
}

