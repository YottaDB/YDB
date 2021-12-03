/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "eintr_wrappers.h"

/*
 * ---------------------------------------------------------
 * Parse job parameters
 * ---------------------------------------------------------
 */

static readonly char definput[] = DEVNULL;

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
	FILE			*curlvn_out;

		/* Initializations */
	job_params->params.baspri = 0;
	job_params->params.input.len = 0;
	job_params->params.output.len = 0;
	job_params->params.error.len = 0;
	job_params->params.gbldir.len = 0;
	job_params->params.startup.len = 0;
	job_params->params.directory.len = 0;
	/* job_params->params.routine.len initialized by caller */
	/* job_params->params.label.len initialized by caller */
	job_params->cmdline.len = 0;
	job_params->passcurlvn = FALSE;

		/* Process parameter list */
	while (*p != jp_eol)
	{
		switch (ch = *p++)
		{
		case jp_default:
			if (*p != 0)
			{
				job_params->params.directory.len = (int)((unsigned char) *p);
				memcpy(job_params->params.directory.buffer, p + 1, job_params->params.directory.len);
			}
			break;

		case jp_error:
			if (*p != 0)
			{
				job_params->params.error.len = (int)((unsigned char) *p);
				memcpy(job_params->params.error.buffer, p + 1, job_params->params.error.len);
			}
			break;

		case jp_gbldir:
			if (*p != 0)
			{
				job_params->params.gbldir.len = (int)((unsigned char) *p);
				memcpy(job_params->params.gbldir.buffer, p + 1, job_params->params.gbldir.len);
			}
			break;

		case jp_input:
			if (*p != 0)
			{
				job_params->params.input.len = (int)((unsigned char) *p);
				memcpy(job_params->params.input.buffer, p + 1, job_params->params.input.len);
			}
			break;

		case jp_output:
			if (*p != 0)
			{
				job_params->params.output.len = (int)((unsigned char) *p);
				memcpy(job_params->params.output.buffer, p + 1, job_params->params.output.len);
			}
			break;

		case jp_priority:
			job_params->params.baspri = (int4)(*((int4 *)p));
			break;

		case jp_startup:
			if (*p != 0)
			{
				job_params->params.startup.len = (int)((unsigned char) *p);
				memcpy(job_params->params.startup.buffer, p + 1, job_params->params.startup.len);
			}
			break;

		case jp_cmdline:
			if(*p != 0)
			{
				job_params->cmdline.len = (int)((unsigned char) *p);
				memcpy(job_params->cmdline.buffer, p + 1, job_params->cmdline.len);
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
	if (0 == job_params->params.input.len)
	{
		job_params->params.input.len = STRLEN(definput);
		memcpy(job_params->params.input.buffer, definput, job_params->params.input.len);
	}
	else if (IS_JOB_SOCKET(job_params->params.input.buffer, job_params->params.input.len))
	{
		handle_len = JOB_SOCKET_HANDLE_LEN(job_params->params.input.len);
		if ((NULL == socket_pool) || (-1 == iosocket_handle(JOB_SOCKET_HANDLE(job_params->params.input.buffer),
									&handle_len, FALSE, socket_pool)))
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_PARFILSPC, 4, 5, "INPUT",
				job_params->params.input.len, job_params->params.input.buffer);
	}
	else
		if (!(status = ojchkfs (job_params->params.input.buffer,
					job_params->params.input.len, TRUE)))
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_PARFILSPC, 4, 5, "INPUT",
				job_params->params.input.len, job_params->params.input.buffer);

/*
 * Output file
 */
	if (0 == job_params->params.output.len)
	{
		memcpy (&job_params->params.output.buffer[0], job_params->params.routine.buffer, job_params->params.routine.len);
		memcpy (&job_params->params.output.buffer[job_params->params.routine.len], defoutext.addr, defoutext.len);
		if (job_params->params.output.buffer[0] == '%')
			job_params->params.output.buffer[0] = '_';
		job_params->params.output.len = job_params->params.routine.len + defoutext.len;
	}
	else if (IS_JOB_SOCKET(job_params->params.output.buffer, job_params->params.output.len))
	{
		handle_len = JOB_SOCKET_HANDLE_LEN(job_params->params.output.len);
		if ((NULL == socket_pool) || (-1 == iosocket_handle(JOB_SOCKET_HANDLE(job_params->params.output.buffer),
									&handle_len, FALSE, socket_pool)))
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_PARFILSPC, 4, 5, "OUTPUT",
				job_params->params.output.len, job_params->params.output.buffer);
	}
	else
		if (!(status = ojchkfs (job_params->params.output.buffer,
					job_params->params.output.len, FALSE)))
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_PARFILSPC, 4, 6,
				"OUTPUT", job_params->params.output.len, job_params->params.output.buffer);
