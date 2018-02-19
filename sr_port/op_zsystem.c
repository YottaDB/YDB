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
#include "jnl.h"
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "send_msg.h"
#include "gtmmsg.h"		/* for gtm_putmsg() prototype */
#include "op.h"
#include "change_reg.h"
#include "setterm.h"
#include "getzposition.h"
#include "restrict.h"
#ifdef DEBUG
#include "have_crit.h"		/* for the TPNOTACID_CHECK macro */
#endif

#define	ZSYSTEMSTR	"ZSYSTEM"

GBLREF	uint4		dollar_trestart;
GBLREF	int4		dollar_zsystem;			/* exit status of child */
GBLREF	io_pair		io_std_device;
GBLREF	uint4           trust;

error_def(ERR_INVSTRLEN);
error_def(ERR_RESTRICTEDOP);
error_def(ERR_SYSCALL);

void op_zsystem(mval *v)
{
	char		*cmd_buf = NULL;
#ifdef _BSD
        union wait      wait_stat;
#else
        int4            wait_stat;
#endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (RESTRICTED(zsystem_op))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_RESTRICTEDOP, 1, "ZSYSTEM");
	TPNOTACID_CHECK(ZSYSTEMSTR);
	MV_FORCE_STR(v);
	flush_pio();
	if (io_std_device.in->type == tt)
		resetterm(io_std_device.in);
	if (v->str.len)
	{
		/* Copy the commander to a new buffer and append a '\0' */
		cmd_buf = (char*)malloc(v->str.len+1);
		memcpy(cmd_buf, v->str.addr, v->str.len);
		cmd_buf[v->str.len] = '\0';
	} else
	{
		cmd_buf = GETENV("SHELL");
		cmd_buf = (NULL == cmd_buf || '\0' == *cmd_buf) ? "/bin/sh" : cmd_buf;
	}
	dollar_zsystem = SYSTEM(cmd_buf);
	if (v->str.len)
		free(cmd_buf);
	if (-1 == dollar_zsystem)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("system"), CALLFROM, errno);
#ifdef _BSD
	assert(SIZEOF(wait_stat) == SIZEOF(int4));
	wait_stat.w_status = dollar_zsystem;
#else
	wait_stat = dollar_zsystem;
#endif
	if (WIFEXITED(wait_stat))
		dollar_zsystem = WEXITSTATUS(wait_stat);
	if (io_std_device.in->type == tt)
		setterm(io_std_device.in);
	return;
}
