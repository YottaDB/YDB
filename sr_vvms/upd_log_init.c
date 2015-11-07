/****************************************************************
 *								*
 *	Copyright 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
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
#include "iosp.h"
#include "repl_log.h"
#include "repl_dbg.h"
#include "gtm_stdio.h"
#include "util.h"
#include "gtm_event_log.h"
#include "read_db_files_from_gld.h"
#include "updproc.h"

#define UPDPROC_LOG_FILE_SUFFIX			"_updproc"
#define UPDHELPER_READER_LOG_FILE_SUFFIX	"_uhr_"
#define UPDHELPER_WRITER_LOG_FILE_SUFFIX	"_uhw_"

GBLREF	recvpool_addrs	recvpool;
GBLREF	uint4		process_id;

int upd_log_init(recvpool_user who)
{
	char		log_file[MAX_FN_LEN + 1], file_suffix_str[MAX_FN_LEN + 1], pid_str[9], *file_suffix;
	int		status = SS_NORMAL, len;

	strcpy(log_file, recvpool.gtmrecv_local->log_file);
	if (UPDPROC == who)
		file_suffix = UPDPROC_LOG_FILE_SUFFIX;
	else
	{
		if (UPD_HELPER_READER == who)
			strcpy(file_suffix_str, UPDHELPER_READER_LOG_FILE_SUFFIX);
		else /* UPD_HELPER_WRITER == who */
			strcpy(file_suffix_str, UPDHELPER_WRITER_LOG_FILE_SUFFIX);
		i2hex(process_id, pid_str, 8);
		pid_str[8] = '\0';
		strcat(file_suffix_str, pid_str);
		file_suffix = file_suffix_str;
	}
	strcat(log_file, file_suffix);
	len = strlen(log_file);
	if (!util_is_log_open() || UPDPROC != who || 0 != memcmp(log_file, recvpool.upd_proc_local->log_file, len))
	{
		util_log_open(log_file, len);
		if (UPDPROC == who)
		{
			memcpy(recvpool.upd_proc_local->log_file, log_file, len+1); /* +1 for '\0' */
			gtm_event_log_init();
		}
	}
	return(status);
}
