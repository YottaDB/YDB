/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "indir_enum.h"
#include <errno.h>
#include "gtm_maxstr.h"
#include "job_addr.h"
#include "gtmci.h"
#include "cache.h"
#include "hashtab_objcode.h"
#include "gtmio.h"
#include "gtmmsg.h"		/* for gtm_putmsg prototype */

GBLREF spdesc			stringpool;
GBLREF io_log_name		*dollar_principal;
GBLREF io_pair			io_std_device;
GBLREF int4			aligned_source_buffer[];
GBLREF stack_frame		*frame_pointer;
GBLREF hash_table_objcode	cache_table;
#ifdef DEBUG
GBLREF unsigned char		*msp;
static unsigned char		*save_msp;
#endif
static	char			*sp;

STATICFNDCL void receive_child_locals_init(char **local_buff, mval **comm_stack_ptr);
STATICFNDCL void receive_child_locals_finalize(char **local_buff);
/* All other platforms use this much faster direct return */
void gtm_levl_ret_code(void);

error_def(ERR_CLOSEFAIL);
error_def(ERR_JOBSETUP);
error_def(ERR_STRINGOFLOW);
error_def(ERR_JOBLVN2LONG);
error_def(ERR_MAXACTARG);

/*
 * ------------------------------------------------
 * Get parameters from passed socket into
 * parameter structure
 * ------------------------------------------------
 */
STATICFNDEF void ojchildparms(job_params_type *jparms, gcall_args *g_args, mval *arglst)
{
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
	char			*local_buff = NULL;
	mval			*command_str;

	if ((NULL == sp) && (!((sp = GETENV(CHILD_FLAG_ENV)) && sp[0]))) /* note assignment */
		return;
	setup_fd = (int)ATOL(sp);
	if (NULL != g_args)
		g_args->callargs = 0;
	while(!setup_done)
	{
		DOREADRC(setup_fd, &setup_op, SIZEOF(setup_op), rc);
		if (rc < 0)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JOBSETUP, 2, LEN_AND_LIT("setup operation"), errno, 0);
		switch(setup_op)
		{
		case job_done:
		case local_trans_done:
			setup_done = TRUE;
			break;
		case job_set_params:
			DOREADRC(setup_fd, &params, SIZEOF(params), rc);
			if (rc < 0)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JOBSETUP, 2, LEN_AND_LIT("job parameters"), errno, 0);

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
			DOREADRC(setup_fd, &arg_count, SIZEOF(arg_count), rc);
			if (rc < 0)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JOBSETUP, 2, LEN_AND_LIT("argument count"), errno, 0);
			if (arg_count > MAX_ACTUALS)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXACTARG);
			g_args->callargs = arg_count + PUSH_PARM_OVERHEAD;
			g_args->truth = 1;
			g_args->retval = 0;
			g_args->mask = 0;
			g_args->argcnt = arg_count;
			ENSURE_STP_FREE_SPACE(arg_count * MAX_JOB_LEN);
			for (i = 0; i < arg_count; i++)
			{
				DOREADRC(setup_fd, &arg_msg.len, SIZEOF(arg_msg.len), rc);
				if (rc < 0)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JOBSETUP, 2, LEN_AND_LIT("argument length"),
						      errno, 0);
				if (0 > arg_msg.len)
					g_args->argval[i] = op_nullexp();	/* negative len indicates null arg */
				else
				{
					DOREADRC(setup_fd, &arg_msg.data, arg_msg.len, rc);
					if (rc < 0)
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JOBSETUP, 2, LEN_AND_LIT("argument"),
							errno, 0);
					assertpro(arg_msg.len <= MAX_JOB_LEN);
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
			DOREADRC(setup_fd, &buffer_size, SIZEOF(buffer_size), rc);
			if (rc < 0)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JOBSETUP, 2,
					      LEN_AND_LIT("input buffer size"), errno, 0);
			assertpro(buffer_size <= DEFAULT_SOCKET_BUFFER_SIZE);
			assertpro(buffer_size <= socketptr->buffer_size);
			DOREADRC(setup_fd, socketptr->buffer, buffer_size, rc);
			if (rc < 0)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JOBSETUP, 2,
					      LEN_AND_LIT("input buffer"), errno, 0);
			socketptr->buffered_length = buffer_size;
			socketptr->buffered_offset = 0;
			break;
		case job_set_locals:
			/* We should get here from the second ojchildparams() call only */
			assert(NULL == jparms);
			if (NULL == local_buff)
			{	/* Initializations to receive the local vars */
				receive_child_locals_init(&local_buff, &command_str);
			}
			command_str->mvtype = MV_STR;
			command_str->str.addr = local_buff;
			DOREADRC(setup_fd, &buffer_size, SIZEOF(buffer_size), rc);
			if (rc < 0)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JOBSETUP, 2,
					      LEN_AND_LIT("receive buffer size"), errno, 0);
			if(buffer_size > MAX_STRLEN)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_JOBLVN2LONG, 2, MAX_STRLEN, buffer_size);
			assert(buffer_size > 0);
			DOREADRC(setup_fd, local_buff, buffer_size, rc);
			if (rc < 0)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JOBSETUP, 2,
					      LEN_AND_LIT("local fragment"), errno, 0);
			assert((NULL != command_str->str.addr) && (0 != buffer_size));
			command_str->str.len = buffer_size;
			s2pool(&command_str->str);
			op_commarg(command_str, indir_set);
			dm_start();
			break;
		default:
			assertpro(FALSE && setup_op);
		}
	}
	if (NULL != local_buff)
		receive_child_locals_finalize(&local_buff);
	/* Keep the pipe alive until local transfer is done which is done at the second call to this function */
	if (local_trans_done == setup_op)
		if ((rc = close(setup_fd)) < 0)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CLOSEFAIL, 1, setup_fd, errno, 0);
}

