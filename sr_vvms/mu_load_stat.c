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
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "msg.h"
#include "mu_load_stat.h"

GBLREF bool mu_ctrlc_occurred;

mu_load_stat(uint4 max_data_len, uint4 max_subsc_len, uint4 key_count, uint4 rec_count, uint4 stat_type)
{
	static readonly unsigned char gt_lit[] = "LOAD TOTAL";
	msgtype		*msg;
	error_def(ERR_STATCNT);

	msg = malloc(SIZEOF(msgtype) + FAO_ARG);
	msg->arg_cnt = 7;
	msg->new_opts = msg->def_opts = 1;
	msg->msg_number = ERR_STATCNT;
	msg->fp_cnt = 5;
	msg->fp[0].n = SIZEOF(gt_lit) - 1;
	msg->fp[1].cp = gt_lit;
	msg->fp[2].n = key_count;
	msg->fp[3].n = max_subsc_len;
	msg->fp[4].n = max_data_len;
	sys$putmsg(msg,0,0,0);

	msg->msg_number = stat_type;
	msg->arg_cnt = 3;
	msg->fp_cnt = 1;
	msg->fp[0].n = rec_count;
	sys$putmsg(msg,0,0,0);

	mu_ctrlc_occurred = FALSE;
	free (msg);
}
