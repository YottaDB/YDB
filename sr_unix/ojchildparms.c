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

/*
* ---------------------------------------------------------
 * Parse job parameters
 * ---------------------------------------------------------
 */
#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdlib.h"
#include "gtm_unistd.h"
#include "eintr_wrappers.h"
#include "job.h"
#include "compiler.h"
#include "gcall.h"
#include "stringpool.h"
#include "op.h"		/* for op_nullexp() */
#include "io.h"
#include "iosocketdef.h"

#include <errno.h>

GBLREF	spdesc		stringpool;
GBLREF	io_log_name	*dollar_principal;
GBLREF	io_pair		io_std_device;

error_def(ERR_CLOSEFAIL);
error_def(ERR_JOBSETUP);
error_def(ERR_STRINGOFLOW);

/*
 * ------------------------------------------------
 * Get parameters from passed socket into
 * parameter structure
 * ------------------------------------------------
 */
void ojchildparms(job_params_type *jparms, gcall_args *g_args, mval *arglst)
{
	char			*sp, *parmbuf;
	char			parm_string[8];
	int4			argcnt, i;
	int			setup_fd;
	int			rc;
	job_setup_op		setup_op;
	boolean_t		setup_done = FALSE;
	job_params_msg		params;
	job_arg_count_msg	arg_count;
	job_arg_msg		arg_msg;
	job_buffer_size_msg	buffer_size;
	d_socket_struct		*dsocketptr;
	socket_struct		*socketptr;

	assertpro((sp = GETENV(CHILD_FLAG_ENV)) && sp[0]);	/* note assignment */
	setup_fd = (int)ATOL(sp);

	g_args->callargs = 0;

	while(!setup_done)
	{
		RECV(setup_fd, &setup_op, SIZEOF(setup_op), 0, rc);
		if (rc < 0)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JOBSETUP, 2, LEN_AND_LIT("setup operation"), errno, 0);
		assert(SIZEOF(setup_op) == rc);
		switch(setup_op)
		{
		case job_done:
			setup_done = TRUE;
			break;

		case job_set_params:
			RECV(setup_fd, &params, SIZEOF(params), 0, rc);
			if (rc < 0)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JOBSETUP, 2, LEN_AND_LIT("job parameters"), errno, 0);
			assert(SIZEOF(params) == rc);

			jparms->directory.len = params.directory_len;
			jparms->gbldir.len = params.gbldir_len;
			jparms->startup.len = params.startup_len;
			jparms->input.len = params.input_len;
			jparms->output.len = params.output_len;
			jparms->error.len = params.error_len;
			jparms->routine.len = params.routine_len;
			jparms->label.len = params.label_len;
			jparms->baspri = params.baspri;
			jparms->offset = params.offset;

			if (0 != params.directory_len)
			{
				jparms->directory.addr = malloc(jparms->directory.len + 1);
				memcpy(jparms->directory.addr, params.directory, jparms->directory.len + 1);
			}
			if (0 != params.gbldir_len)
			{
				jparms->gbldir.addr = malloc(jparms->gbldir.len + 1);
				memcpy(jparms->gbldir.addr, params.gbldir, jparms->gbldir.len + 1);
			}
			if (0 != params.startup_len)
			{
				jparms->startup.addr = malloc(jparms->startup.len + 1);
				memcpy(jparms->startup.addr, params.startup, jparms->startup.len + 1);
			}
			if (0 != params.input_len)
			{
				jparms->input.addr = malloc(jparms->input.len + 1);
				memcpy(jparms->input.addr, params.input, jparms->input.len + 1);
			}
			if (0 != params.output_len)
			{
				jparms->output.addr = malloc(jparms->output.len + 1);
				memcpy(jparms->output.addr, params.output, jparms->output.len + 1);
			}
			if (0 != params.error_len)
			{
				jparms->error.addr = malloc(jparms->error.len + 1);
				memcpy(jparms->error.addr, params.error, jparms->error.len + 1);
			}
			if (0 != params.routine_len)
			{
				jparms->routine.addr = malloc(jparms->routine.len + 1);
				memcpy(jparms->routine.addr, params.routine, jparms->routine.len + 1);
			}
			if (0 != params.label_len)
			{
				jparms->label.addr = malloc(jparms->label.len + 1);
				memcpy(jparms->label.addr, params.label, jparms->label.len + 1);
			}

			break;

		case job_set_parm_list:
			RECV(setup_fd, &arg_count, SIZEOF(arg_count), 0, rc);
			if (rc < 0)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JOBSETUP, 2, LEN_AND_LIT("argument count"), errno, 0);
			assert(SIZEOF(arg_count) == rc);
			g_args->callargs = arg_count + 4;
			g_args->truth = 1;
			g_args->retval = 0;
			g_args->mask = 0;
			g_args->argcnt = arg_count;
			for (i = 0; i < arg_count; i++)
			{
				RECV(setup_fd, &arg_msg, SIZEOF(arg_msg), 0, rc);
				if (rc < 0)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JOBSETUP, 2, LEN_AND_LIT("argument"),
							errno, 0);
				assert(SIZEOF(arg_msg) == rc);
				if (0 > arg_msg.len)
					g_args->argval[i] = op_nullexp();	/* negative len indicates null arg */
				else
				{
					if (!IS_STP_SPACE_AVAILABLE_PRO(STRLEN(sp)))
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) (ERR_STRINGOFLOW));
					arglst[i].str.len = arg_msg.len;
					arglst[i].str.addr = (char *)stringpool.free;
					memcpy(stringpool.free, arg_msg.data, arg_msg.len);
					stringpool.free += arg_msg.len;
					arglst[i].mvtype = MV_STR;
					g_args->argval[i] = &arglst[i];
				}
			}
			break;

		case job_set_input_buffer:
			assertpro(io_std_device.in && (gtmsocket == io_std_device.in->type));
			dsocketptr = (d_socket_struct *)(io_std_device.in->dev_sp);
			assertpro(dsocketptr);
			assertpro(-1 != dsocketptr->current_socket);
			assertpro(dsocketptr->current_socket < dsocketptr->n_socket);
			socketptr = dsocketptr->socket[dsocketptr->current_socket];

			RECV(setup_fd, &buffer_size, SIZEOF(buffer_size), 0, rc);
			if (rc < 0)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JOBSETUP, 2, LEN_AND_LIT("input buffer size"),
						errno, 0);
			assert(SIZEOF(buffer_size) == rc);
			assertpro(buffer_size <= DEFAULT_SOCKET_BUFFER_SIZE);
			assertpro(buffer_size <= socketptr->buffer_size);

			RECV(setup_fd, socketptr->buffer, buffer_size, 0, rc);
			if (rc < 0)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JOBSETUP, 2, LEN_AND_LIT("input buffer"), errno, 0);
			assert(buffer_size == rc);
			socketptr->buffered_length = buffer_size;
			socketptr->buffered_offset = 0;
			break;

		default:
			assertpro(FALSE && setup_op);
		}
	}
	if ((rc = close(setup_fd)) < 0)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CLOSEFAIL, 1, setup_fd, errno, 0);
}
