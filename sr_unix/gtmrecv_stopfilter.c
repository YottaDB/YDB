/****************************************************************
 *								*
 * Copyright (c) 2019-2025 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include "gtm_unistd.h"
#include "gtm_stdio.h"
#include "gtm_fcntl.h"
#include "gtm_inet.h"
#include "gtm_ipc.h"
#include <sys/wait.h>
#include <errno.h>
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdskill.h"
#include "gdscc.h"
#include "filestruct.h"
#include "iosp.h"
#include "repl_shutdcode.h"
#include "repl_log.h"
#include "util.h"

#include <gtmrecv.h>

GBLREF	recvpool_addrs		recvpool;
int gtmrecv_stopfilter(void)
{
	repl_log(stderr, TRUE, TRUE,
		"Initiating STOPRECEIVERFILTER operation on receiver server pid [%d]\n", recvpool.gtmrecv_local->recv_serv_pid);
	if ('\0' == recvpool.gtmrecv_local->filter_cmd[0])
	{
		 util_out_print("No filter currently active", TRUE);
		return (ABNORMAL_SHUTDOWN);
	}
	recvpool.gtmrecv_local->filter_cmd[0] = '\0';
	recvpool.gtmrecv_local->recv_filter_pid = 0;
	util_out_print("Stop filter initiated", TRUE);
	return (NORMAL_SHUTDOWN);

}
