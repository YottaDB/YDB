/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_fcntl.h"
#include "gtm_stdio.h"
#include "gtm_unistd.h"
#include "gtm_stat.h"
#include "gtm_string.h"
#include <errno.h>

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "iosp.h"
#include "repl_log.h"
#include "repl_errno.h"
#include "gtmio.h"
#include "eintr_wrappers.h"
#include "repl_sp.h"
#include "send_msg.h"
#include "gtmmsg.h"
#include "have_crit.h"
#ifdef __MVS__
#include "gtm_zos_io.h"
#endif

GBLDEF int		gtmsource_log_fd = FD_INVALID;
GBLDEF int		gtmrecv_log_fd = FD_INVALID;
GBLDEF int		updproc_log_fd = FD_INVALID;
GBLDEF int		updhelper_log_fd = FD_INVALID;

GBLDEF FILE		*gtmsource_log_fp = NULL;
GBLDEF FILE		*gtmrecv_log_fp = NULL;
GBLDEF FILE		*updproc_log_fp = NULL;
GBLDEF FILE		*updhelper_log_fp = NULL;

error_def(ERR_REPLLOGOPN);
error_def(ERR_TEXT);
error_def(ERR_REPLEXITERR);
ZOS_ONLY(error_def(ERR_BADTAG);)
int repl_log_init(repl_log_file_t log_type,
		  int *log_fd,
		  char *log)
{
	/* Open the log file */
	char log_file_name[MAX_FN_LEN + 1], *err_code;
	int	tmp_fd;
	int	save_errno;
	int	stdout_status, stderr_status;
	int	rc;
#ifdef __MVS__
	int		status;
	int		realfiletag;
	struct stat     info;
#endif
	if (*log == '\0')
		return(EREPL_LOGFILEOPEN);

	strcpy(log_file_name, log);
	ZOS_ONLY(STAT_FILE(log_file_name, &info, status);)
	OPENFILE3(log_file_name, O_RDWR | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, tmp_fd);
	if (tmp_fd < 0)
	{
		if ((REPL_GENERAL_LOG == log_type) && (FD_INVALID == *log_fd))
		{
			save_errno = ERRNO;
			err_code = STRERROR(save_errno);
			gtm_putmsg(VARLSTCNT(8) ERR_REPLLOGOPN, 6, LEN_AND_STR(log_file_name),
				   LEN_AND_STR(err_code),LEN_AND_STR(NULL_DEVICE));
			strcpy(log_file_name, NULL_DEVICE);
			if (log_type == REPL_GENERAL_LOG)
				strcpy(log, NULL_DEVICE);
			OPENFILE(log_file_name, O_RDWR, tmp_fd); /* Should not fail */
			exit(EREPL_LOGFILEOPEN);
		} else
		{
			save_errno = ERRNO;
			err_code = STRERROR(save_errno);
			gtm_putmsg(VARLSTCNT(8) ERR_REPLLOGOPN, 6, LEN_AND_STR(log_file_name),
				   LEN_AND_STR(err_code), LEN_AND_STR(log));
			return(EREPL_LOGFILEOPEN);
		}
	}
#ifdef __MVS__
	if (0 == status)
	{
		if (-1 == gtm_zos_tag_to_policy(tmp_fd, TAG_UNTAGGED, &realfiletag))
			TAG_POLICY_GTM_PUTMSG(log_file_name, errno, realfiletag, TAG_UNTAGGED);
	} else
	{
		if (-1 == gtm_zos_set_tag(tmp_fd, TAG_EBCDIC, TAG_TEXT, TAG_FORCE, &realfiletag))
			TAG_POLICY_GTM_PUTMSG(log_file_name, errno, realfiletag, TAG_EBCDIC);
	}
#endif
	if (log_type == REPL_GENERAL_LOG)
	{
		int dup2_res;
		/* Duplicate stdout and stderr onto log file */
		DUP2(tmp_fd, 1, stdout_status);
		if (stdout_status >= 0)
		{
			DUP2(tmp_fd, 2, stderr_status);
			if (stderr_status < 0)
			{
				save_errno = ERRNO;
				if (FD_INVALID != *log_fd)
				{
					DUP2(*log_fd, 1, dup2_res); /* Restore old log file */
					DUP2(*log_fd, 2, dup2_res);
				}
			}
		} else
		{
			save_errno = ERRNO;
			if (FD_INVALID != *log_fd)
				DUP2(*log_fd, 1, dup2_res); /* Restore old log file */
		}

		if (stdout_status >= 0 && stderr_status >= 0)
		{
			if (FD_INVALID != *log_fd)
				CLOSEFILE_RESET(*log_fd, rc);	/* resets "*log_fd" to FD_INVALID */
			*log_fd = tmp_fd;
		} else
		{
			err_code = STRERROR(save_errno);
			gtm_putmsg(VARLSTCNT(10) ERR_REPLLOGOPN, 6,
			 	   LEN_AND_STR(log_file_name),
				   LEN_AND_STR(err_code),
				   strlen(log),
				   log,
				   ERR_TEXT, 2, RTS_ERROR_LITERAL("Error in dup2"));
		}
	}
	return(SS_NORMAL);
}

int repl_log_fd2fp(FILE **fp, int fd)
{
	int fclose_res;

	/* For stats log file, we need to have FCLOSEd *fp because a later open() in repl_log_init() might return the same file
	 * descriptor as a previously closed stats log file. In that case, FCLOSE if done here affects the newly opened file and
	 * FDOPEN will fail returning NULL for the file pointer. */
	if (NULL != *fp)
		FCLOSE(*fp, fclose_res);
	FDOPEN(*fp, fd, "a");
	assert(NULL != *fp); /* we don't expect FDOPEN to fail */
	return(SS_NORMAL);
}