STATICFNDEF void receive_child_locals_init(char **local_buff, mval **command_str)
{
	rhdtyp	*base_addr;
	int i;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(active_ch == ctxt);
	assert(ctxt == chnd);
	DEBUG_ONLY(save_msp = msp);
	*local_buff = malloc(MAX_STRLEN);
	(TREF(source_buffer)).addr = malloc(MAX_STRLEN + 2);	/* changing source_buffer prevents long items from causing errors */
	(TREF(source_buffer)).len = MAX_STRLEN + 1;
	/* Get space from the stack to save the command strings before putting the base stack frame This must be done first (before
	 * putting the base frame) so that dm_start does not unintentionally pop strings off the stack
	 */
	PUSH_MV_STENT(MVST_MVAL);
	*command_str = &mv_chain->mv_st_cont.mvs_mval;
	/* Setup the base frame */
	base_addr = make_cimode();
	base_frame(base_addr);
	/* Finish base frame initialization - reset mpc/context to return to us without unwinding base frame */
	frame_pointer->mpc = CODE_ADDRESS(gtm_levl_ret_code);
	frame_pointer->ctxt = GTM_CONTEXT(gtm_levl_ret_code);
}

STATICFNDEF void receive_child_locals_finalize(char **local_buff)
{
	int i;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Unwind the base frame */
	op_unwind();
	frame_pointer = *(stack_frame**)msp;
	msp += SIZEOF(stack_frame *);           /* Remove frame save pointer from stack */
	free(*local_buff);
	free((TREF(source_buffer)).addr);
	/* Reset the source buffer */
	(TREF(source_buffer)).len = MAX_SRCLINE;
	(TREF(source_buffer)).addr = (char *)&aligned_source_buffer;
	/* Return the space saved for command strings */
	POP_MV_STENT();
	ctxt = active_ch = chnd;		/* Clear extra condition handlers added by dm_start()s */
	assert(save_msp == msp);
}
