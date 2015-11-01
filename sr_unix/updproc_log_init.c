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
#include "gtm_string.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtmrecv.h"
#include "iosp.h"
#include "repl_log.h"
#include "repl_dbg.h"
#include "gtm_stdio.h"
#include "gtm_event_log.h"
#include "updproc.h"

#define UPDPROC_LOG_FILE_SUFFIX	".updproc"

GBLREF	int		updproc_log_fd;
GBLREF 	FILE		*updproc_log_fp;
GBLREF	recvpool_addrs	recvpool;

int updproc_log_init(void)
{
	char		log_file[MAX_FN_LEN + 1];
	int		status = SS_NORMAL;

	strcpy(log_file, recvpool.gtmrecv_local->log_file);
	if (strncmp(log_file, DEVICE_PREFIX, sizeof(DEVICE_PREFIX) - 1) != 0)
		strcat(log_file, UPDPROC_LOG_FILE_SUFFIX);

	if (strcmp(log_file, recvpool.upd_proc_local->log_file) != 0 || updproc_log_fd == -1)
	{
		strcpy(recvpool.upd_proc_local->log_file, log_file);
		status = repl_log_init(REPL_GENERAL_LOG, &updproc_log_fd, NULL, recvpool.upd_proc_local->log_file, NULL);
		repl_log_fd2fp(&updproc_log_fp, updproc_log_fd);
		gtm_event_log_init();
	}

	return(status);
}
