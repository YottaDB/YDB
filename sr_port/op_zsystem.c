/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include <sys/wait.h>
#include "gtm_stdlib.h"
#include "gtm_string.h"

#include "gdsroot.h"
#include "gdskill.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "filestruct.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "io.h"
#include "iott_setterm.h"
#include "jnl.h"
#include "tp.h"
#include "send_msg.h"
#include "gtmmsg.h"		/* for gtm_putmsg() prototype */
#include "op.h"
#include "change_reg.h"
#include "getzposition.h"
#ifdef DEBUG
#include "have_crit.h"		/* for the TPNOTACID_CHECK macro */
#endif
#include "iottdef.h"
#include "ydb_getenv.h"
#include "gtmxc_types.h"
#include "gtm_filter_command.h"
#include "restrict.h"

#define	ZSYSTEMSTR	"ZSYSTEM"
/* If command buffer was allocated (which will be only when a non NULL command came
 * in), free it before returning
 */
#define CHECK_N_FREE				\
if (v->str.len)					\
	free(cmd_buf);				\

GBLREF	uint4		dollar_trestart;
GBLREF	int4		dollar_zsystem;			/* exit status of child */
GBLREF	io_pair		io_std_device;
GBLREF	uint4           trust;

error_def(ERR_INVSTRLEN);
error_def(ERR_RESTRICTEDOP);
error_def(ERR_SYSCALL);
error_def(ERR_COMMFILTERERR);

void op_zsystem(mval *v)
{
	char		*cmd_buf = NULL;
#ifdef _BSD
        union wait      wait_stat;
#else
        int4            wait_stat;
#endif
	ydb_string_t	filtered_command;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (RESTRICTED(zsystem_op))
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_RESTRICTEDOP, 1, ZSYSTEMSTR);
	TPNOTACID_CHECK(ZSYSTEMSTR);
	MV_FORCE_STR(v);
	flush_pio();
<<<<<<< HEAD
=======
	if (io_std_device.in->type == tt)
		iott_resetterm(io_std_device.in);
>>>>>>> 3c1c09f2 (GT.M V7.1-001)
	if (v->str.len)
	{
		/* Copy the command to a new buffer and append a '\0' */
		cmd_buf = (char*)malloc(v->str.len + 1);
		memcpy(cmd_buf, v->str.addr, v->str.len);
		cmd_buf[v->str.len] = '\0';
	} else	/* No command , get the shell from env */
	{
		cmd_buf = ydb_getenv(YDBENVINDX_GENERIC_SHELL, NULL_SUFFIX, NULL_IS_YDB_ENV_MATCH);
		cmd_buf = (NULL == cmd_buf || '\0' == *cmd_buf) ? "/bin/sh" : cmd_buf;
	}
	/*Filter the command first, if required*/
	if (RESTRICTED(zsy_filter))
	{
		filtered_command = gtm_filter_command(cmd_buf, ZSYSTEMSTR);
		if (filtered_command.length)
		{
			if (!strlen(filtered_command.address)) /*empty command returned*/
			{
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_COMMFILTERERR, 4, LEN_AND_LIT(ZSYSTEMSTR),
								LEN_AND_LIT("Empty return command"));
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_COMMFILTERERR, 4, LEN_AND_LIT(ZSYSTEMSTR),
								LEN_AND_LIT("Empty return command"));
				CHECK_N_FREE;
				return;
			}
			/* If a command argument came in and command buffer allocated above,
			 * free the buffer and copy the filtered command into it, else
			 * just assign the buffer to the returned filtered command.
			 */
			if (v->str.len)
			{	/* Free original cmd, malloc a replacement; CHECK_N_FREE uses
				 * v->str.len as flag to later free it.
				 */
				free(cmd_buf);
				cmd_buf = (char*)malloc(filtered_command.length + 1);
				memcpy(cmd_buf, filtered_command.address, filtered_command.length);
				cmd_buf[filtered_command.length] = '\0';
			} else
				cmd_buf = filtered_command.address;
		} else /* Error case */
		{
			CHECK_N_FREE;
			return; /* Error would have been printed in the filter execution routine*/
		}
	}

	dollar_zsystem = SYSTEM(cmd_buf);
	CHECK_N_FREE;
	if (-1 == dollar_zsystem)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("system"), CALLFROM, errno);
#ifdef _BSD
	assert(SIZEOF(wait_stat) == SIZEOF(int4));
	wait_stat.w_status = dollar_zsystem;
#else
	wait_stat = dollar_zsystem;
#endif
	if (WIFEXITED(wait_stat))
		dollar_zsystem = WEXITSTATUS(wait_stat);
<<<<<<< HEAD
=======
	if (io_std_device.in->type == tt)
		iott_setterm(io_std_device.in);
>>>>>>> 3c1c09f2 (GT.M V7.1-001)
	return;
}
