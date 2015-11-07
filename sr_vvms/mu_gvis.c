/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "msg.h"
#include "format_targ_key.h"
#include "mu_gvis.h"

GBLREF gv_key	*gv_currkey;

void mu_gvis(void)
{
	char		key_buff[MAX_ZWR_KEY_SZ], *key_end;
	msgtype		msg;

	error_def(ERR_GVIS);

	msg.arg_cnt = 4;
	msg.new_opts = msg.def_opts = 1;
	msg.msg_number = ERR_GVIS;
	msg.fp_cnt = 2;
	if (gv_currkey->end)
	{
		if ((key_end = format_targ_key(&key_buff[0], MAX_ZWR_KEY_SZ, gv_currkey, TRUE)) == 0)
			key_end = &key_buff[MAX_ZWR_KEY_SZ - 1];
	} else
		key_end = &key_buff[0];
	msg.fp[0].n = key_end - key_buff;
	msg.fp[1].cp = &key_buff[0];
	sys$putmsg(&msg,0,0,0);
}
