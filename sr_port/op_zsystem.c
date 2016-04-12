/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information	*
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
#ifdef VMS
#include <descrip.h>
#include <ssdef.h>
#else
#include <sys/wait.h>
#endif
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
	int		len;
#ifdef UNIX
	char		*sh, cmd_buf[MAXZSYSSTRLEN + 1], *cmd;
#ifdef _BSD
        union wait      wait_stat;
#else
        int4            wait_stat;
#endif
#elif defined VMS
	uint4		status;
	$DESCRIPTOR(d_cmd,"");
#else
#error UNSUPPORTED PLATFORM
#endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	TPNOTACID_CHECK(ZSYSTEMSTR);
	MV_FORCE_STR(v);
#ifdef UNIX
	/* get SHELL environment */
	sh = GETENV("SHELL");
	len = ((sh)? STRLEN(sh):STRLEN("/bin/sh")) + STRLEN(" -c ''"); /* Include the command " -c ''" string */
	if (v->str.len > (MAXZSYSSTRLEN - len))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_INVSTRLEN, 2, v->str.len,
				(MAXZSYSSTRLEN > len)? (MAXZSYSSTRLEN - len) : 0);
	/* use bourn shell as default */
	if (!sh)
		strncpy(cmd_buf, "/bin/sh", STRLEN("/bin/sh"));
	else
		strncpy(cmd_buf, sh, len); /* sh is null terminated, using len is ok even though it's longer */
	cmd = cmd_buf;
	if (v->str.len)
	{
		strcat(cmd, " -c '");
		len = STRLEN(cmd);
		memcpy(cmd + len, v->str.addr, v->str.len);
		*(cmd + len + v->str.len) = 39; /* ' = 39 */
		*(cmd + len + v->str.len + 1) = 0;
	}
#elif defined VMS
	MVAL_TO_DSC(v, d_cmd);
#endif
	flush_pio();
	if (io_std_device.in->type == tt)
		resetterm(io_std_device.in);
#ifdef UNIX
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
#elif defined VMS
	status = spawn_and_bgwait(&d_cmd, 0, 0, &trust, 0, 0, &dollar_zsystem);
#endif
	if (io_std_device.in->type == tt)
		setterm(io_std_device.in);
#ifdef VMS
	if (status != SS$_NORMAL)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
#endif
	return;
}
