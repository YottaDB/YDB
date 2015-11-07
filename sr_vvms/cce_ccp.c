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
#include <prvdef.h>
#include <ssdef.h>
#include <descrip.h>
#include <dvidef.h>
#include <jpidef.h>
#include <efndef.h>

#include "vmsdtype.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "ccp.h"
#include "ccpact.h"

GBLREF	uint4	ret_status;

cce_ccp()
{
	struct
	{
		item_list_3	item[2];
		int4		terminator;
	} item_list;
	uint4		status;
	char			out_str[15];
	$DESCRIPTOR(out_desc,out_str);
	unsigned short		out_len;
	short			dvisb[4];
	int4			refnum, pid, pidadr;
	short			dummy;
	int4			prvadr[2], prvprv[2];
	int4			iosb[2];
	short ccp_channel;
	error_def(ERR_CCENOCCP);
	error_def(ERR_CCECCPPID);
	error_def(ERR_CCECLSTPRCS);
	error_def(ERR_CCENOWORLD);
	error_def(ERR_CCENOGROUP);

	lib$establish(cce_ccp_ch);
	ccp_channel = ccp_sendmsg(CCTR_NULL,0);
	lib$revert();
	if (!ccp_channel)
	{
		ret_status = ERR_CCENOCCP;
		lib$signal(ERR_CCENOCCP);
		return;
	}
	prvadr[0] = PRV$M_WORLD;
	prvadr[1] = 0;
	status = sys$setprv(TRUE, &prvadr[0], FALSE, &prvprv[0]);
	if (status != SS$_NORMAL)
	{	lib$signal(ERR_CCENOWORLD);
		prvadr[0] = PRV$M_GROUP;
		prvadr[1] = 0;
		status = sys$setprv(TRUE, &prvadr[0], FALSE, &prvprv[0]);
		if (status != SS$_NORMAL)
		{	lib$signal(ERR_CCENOGROUP);
		}
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
		{	lib$signal(ERR_CCECCPPID,1,pid);
			break;
		}
		if (status == SS$_NOMOREPROC)
			break;
	}
	refnum = 0;
	memset(&item_list, 0, SIZEOF(item_list));
	item_list.item[0].buffer_length		= SIZEOF(refnum);
	item_list.item[0].buffer_address	= &refnum;
	item_list.item[0].item_code		= DVI$_REFCNT;
	item_list.item[0].return_length_address = &dummy;
	status = sys$getdviw (0, ccp_channel, 0, &item_list, &dvisb, 0, 0, 0);
	if (status != SS$_NORMAL)
	{	lib$signal(status);
		if (prvprv[0] & prvadr[0] == 0)
		{	sys$setprv(FALSE, &prvadr[0], FALSE, 0);
		}
		return;
	}
	if (pid)
	{	lib$signal(ERR_CCECLSTPRCS,1,refnum - 2);
	}else
	{	lib$signal(ERR_CCECLSTPRCS,1,refnum - 1);
	}
	if (prvprv[0] & prvadr[0] == 0)
	{	sys$setprv(FALSE, &prvadr[0], FALSE, 0);
	}
	return;
}
