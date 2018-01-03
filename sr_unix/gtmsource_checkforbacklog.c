/****************************************************************
 *								*
 * Copyright (c) 2006-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdio.h"

#if !defined(__MVS__) && !defined(VMS)
#include <sys/param.h>
#endif

#include <sys/time.h>
#include "gtm_inet.h"
#include <errno.h>

#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "error.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_dbg.h"
#include "repl_shutdcode.h"
#include "repl_sem.h"
#include "util.h"
#include "repl_log.h"
#include "is_proc_alive.h"

GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	gtmsource_options_t	gtmsource_options;


seq_num gtmsource_checkforbacklog(void)
{
	seq_num			seq_num, jnl_seqno, read_jnl_seqno, backlog_count = 0;
	gtmsource_local_ptr_t	gtmsourcelocal_ptr;
	int4			index;
	boolean_t		srv_alive;
	uint4			gtmsource_pid;

	jnl_seqno = jnlpool->jnlpool_ctl->jnl_seqno;
	if (NULL != jnlpool->gtmsource_local)	/* Show backlog for a specific source server */
		gtmsourcelocal_ptr = jnlpool->gtmsource_local;
	else
		gtmsourcelocal_ptr = &jnlpool->gtmsource_local_array[0];
	for (index = 0; index < NUM_GTMSRC_LCL; index++, gtmsourcelocal_ptr++)
	{
		if ('\0' == gtmsourcelocal_ptr->secondary_instname[0])
		{
			assert(NULL == jnlpool->gtmsource_local);
			continue;
		}
		gtmsource_pid = gtmsourcelocal_ptr->gtmsource_pid;
		if ((NULL == jnlpool->gtmsource_local) && (0 == gtmsource_pid))
			continue;
		read_jnl_seqno = gtmsourcelocal_ptr->read_jnl_seqno;
		/* jnl_seqno >= read_jnl_seqno is the most common case; see gtmsource_readpool() for when the rare case can occur */
		seq_num = (jnl_seqno >= read_jnl_seqno) ? jnl_seqno - read_jnl_seqno : 0;
		backlog_count += seq_num;
		if (NULL != jnlpool->gtmsource_local)
			break;
	}
	return backlog_count;
}
