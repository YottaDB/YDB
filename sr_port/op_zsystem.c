/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef VMS
#include <descrip.h>
#include <ssdef.h>
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
#include "hashtab.h"		/* needed for tp.h */
#include "io.h"
#include "jnl.h"
#include "tp.h"
#include "send_msg.h"
#include "gtmmsg.h"		/* for gtm_putmsg() prototype */
#include "op.h"
#include "change_reg.h"
#include "setterm.h"
#include "getzposition.h"

GBLREF int4		dollar_zsystem;			/* exit status of child */
GBLREF short		dollar_tlevel;
GBLREF io_pair		io_std_device;
GBLREF unsigned int	t_tries;
GBLREF tp_region	*tp_reg_list;
GBLREF uint4            trust;

void op_zsystem(mval *v)
{
	int		len;
	mval		zpos;
	sgmnt_addrs	*csa;
	tp_region	*tr;
#ifdef UNIX
	char		*sh, cmd_buf[238], *cmd;
#elif defined VMS
	uint4		status;
	$DESCRIPTOR(d_cmd,"");
#else
#error UNSUPPORTED PLATFORM
#endif
	error_def(ERR_INVSTRLEN);
	error_def(ERR_TPNOTACID);

	if (0 < dollar_tlevel)
	{	/* Note the existence of similar code in op_dmode.c and mdb_condition_handler.c.
		 * Any changes here should be reflected there too. We don't have a macro for this because
		 * 	(a) This code is considered pretty much stable.
		 * 	(b) Making it a macro makes it less readable.
		 */
		for (tr = tp_reg_list;  NULL != tr;  tr = tr->fPtr)
		{	/* the mdb_condition_handler does all regions in all global directories
			 * this should produce the same result more quickly but the difference should be noted */
			csa = (sgmnt_addrs *)&FILE_INFO(tr->reg)->s_addrs;
			if (csa->now_crit)
				rel_crit(tr->reg);
		}
		if (CDB_STAGNATE <= t_tries)
		{
			assert(CDB_STAGNATE == t_tries);
			t_tries = CDB_STAGNATE - 1;
			getzposition(&zpos);
			gtm_putmsg(VARLSTCNT(4) ERR_TPNOTACID, 2, zpos.str.len, zpos.str.addr);
			send_msg(VARLSTCNT(4) ERR_TPNOTACID, 2, zpos.str.len, zpos.str.addr);
		}
	}
	MV_FORCE_STR(v);
#ifdef UNIX
	if (v->str.len > 205) /* 32 char for shell name, 206 for ZSYSTEM command */
		rts_error(VARLSTCNT(4) ERR_INVSTRLEN, 2, v->str.len, 205);
	/* get SHELL environment */
	sh = GETENV("SHELL");
	/* use bourn shell as default */
	if (!sh)
		strcpy(cmd_buf, "/bin/sh");
	else
		strcpy(cmd_buf, sh);
	cmd = cmd_buf;
	if (v->str.len)
	{
		strcat(cmd, " -c '");
		len = strlen(cmd);
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
#elif defined VMS
	status = spawn_and_bgwait(&d_cmd, 0, 0, &trust, 0, 0, &dollar_zsystem);
#endif
	if (io_std_device.in->type == tt)
		setterm(io_std_device.in);
#ifdef VMS
	if (status != SS$_NORMAL)
		rts_error(VARLSTCNT(1) status);
#endif
	return;
}
