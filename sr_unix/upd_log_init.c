/****************************************************************
 *								*
 *	Copyright 2005, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

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
#include "read_db_files_from_gld.h"
#include "updproc.h"

#define UPDPROC_LOG_FILE_SUFFIX			".updproc"
#define UPDHELPER_READER_LOG_FILE_SUFFIX	".uhr_"
#define UPDHELPER_WRITER_LOG_FILE_SUFFIX	".uhw_"

GBLREF	int		updproc_log_fd, updhelper_log_fd;
GBLREF 	FILE		*updproc_log_fp, *updhelper_log_fp;
GBLREF	recvpool_addrs	recvpool;
GBLREF	uint4		process_id;

int upd_log_init(recvpool_user who)
{
	char	log_file[MAX_FN_LEN + 1], file_suffix_str[MAX_FN_LEN + 1], pid_str[11], *pid_end_ptr, *file_suffix;
	int		status = SS_NORMAL;
	int		*fd_addrs;
	FILE		**fp_addrs;

	fd_addrs = (UPDPROC == who) ? &updproc_log_fd : &updhelper_log_fd;
	fp_addrs = (UPDPROC == who) ? &updproc_log_fp : &updhelper_log_fp;
	strcpy(log_file, recvpool.gtmrecv_local->log_file);
	if (0 != strncmp(log_file, DEVICE_PREFIX, STR_LIT_LEN(DEVICE_PREFIX)))
	{
		if (UPDPROC == who)
			file_suffix = UPDPROC_LOG_FILE_SUFFIX;
		else
		{
			if (UPD_HELPER_READER == who)
				strcpy(file_suffix_str, UPDHELPER_READER_LOG_FILE_SUFFIX);
			else /* UPD_HELPER_WRITER == who */
				strcpy(file_suffix_str, UPDHELPER_WRITER_LOG_FILE_SUFFIX);
			pid_end_ptr = (char *)i2asc((uchar_ptr_t)pid_str, process_id);
			*pid_end_ptr = '\0';
			strcat(file_suffix_str, pid_str);
			file_suffix = file_suffix_str;
		}
		strcat(log_file, file_suffix);
	}
	if (FD_INVALID == *fd_addrs || (UPDPROC == who && 0 != strcmp(log_file, recvpool.upd_proc_local->log_file)))
	{
		status = repl_log_init(REPL_GENERAL_LOG, fd_addrs, log_file);
		repl_log_fd2fp(fp_addrs, *fd_addrs);
		if (UPDPROC == who)
		{
			strcpy(recvpool.upd_proc_local->log_file, log_file);
			gtm_event_log_init();
		}
	}
	return(status);
}
