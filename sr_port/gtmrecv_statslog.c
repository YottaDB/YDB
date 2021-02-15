/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#if !defined(__MVS__) && !defined(VMS)
#include <sys/param.h>
#endif
#include <sys/time.h>
#include <errno.h>
#include "gtm_string.h"
#include "gtm_inet.h"
#ifdef UNIX
#include <sys/sem.h>
#endif

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtmrecv.h"
#include "repl_dbg.h"
#include "repl_shutdcode.h"
#include "repl_sem.h"
#include "util.h"

#include "gtm_fcntl.h"
#include "gtmio.h"
#include "repl_log.h"
#include "repl_sp.h"

GBLREF	recvpool_addrs		recvpool;
GBLREF	gtmrecv_options_t	gtmrecv_options;
error_def(ERR_REPLLOGOPN);
int gtmrecv_statslog(void)
{
	/* Grab the recvpool option write lock */
	if (0 > grab_sem(RECV, RECV_SERV_OPTIONS_SEM))
	{
		util_out_print("Error grabbing recvpool option write lock. Could not initiate stats log", TRUE);
		return (ABNORMAL_SHUTDOWN);
	}

	if (gtmrecv_options.statslog == recvpool.gtmrecv_local->statslog)
	{
		util_out_print("STATSLOG is already !AD. Not initiating change in stats log", TRUE, gtmrecv_options.statslog ?
				strlen("ON") : strlen("OFF"), gtmrecv_options.statslog ? "ON" : "OFF");
		rel_sem_immediate(RECV, RECV_SERV_OPTIONS_SEM);
		return (ABNORMAL_SHUTDOWN);
	}

	if (!gtmrecv_options.statslog)
	{
		recvpool.gtmrecv_local->statslog = FALSE;
		util_out_print("STATSLOG turned OFF", TRUE);
		rel_sem_immediate(RECV, RECV_SERV_OPTIONS_SEM);
		return (NORMAL_SHUTDOWN);
	}

	recvpool.gtmrecv_local->statslog = TRUE;
	util_out_print("Stats log turned on", TRUE);
	rel_sem_immediate(RECV, RECV_SERV_OPTIONS_SEM);
	return (NORMAL_SHUTDOWN);
}