/*
 * Error file
 */
	if (0 == job_params->params.error.len)
	{
		memcpy(&job_params->params.error.buffer[0], job_params->params.routine.buffer, job_params->params.routine.len);
		memcpy(&job_params->params.error.buffer[job_params->params.routine.len], deferrext.addr, deferrext.len);
		if (job_params->params.error.buffer[0] == '%')
			job_params->params.error.buffer[0] = '_';
		job_params->params.error.len = job_params->params.routine.len + deferrext.len;
	}
	else if (IS_JOB_SOCKET(job_params->params.error.buffer, job_params->params.error.len))
	{
		handle_len = JOB_SOCKET_HANDLE_LEN(job_params->params.error.len);
		if ((NULL == socket_pool) || (-1 == iosocket_handle(JOB_SOCKET_HANDLE(job_params->params.error.buffer),
									&handle_len, FALSE, socket_pool)))
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_PARFILSPC, 4, 5, "ERROR",
				job_params->params.error.len, job_params->params.error.buffer);
	}
	else
		if (!(status = ojchkfs (job_params->params.error.buffer,
					job_params->params.error.len, FALSE)))
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_PARFILSPC, 4, 5, "ERROR",
				job_params->params.error.len, job_params->params.error.buffer);
/*
 * Global Directory
 */
	if (0 == job_params->params.gbldir.len)
	{
		assert(MAX_JOBPARM_LEN > dollar_zgbldir.str.len);
		job_params->params.gbldir.len = dollar_zgbldir.str.len;
		memcpy(job_params->params.gbldir.buffer, dollar_zgbldir.str.addr, dollar_zgbldir.str.len);
	}
	else
		if (!(status = ojchkfs (job_params->params.gbldir.buffer,
					job_params->params.gbldir.len, FALSE)))
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_PARFILSPC, 4, 6, "GBLDIR",
				job_params->params.gbldir.len, job_params->params.gbldir.buffer);
/*
 * Startup
 */
	if (job_params->params.startup.len)
		if (!(status = ojchkfs (job_params->params.startup.buffer,
					job_params->params.startup.len, TRUE)))
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_PARFILSPC, 4, 7, "STARTUP",
				job_params->params.startup.len, job_params->params.startup.buffer);
/*
 * Default Directory
 */
	if (job_params->params.directory.len)
		if (!(status = ojchkfs (job_params->params.directory.buffer,
					job_params->params.directory.len, FALSE)))
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_PARFILSPC, 4, 7, "DEFAULT",
				job_params->params.directory.len, job_params->params.directory.buffer);

	/* Gather local variables to pass */
	if (job_params->passcurlvn)
	{	/* Create a "memory file" to store the job_set_locals messages for later transmission by the middle child. */
		curlvn_out = open_memstream(&job_params->curlvn_buffer_ptr, &job_params->curlvn_buffer_size);
		local_variable_marshalling(curlvn_out);
		FCLOSE(curlvn_out, status);	/* Force "written" messages into the buffer */
	}
}
