/****************************************************************
 *								*
 * Copyright (c) 2018-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2023 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include "gtm_filter_command.h"
#include "gtm_string.h"

#include "gtmmsg.h"
#include "send_msg.h"
#include "gtm_threadgbl.h"
#include "gtm_threadgbl_init.h"
#include "gtmci.h"
#include "stack_frame.h"
#include "mprof.h"
#include "util.h"

error_def(ERR_COMMFILTERERR);
error_def(ERR_RESTRICTEDOP);
error_def(ERR_NOFILTERNEST);

GBLREF stack_frame      *frame_pointer;
GBLREF boolean_t	is_tracing_on;

#define CALL_ROUTINE_N_CHECK_ERR(ROUTINE, COMMAND, RET_COMM)							\
{														\
	ydb_status_t	stat;											\
	ydb_long_t	quit_return;										\
	ydb_char_t	err_str[2 * OUT_BUFF_SIZE]; /* Same as Max buffer for zstatus */			\
	stat = gtm_ci_filter(ROUTINE, &quit_return, COMMAND, &RET_COMM);					\
	if (quit_return == -1)											\
	{													\
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_COMMFILTERERR, 4, LEN_AND_LIT(ROUTINE),		\
							LEN_AND_LIT("RESTRICTEDOP"));				\
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_RESTRICTEDOP, 1, COMMAND);			\
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_COMMFILTERERR, 4, LEN_AND_LIT(ROUTINE), 		\
							LEN_AND_LIT("RESTRICTEDOP"));				\
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_RESTRICTEDOP, 1, COMMAND);				\
		returned_command.length = 0;									\
	}													\
	if (stat)												\
	{													\
		ydb_zstatus(&err_str[0], 2 * OUT_BUFF_SIZE);							\
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_COMMFILTERERR, 4,					\
							LEN_AND_LIT(ROUTINE), LEN_AND_STR(err_str));		\
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_COMMFILTERERR, 4, LEN_AND_LIT(ROUTINE), 		\
							LEN_AND_STR(err_str));					\
		returned_command.length = 0;									\
		if (frame_pointer->type & SFT_CI)								\
			ci_ret_code_quit();									\
	}													\
}

ydb_string_t gtm_filter_command(char * command, char * caller_name)
{
	ydb_char_t	com_str[MAX_STRLEN];
	ydb_string_t 	returned_command;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;

	returned_command.address = (ydb_char_t *)&com_str;
	returned_command.length = MAX_STRLEN;

	if (TREF(comm_filter_init)) /* Already in a filter, no nesting allowed */
	{
		TREF(comm_filter_init) = FALSE;
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOFILTERNEST);
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_NOFILTERNEST);
	}
	if (!STRCMP(caller_name, "PIPE"))
		CALL_ROUTINE_N_CHECK_ERR("gtmpipeopen", command, returned_command);
	if (!STRCMP(caller_name, "ZSYSTEM"))
		CALL_ROUTINE_N_CHECK_ERR("gtmzsystem", command, returned_command);
	if (is_tracing_on)
		TREF(prof_fp)	= TREF(mprof_stack_curr_frame);
	return returned_command;

}
