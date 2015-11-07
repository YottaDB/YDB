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

#ifndef MUTEX_MSEM_WAKE
#include "gtm_ipc.h"
#include "gtm_socket.h"
#include <sys/un.h>
#include <sys/time.h>
#include <errno.h>
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_stdio.h"
#include "gtm_stat.h"
#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "mutex.h"
#include "io.h"
#include "secshr_client.h"
#include "gtmsecshr.h"
#include "iosp.h"
#include "gtm_logicals.h"
#include "eintr_wrappers.h"
#include "send_msg.h"
#include "trans_log_name.h"

GBLREF uint4			process_id;
GBLREF int			mutex_sock_fd;
GBLREF struct sockaddr_un	mutex_sock_address;
GBLREF struct sockaddr_un	mutex_wake_this_proc;
GBLREF int			mutex_wake_this_proc_len;
GBLREF int			mutex_wake_this_proc_prefix_len;
GBLREF fd_set			mutex_wait_on_descs;

static readonly char hex_table[] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

error_def(ERR_MUTEXERR);
error_def(ERR_MUTEXRSRCCLNUP);
error_def(ERR_TEXT);

void mutex_sock_init(void)
{
	mstr		mutex_sock_dir_lognam, mutex_sock_dir_transnam;
	int		mutex_sock_path_len;
	uint4		mutex_sock_trans_status;
	char		mutex_sock_path[MAX_TRANS_NAME_LEN];
	int		mutex_sock_len, save_errno;
	struct stat	mutex_sock_stat_buf;
	int		status;
	unsigned char   pid_str[2 * SIZEOF(pid_t) + 1];
	DEBUG_ONLY(boolean_t existed;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (FD_INVALID != mutex_sock_fd) /* Initialization done already */
		return;
	/* Create the socket used for sending and receiving mutex wake mesgs */
	if (FD_INVALID == (mutex_sock_fd = socket(AF_UNIX, SOCK_DGRAM, 0)))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_MUTEXERR, 0, ERR_TEXT, 2,
			      RTS_ERROR_TEXT("Error with mutex socket create"), errno);
	memset((char *)&mutex_sock_address, 0, SIZEOF(mutex_sock_address));
	/* Get the socket path */
	mutex_sock_dir_lognam.len = SIZEOF(MUTEX_SOCK_DIR) - 1;
	mutex_sock_dir_lognam.addr = MUTEX_SOCK_DIR;
	mutex_sock_trans_status = TRANS_LOG_NAME(&mutex_sock_dir_lognam, &mutex_sock_dir_transnam,
						 mutex_sock_path, SIZEOF(mutex_sock_path), do_sendmsg_on_log2long);
	if (mutex_sock_trans_status != SS_NORMAL)
	{
		strcpy(mutex_sock_path, DEFAULT_MUTEX_SOCK_DIR);
		mutex_sock_path_len = SIZEOF(DEFAULT_MUTEX_SOCK_DIR) - 1;
	} else
		mutex_sock_path_len = mutex_sock_dir_transnam.len;
	/* If the path doesn't already end with a '/' pad a '/' */
	if (mutex_sock_path[mutex_sock_path_len - 1] != '/')
	{
		mutex_sock_path[mutex_sock_path_len++] = '/';
		mutex_sock_path[mutex_sock_path_len] = '\0';
	}
	if ((mutex_sock_path_len + MAX_MUTEX_SOCKFILE_NAME_LEN) > SIZEOF(mutex_sock_address.sun_path))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUTEXERR, 0, ERR_TEXT, 2,
			      RTS_ERROR_TEXT("Mutex socket path too long"));
	strcpy(mutex_sock_path + mutex_sock_path_len, MUTEX_SOCK_FILE_PREFIX);
	mutex_sock_path_len += (SIZEOF(MUTEX_SOCK_FILE_PREFIX) - 1);
	mutex_wake_this_proc_prefix_len = mutex_sock_path_len;
	/* Extend mutex_sock_path with pid */
	strcpy(mutex_sock_path + mutex_sock_path_len, (char *)pid2ascx(pid_str, process_id));
	mutex_sock_path_len += STRLEN((char *)pid_str);
	if (mutex_sock_path_len > SIZEOF(mutex_sock_address.sun_path))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUTEXERR, 0, ERR_TEXT, 2,
			      RTS_ERROR_TEXT("Mutex socket path too long"));
	mutex_sock_address.sun_family = AF_UNIX;
	strcpy(mutex_sock_address.sun_path, mutex_sock_path);
	mutex_sock_len = SIZEOF(mutex_sock_address.sun_family) + mutex_sock_path_len + 1; /* Include NULL byte in length */
	DEBUG_ONLY(if (!TREF(gtm_usesecshr)))
	{
		status = UNLINK(mutex_sock_address.sun_path);	/* in case it was left from last time */
		save_errno = (-1 == status) ? errno : 0;
	} DEBUG_ONLY(else save_errno = -1);	/* Non-zero and non-ENOENT value so uses gtmsecshr */
	if (0 != save_errno)
	{	/* Separate checks for unlink success vs no file removal needed */
		if (ENOENT != save_errno)
		{
#			ifdef DEBUG
			/* If using gtm_usesecshr, can get a log of bogus MUTEXRSRCCLNUP messages so see if the socket
			 * actually exists or not so can supress the message if not (but still push it through secshr).
			 */
			if (TREF(gtm_usesecshr))
			{
				STAT_FILE(mutex_sock_address.sun_path, &mutex_sock_stat_buf, status);
				existed = (0 == status) ? TRUE : FALSE;
			} else
				existed = TRUE;		/* If no gtm_usesecshr, wouldn't be here unless existed */
#			endif
			if (0 == (status = send_mesg2gtmsecshr(REMOVE_FILE, (unsigned int)-1, mutex_sock_address.sun_path,
							       mutex_sock_path_len + 1)))
			{
				DEBUG_ONLY(if (existed))	/* Avoid mesg unless socket existed */
					send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_MUTEXRSRCCLNUP, 2, mutex_sock_path_len,
						     mutex_sock_path, ERR_TEXT, 2, LEN_AND_LIT("Resource removed by gtmsecshr"));
			} else if (ENOENT != status)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(10) ERR_MUTEXERR, 0, ERR_TEXT, 2,
					  LEN_AND_LIT("gtmsecshr failed to remove leftover mutex resource"),
					  ERR_TEXT, 2, mutex_sock_path_len, mutex_sock_path);
			/* else don't bother if somebody removed the file before gtmsecshr got to it */
		}
	} else  /* unlink succeeded - socket must have existed - now cleaned up */
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_MUTEXRSRCCLNUP, 2, mutex_sock_path_len, mutex_sock_path);
	if (0 > BIND(mutex_sock_fd, (struct sockaddr *)&mutex_sock_address, mutex_sock_len))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_MUTEXERR, 0, ERR_TEXT, 2,
			      RTS_ERROR_TEXT("Error with mutex socket bind"), errno);
	/* Set the socket permissions to override any umask settings.
	 * Allow owner and group read and write access.
	 */
	STAT_FILE(mutex_sock_address.sun_path, &mutex_sock_stat_buf, status);
	if (-1 == status)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_MUTEXERR, 0, ERR_TEXT, 2,
			  RTS_ERROR_TEXT("Error with mutex socket stat"),
			  errno);
	mutex_sock_stat_buf.st_mode |= (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
	if (-1 == CHMOD(mutex_sock_address.sun_path, mutex_sock_stat_buf.st_mode))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_MUTEXERR, 0, ERR_TEXT, 2,
			  RTS_ERROR_TEXT("Error with mutex socket chmod"),
			  errno);
	/* Clear the descriptor set used to sense wake up message */
	FD_ZERO(&mutex_wait_on_descs);
	/* To make mutex_wake_proc faster, pre-initialize portions of
	 * mutex_wake_this_proc which are invariant of the pid to be woken up.
	 */
	memset((char *)&mutex_wake_this_proc, 0, SIZEOF(mutex_wake_this_proc));
	mutex_wake_this_proc.sun_family = AF_UNIX;
	strcpy(mutex_wake_this_proc.sun_path, mutex_sock_path);
	mutex_wake_this_proc_len = mutex_sock_len;
}

unsigned char *pid2ascx(unsigned char *pid_str, pid_t pid)
{	/* pid_str should accommodate atleast 2*SIZEOF(pid_t) + 1 characters */
	register unsigned char *cp;

	cp = &pid_str[2*SIZEOF(pid_t)];
	*cp = '\0'; /* Null terminate the string */
	while(cp > pid_str)
	{
		*--cp = hex_table[pid & 0xF];
		pid >>= 4;
	}
	return(pid_str);
}

#endif /*MUTEX_MSEM_WAKE*/
