/****************************************************************
 *								*
 *	Copyright 2005 Fidelity Information Services, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_inet.h"
#ifdef VMS
#include <descrip.h>
#endif

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtmrecv.h"

GBLREF recvpool_addrs	recvpool;
GBLREF seq_num		lastlog_seqno;
GBLREF uint4		log_interval;
GBLREF qw_num		trans_recvd_cnt, last_log_tr_recvd_cnt;

void gtmrecv_reinit_logseqno(void)
{
	lastlog_seqno = recvpool.recvpool_ctl->jnl_seqno - log_interval;
	trans_recvd_cnt = -(qw_num)(log_interval - 1);
	last_log_tr_recvd_cnt = 0;
}
