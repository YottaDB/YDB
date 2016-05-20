/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
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
#ifdef DEBUG
#include "have_crit.h"		/* for the TPNOTACID_CHECK macro */
#endif

#define	ZSYSTEMSTR	"ZSYSTEM"
#define	MAXZSYSSTRLEN	4096	/* maximum command line length supported by most Unix shells */

GBLREF	uint4		dollar_trestart;
GBLREF	int4		dollar_zsystem;			/* exit status of child */
GBLREF	io_pair		io_std_device;
GBLREF	uint4           trust;

error_def(ERR_INVSTRLEN);
error_def(ERR_SYSCALL);

void op_zsystem(mval *v)
{
	int		len, shlen;
	char		*sh, cmd_buf[MAXZSYSSTRLEN + 1], *cmd;
#ifdef _BSD
        union wait      wait_stat;
#else
        int4            wait_stat;
#endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	TPNOTACID_CHECK(ZSYSTEMSTR);
	MV_FORCE_STR(v);
	/* get SHELL environment */
	sh = GETENV("SHELL");
	shlen = (NULL == sh)? 0 : STRLEN(sh);
	len = (0 < shlen)? shlen : STRLEN("/bin/sh");
	/* Include " -c ''" in the total string length */
	len += STRLEN(" -c ''");
	if ((v->str.len + len) > MAXZSYSSTRLEN)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_INVSTRLEN, 2, v->str.len,
				(MAXZSYSSTRLEN > len)? (MAXZSYSSTRLEN - len) : 0);
	cmd = cmd_buf;
	if (v->str.len)
	{
		/* Use bourne shell as default */
		if (0 < shlen)
			SNPRINTF(cmd_buf, MAXZSYSSTRLEN, "%s -c '", sh);
		else
			STRNCPY_LIT_FULL(cmd_buf, "/bin/sh -c '");
		len = STRLEN(cmd);
		memcpy(cmd + len, v->str.addr, v->str.len);
		*(cmd + len + v->str.len) = 39; /* ' = 39 */
		*(cmd + len + v->str.len + 1) = 0;
	} else
	{
		if (0 < shlen)
			SNPRINTF(cmd_buf, MAXZSYSSTRLEN, "%s", sh);
		else
			STRNCPY_LIT_FULL(cmd_buf, "/bin/sh");
	}
	flush_pio();
	if (io_std_device.in->type == tt)
		resetterm(io_std_device.in);
	dollar_zsystem = SYSTEM(cmd);
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
