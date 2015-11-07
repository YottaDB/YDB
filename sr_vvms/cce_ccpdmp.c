/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <ssdef.h>
#include <prvdef.h>
#include "vmsdtype.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "ccp.h"
#include "ccpact.h"
#include <jpidef.h>
#include <efndef.h>

#include "cli.h"

void cce_ccpdmp(void)
{

	struct
	{
		item_list_3	item[2];
		int4		terminator;
	} item_list;
	short		ccp_channel, dummy, out_len, iosb[4];
	int4		prvadr[2], prvprv[2], pid, pidadr, status;
	char		out_str[15];
	uint4	dumpon,dumpnow;
	int4		error;
	error_def(ERR_CCEDUMPNOW);
	error_def(ERR_CCEDUMPON);
	error_def(ERR_CCEDUMPOFF);
	error_def(ERR_CCENOCCP);
	error_def(ERR_NOCCPPID);
	error_def(ERR_CCEDMPQUALREQ);

	if (cli_present("DB") == CLI_PRESENT)
	{	cce_dbdump();
		return;
	}
	lib$establish(cce_ccp_ch);
	ccp_channel = ccp_sendmsg(CCTR_NULL,0);
	lib$revert();
	if (!ccp_channel)
	{	rts_error(VARLSTCNT(1) ERR_CCENOCCP);
		return;
	}
	dumpnow = cli_present("NOW");
	if (dumpnow != CLI_PRESENT && ((dumpon = cli_present("ON")) != CLI_PRESENT) && (dumpon != CLI_NEGATED))
	{	rts_error(VARLSTCNT(1) ERR_CCEDMPQUALREQ);
		return;
	}
	if (dumpon == CLI_NEGATED)
		dumpon = FALSE;
	prvadr[0] = PRV$M_WORLD;
	prvadr[1] = 0;
	status = sys$setprv(TRUE, &prvadr[0], FALSE, &prvprv[0]);
	if (status != SS$_NORMAL)
	{	prvadr[0] = PRV$M_GROUP;
		prvadr[1] = 0;
		sys$setprv(TRUE, &prvadr[0], FALSE, &prvprv[0]);
	}
	item_list.item[0].buffer_length		= SIZEOF(pid);
	item_list.item[0].buffer_address	= &pid;
	item_list.item[0].item_code		= JPI$_PID;
	item_list.item[0].return_length_address = &dummy;
	item_list.item[1].buffer_length		= 15;
	item_list.item[1].buffer_address	= out_str;
	item_list.item[1].item_code		= JPI$_PRCNAM;
	item_list.item[1].return_length_address = &out_len;
	item_list.terminator = 0;
	pidadr = -1;
	for (; ;)
	{	status = sys$getjpiw(EFN$C_ENF,&pidadr,0,&item_list,&iosb,0,0);
		if (status == SS$_NORMAL && out_len == (SIZEOF(CCP_PRC_NAME) - 1)
				&& memcmp(&out_str[0],CCP_PRC_NAME,(SIZEOF(CCP_PRC_NAME) - 1)) == 0)
			break;
		if (status == SS$_NOMOREPROC)
		{	rts_error(VARLSTCNT(1) ERR_NOCCPPID);
			return;
		}
	}
	if (dumpnow == CLI_PRESENT)
		error = ERR_CCEDUMPNOW;
	else if (dumpon == CLI_PRESENT)
		error = ERR_CCEDUMPON;
	else
		error = ERR_CCEDUMPOFF;
	status = sys$forcex(&pid,0,error);
	if (status != SS$_NORMAL)
	{	rts_error(VARLSTCNT(1) status);
		return;
	}
	return;
}
