/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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

int gtmrecv_statslog(void)
{

#ifdef VMS
	error_def(ERR_UNIMPLOP);
	error_def(ERR_TEXT);

	rts_error(VARLSTCNT(6) ERR_UNIMPLOP, 0, ERR_TEXT, 2, LEN_AND_LIT("Statistics logging not supported on VMS"));
#endif
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
		recvpool.gtmrecv_local->statslog_file[0] = '\0';
		util_out_print("STATSLOG turned OFF", TRUE);
		rel_sem_immediate(RECV, RECV_SERV_OPTIONS_SEM);
		return (NORMAL_SHUTDOWN);
	}

	if ('\0' == gtmrecv_options.log_file[0]) /* Stats log file not specified, use general log file */
	{
		util_out_print("No file specified for stats log. Using general log file !AD\n", TRUE,
				LEN_AND_STR(recvpool.gtmrecv_local->log_file));
		strcpy(gtmrecv_options.log_file, recvpool.gtmrecv_local->log_file);
	} else if (0 == strcmp(recvpool.gtmrecv_local->log_file, gtmrecv_options.log_file))
	{
		util_out_print("Stats log file is already !AD. Not initiating change in log file", TRUE,
				LEN_AND_STR(gtmrecv_options.log_file));
		rel_sem_immediate(RECV, RECV_SERV_OPTIONS_SEM);
		return (ABNORMAL_SHUTDOWN);
	}

	strcpy(recvpool.gtmrecv_local->statslog_file, gtmrecv_options.log_file);
	recvpool.gtmrecv_local->statslog = TRUE;

	util_out_print("Stats log turned on with file !AD", TRUE, strlen(recvpool.gtmrecv_local->statslog_file),
			recvpool.gtmrecv_local->statslog_file);

	rel_sem_immediate(RECV, RECV_SERV_OPTIONS_SEM);
	return (NORMAL_SHUTDOWN);
}
