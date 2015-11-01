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

#include <sys/time.h>
#include <errno.h>
#include "gtm_string.h"
#include <arpa/inet.h>
#ifdef UNIX
#include <sys/sem.h>
#endif
#ifdef VMS
#include <descrip.h> /* Required for gtmrecv.h */
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

GBLREF	recvpool_addrs		recvpool;
GBLREF	gtmrecv_options_t	gtmrecv_options;

int gtmrecv_changelog(void)
{
	/* Grab the recvpool jnlpool option write lock */
	if (0 > grab_sem(RECV, RECV_SERV_OPTIONS_SEM))
	{
		util_out_print("Error grabbing recvpool option write lock. Could not initiate change log", TRUE);
		return (ABNORMAL_SHUTDOWN);
	}

	if (0 == strcmp(recvpool.gtmrecv_local->log_file, gtmrecv_options.log_file))
	{
		util_out_print("Log file is already !AD. Not initiating change in log file", TRUE,
				LEN_AND_STR(gtmrecv_options.log_file));
		return (ABNORMAL_SHUTDOWN);
	}

	if (recvpool.gtmrecv_local->changelog)
	{
		util_out_print("Change log is already in progress to !AD. Not initiating change in log file", TRUE,
				LEN_AND_STR(recvpool.gtmrecv_local->log_file));
		return (ABNORMAL_SHUTDOWN);
	}

	strcpy(recvpool.gtmrecv_local->log_file, gtmrecv_options.log_file);
	recvpool.gtmrecv_local->changelog = TRUE;

	util_out_print("Change log initiated with file !AD", TRUE,
			LEN_AND_STR(recvpool.gtmrecv_local->log_file));

	/* Release the recvpool jnlpool option write lock */
	rel_sem(RECV, RECV_SERV_OPTIONS_SEM);
	return (NORMAL_SHUTDOWN);
}
