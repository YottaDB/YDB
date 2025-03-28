/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_signal.h"
#include "gtm_ctype.h"
#include "gtm_stdlib.h"		/* for EXIT() */
#include "gtm_string.h"
#include "gtm_socket.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_stdio.h"
#include "gtm_stat.h"
#include "gtm_limits.h"
#include "gtm_syslog.h"
#include "gtm_ipc.h"

#include <stddef.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/param.h>

#include "gtmio.h"
#include "io.h"
#include "gtmsecshr.h"
#include "gtmimagename.h"
#include "iosp.h"
#include "error.h"
#include "eintr_wrappers.h"
#include "util.h"
#include "send_msg.h"
#include "gtm_un.h"
#include "gtmmsg.h"
#include "wcs_backoff.h"
#include "trans_log_name.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "secshr_client.h"
#include "hashtab.h"		/* for STR_HASH macro */
#include "fork_init.h"
#include "gtm_permissions.h"
#include "wbox_test_init.h"
#include "ydb_getenv.h"
#include "getjobnum.h"		/* for SET_PROCESS_ID */

GBLREF struct sockaddr_un       gtmsecshr_sock_name;
GBLREF key_t                    gtmsecshr_key;
GBLREF int                      gtmsecshr_sockpath_len;
GBLREF int                      gtmsecshr_sockfd;
GBLREF mstr                     gtmsecshr_pathname;
GBLREF int			server_start_tries;
GBLREF boolean_t		gtmsecshr_sock_init_done;
GBLREF uint4			process_id;
GBLREF ipcs_mesg		db_ipcs;
GBLREF char			ydb_dist[YDB_PATH_MAX];
GBLREF boolean_t		ydb_dist_ok_to_use;

LITREF char			ydb_release_name[];
LITREF int4			ydb_release_name_len;
LITREF gtmImageName		gtmImageNames[];

static int			secshr_sem;
static boolean_t		gtmsecshr_file_check_done;
static char			gtmsecshr_path[YDB_PATH_MAX];
static unsigned long		cur_seqno;

/* The below messages match up with the gtmsecshr_mesg_type codes */
const static char readonly *secshr_fail_mesg_code[] = {
	"",
	"Wake Message Failed",
	"Remove Semaphore failed",
	"Remove Shared Memory segment failed",
	"Remove File failed",
	"Continue Process failed",
	"Database Header flush failed",
};
/* The below messages match up with gtmsecshr exit codes from gtmsecshr.h. */
const static char readonly *secshrstart_error_code[] = {
	"",
	"gtmsecshr unable to set-uid to root",
	"The environmental variable ydb_dist is pointing to an invalid path",
	"Unable to start gtmsecshr executable",
	"gtmsecshr unable to create a  child process",
	"Error with gtmsecshr semaphore",
	"gtmsecshr already running - invalid invocation",
	"See syslog for cause of failure",
	"gtmsecshr startup failed - gtmsecshr unable to chdir to tmp directory",
	"gtmsecshr startup failed - gtmsecshr unable to determine invocation path",
	"gtmsecshr startup failed - gtmsecshr not named gtmsecshr",
	"gtmsecshr startup failed - startup path through $ydb_dist not setup correctly - check path and permissions"
};

#define MAX_COMM_ATTEMPTS		4	/* 1 to start secshr, 2 maybe slow, 3 maybe really slow, 4 outside max */
#define CLIENT_ACK_TIMEOUT_IN_SECONDS	5	/* 5 seconds timeout for RECVFROM calls in "send_mesg2gtmsecshr" */


#define START_SERVER										\
{												\
	int	arraysize, errorindex;								\
												\
	if (0 != (create_server_status = create_server()))					\
	{											\
		assert(ARRAYSIZE(secshrstart_error_code) == (LASTEXITCODE + 1));		\
		errorindex = create_server_status;						\
		if ((0 > errorindex) || (LASTEXITCODE < errorindex))				\
			errorindex = LASTEXITCODE;						\
		assert(0 <= errorindex);							\
		assert(ARRAYSIZE(secshrstart_error_code) > errorindex);				\
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_GTMSECSHRSTART, 3, RTS_ERROR_TEXT("Client"),	\
			process_id, ERR_TEXT, 2,						\
			RTS_ERROR_STRING(secshrstart_error_code[errorindex]));			\
		if (FATALFAILURE(create_server_status))						\
		{										\
			gtmsecshr_sock_cleanup(CLIENT);						\
			return create_server_status;						\
		}										\
		/* For transient failures we will continue after printing out message */	\
	}											\
	hiber_start(500); /* half-a-second to allow server to come up */			\
}

#define SETUP_FOR_RECV										\
{												\
	recv_ptr = (char *)&mesg;								\
	recv_len = SIZEOF(mesg);								\
	recv_complete = FALSE;									\
	save_errno = 0;										\
}

error_def(ERR_YDBDISTUNVERIF);
error_def(ERR_GTMSECSHR);
error_def(ERR_GTMSECSHRPERM);
error_def(ERR_GTMSECSHRSOCKET);
error_def(ERR_GTMSECSHRSRVF);
error_def(ERR_GTMSECSHRSRVFID);
error_def(ERR_GTMSECSHRSRVFIL);
error_def(ERR_GTMSECSHRSTART);
error_def(ERR_GTMSECSHRTMPPATH);
error_def(ERR_LOGTOOLONG);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);

int send_mesg2gtmsecshr(unsigned int code, unsigned int id, char *path, int path_len)
{
	int                     client_sockfd, create_server_status, fcntl_res;
	int			req_code, wait_count = 0;
	int			recv_len, send_len;
	ssize_t			num_chars_recvd, num_chars_sent;
	int 			save_errno, ret_code = 0, init_ret_code = 0;
	int			loop_count = 0;
	int			recv_complete;
	size_t			server_proc_len;
	int			semop_res;
	int			selstat, status;
	char			*recv_ptr, *send_ptr;
	struct sockaddr_un	server_proc;
	struct sembuf		sop[4];
	fd_set			wait_on_fd;
	gtmsecshr_mesg		mesg;
	int4			msec_timeout;
	char			*ydb_tmp_ptr;
	struct stat		stat_buf;
	char			file_perm[MAX_PERM_LEN];
	struct shmid_ds		shm_info;
	int			len;
	boolean_t		recvfrom_timedout;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	DBGGSSHR((LOGFLAGS, "secshr_client: New send request\n"));
	if (!ydb_dist_ok_to_use)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_YDBDISTUNVERIF, 4, STRLEN(ydb_dist), ydb_dist,
				gtmImageNames[image_type].imageNameLen, gtmImageNames[image_type].imageName);
	/* Create communication key (hash of release name) if it has not already been done */
	if (0 == TREF(gtmsecshr_comkey))
		STR_HASH((char *)ydb_release_name, ydb_release_name_len, TREF(gtmsecshr_comkey), 0);
	if (!gtmsecshr_file_check_done)
	{
		len = STRLEN(ydb_dist);
		assert(YDB_PATH_MAX >= (SECSHR_PARENT_DIR_LEN(len) + 1 + sizeof(GTMSECSHR_EXECUTABLE))); /* Includes null */
		SNPRINTF(gtmsecshr_path, GTM_PATH_MAX, "%.*s/%s",
			SECSHR_PARENT_DIR_LEN(len), SECSHR_PARENT_DIR(ydb_dist), GTMSECSHR_EXECUTABLE);
		gtmsecshr_pathname.addr = gtmsecshr_path;
		gtmsecshr_pathname.len = (mstr_len_t)(SECSHR_PARENT_DIR_LEN(len) + 1 + strlen(GTMSECSHR_EXECUTABLE)); /* Excludes null */
		assert((0 < gtmsecshr_pathname.len) && (YDB_PATH_MAX > gtmsecshr_pathname.len));
		if (-1 == Stat(gtmsecshr_pathname.addr, &stat_buf))
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_SYSCALL, 5,
				LEN_AND_LIT("stat"), CALLFROM, errno);
		if ((ROOTUID != stat_buf.st_uid) || !(stat_buf.st_mode & S_ISUID))
		{
			SNPRINTF(file_perm, SIZEOF(file_perm), "%04o", stat_buf.st_mode & PERMALL);
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(7) ERR_GTMSECSHRPERM, 5,
				gtmsecshr_pathname.len, gtmsecshr_pathname.addr,
				RTS_ERROR_STRING(file_perm), stat_buf.st_uid);
		}
		if (0 != ACCESS(gtmsecshr_pathname.addr, (X_OK)))
		{
			SNPRINTF(file_perm, SIZEOF(file_perm), "%04o", stat_buf.st_mode & PERMALL);
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_GTMSECSHRPERM, 5,
				gtmsecshr_pathname.len, gtmsecshr_pathname.addr,
				RTS_ERROR_STRING(file_perm), stat_buf.st_uid, errno);
		}
		gtmsecshr_file_check_done = TRUE;
	}
	if (!gtmsecshr_sock_init_done)
	{
		struct timeval		tv;

		if (0 < (init_ret_code = gtmsecshr_sock_init(CLIENT)))	/* Note assignment */
			return init_ret_code;
		assert(gtmsecshr_sock_init_done);
		/* Set receive timeout at socket level using "setsockopt()". Used by RECVFROM in later do/while loop. */
		tv.tv_sec = CLIENT_ACK_TIMEOUT_IN_SECONDS;
		tv.tv_usec = 0;
		if (-1 == setsockopt(gtmsecshr_sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, SIZEOF(tv)))
		{
			char	*errptr;

			save_errno = errno;
			errptr = (char *)STRERROR(save_errno);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
				RTS_ERROR_LITERAL("SO_RCVTIMEO"), save_errno, STRLEN(errptr), errptr);
			assert(FALSE);	/* rts_error_csa should not return */
		}
	}
	WBTEST_ONLY(WBTEST_YDB_MAXSECSHRRETRY,
		    loop_count = MAX_COMM_ATTEMPTS + 1;		/* Cause us to enter error block below */
		    req_code = code;				/* We won't enter the loop below to set this field */
		    );
	DEBUG_ONLY(mesg.usesecshr = TREF(ydb_usesecshr));	/* Flag ignored in PRO build */
	while (MAX_COMM_ATTEMPTS >= loop_count)
	{	/* first, try the sendto */
		req_code = mesg.code = code;
		send_len = (int4)(GTM_MESG_HDR_SIZE);
  		if (REMOVE_FILE == code)
		{
			assert(YDB_PATH_MAX > path_len);	/* Name is not user supplied so simple check */
			memcpy(mesg.mesg.path, path, path_len);
			send_len += path_len;
		} else if (FLUSH_DB_IPCS_INFO == code)
		{
			assert(YDB_PATH_MAX > db_ipcs.fn_len);
			/* Most of the time file length is much smaller than YDB_PATH_MAX, hence the fn_len + 1 below */
			memcpy(&mesg.mesg.db_ipcs, &db_ipcs, (offsetof(ipcs_mesg, fn[0]) + db_ipcs.fn_len + 1));
			send_len += offsetof(ipcs_mesg, fn[0]);
			send_len += mesg.mesg.db_ipcs.fn_len + 1;
		} else
		{
			mesg.mesg.id = id;
			send_len += SIZEOF(mesg.mesg.id);
		}
		DBGGSSHR((LOGFLAGS, "secshr_client: loop %d  frm-pid: %d  to-pid: %d  send_len: %d  code: %d\n", loop_count,
			  process_id, id, send_len, code));
		mesg.comkey = TREF(gtmsecshr_comkey);	/* Version communication key */
		mesg.pid = process_id;			/* Process id of client */
		mesg.seqno = ++cur_seqno;
		send_ptr = (char *)&mesg;
		SENDTO_SOCK(gtmsecshr_sockfd, send_ptr, send_len, 0, (struct sockaddr *)&gtmsecshr_sock_name,
			    (GTM_SOCKLEN_TYPE)gtmsecshr_sockpath_len, num_chars_sent);	/* This form handles EINTR internally */
		save_errno = errno;
		DBGGSSHR((LOGFLAGS, "secshr_client: sendto rc:    %d  errno: %d (only important if rc=-1)\n", (int)num_chars_sent,
			  save_errno));
		if (0 >= num_chars_sent)
		{	/* SENDTO_SOCK failed - start server and attempt to resend */
			char	buff[512];

			SNPRINTF(buff, SIZEOF(buff), "sendto(\"%s\")", gtmsecshr_sock_name.sun_path);
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_STR(buff), CALLFROM, save_errno);
			if ((EISCONN == save_errno) || (EBADF == save_errno))
			{
				gtmsecshr_sock_cleanup(CLIENT);
				gtmsecshr_sock_init(CLIENT);
				wcs_backoff(loop_count + 1);
				DBGGSSHR((LOGFLAGS, "secshr_client: Connection error, reset socket\n"));
			} else
			{
				START_SERVER;
				DBGGSSHR((LOGFLAGS, "secshr_client: sendto() failed - restarting server\n"));
			}
			loop_count++;
			continue;
		}
		SETUP_FOR_RECV;		/* Sets recvcomplete = FALSE */
		do
		{	/* Note RECVFROM does not loop on EINTR return codes so must be handled. Note also we only expect
			 * to receive the message header back as an acknowledgement.
			 */
			recvfrom_timedout = FALSE;
			num_chars_recvd = RECVFROM(gtmsecshr_sockfd, recv_ptr, GTM_MESG_HDR_SIZE, 0, (struct sockaddr *)0,
						   (GTM_SOCKLEN_TYPE *)0);
			HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
			save_errno = errno;
			DBGGSSHR((LOGFLAGS, "secshr_client: recvfrom rc: %d  errno: %d (only important if rc=-1)\n",
				  (int)num_chars_recvd, save_errno));
			if (0 <= num_chars_recvd)
			{	/* Message received - make sure it is large enough to have set seqno before we do anything
				 * to rely on it.
				 */
				if ((GTM_MESG_HDR_SIZE <= num_chars_recvd) && (mesg.seqno == cur_seqno)
				    && (TREF(gtmsecshr_comkey) == mesg.comkey))
					recv_complete = TRUE;
				else
				{	/* Message too short or not correct sequence */
					/* Print True/False for the possibilities we failed */
					DBGGSSHR((LOGFLAGS, "secshr_client: Message incorrect - chars: %d, seq: %d\n",
						  (GTM_MESG_HDR_SIZE <= num_chars_recvd), (mesg.seqno == cur_seqno)));
					SETUP_FOR_RECV;
					continue;
				}
			} else
			{	/* Something untoward happened */
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8)
					ERR_SYSCALL, 5, LEN_AND_STR("recvfrom()"), CALLFROM, save_errno);
				if ((EAGAIN == save_errno) || (EWOULDBLOCK == save_errno))
				{	/* Receive timeout expired in "RECVFROM()" call before any data was received */
					recvfrom_timedout = TRUE;
					break;
				}
				if (EINTR == save_errno)	/* Had an irrelevant interrupt - ignore */
				{
					eintr_handling_check();
					continue;
				}
				if (EBADF == save_errno)
					break;
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(11) ERR_GTMSECSHRSRVF, 4,
						RTS_ERROR_TEXT("Client"), process_id, loop_count - 1, ERR_TEXT, 2,
						RTS_ERROR_TEXT("recvfrom from gtmsecshr failed"), save_errno);
				if ((ECONNRESET == save_errno) || (ENOTCONN == save_errno))
				{
					num_chars_recvd = 0;
					break;
				}
				gtmsecshr_sock_cleanup(CLIENT);
				return save_errno;
			}
		} while (!recv_complete);
		if (recvfrom_timedout || (EBADF == save_errno) || (0 == num_chars_recvd))
		{	/* Timeout, connection issues, bad descriptor block - retry */
			gtmsecshr_sock_cleanup(CLIENT);
			gtmsecshr_sock_init(CLIENT);
			if (recvfrom_timedout)
			{
				START_SERVER;
				DBGGSSHR((LOGFLAGS, "secshr_client: Receive timeout expired - restarting server\n"));
			} else
				DBGGSSHR((LOGFLAGS, "secshr_client: Read error - socket reset, retrying\n"));
			loop_count++;
			continue;
		}
		/* Response to *our* latest message available */
		assert(recv_complete);
		if ((ret_code = mesg.code))		/* Warning - assignment */
		{
			DBGGSSHR((LOGFLAGS, "secshr_client: non-zero response from gtmsecshr - request: %d  retcode: %d\n",
				  req_code, ret_code));
			if (INVALID_COMKEY == ret_code)
			{	/* Comkey mismatch means for a different version of GT.M - we will not handle it */
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(13) ERR_GTMSECSHRSRVFIL, 7, RTS_ERROR_TEXT("Client"),
					 process_id, mesg.pid, req_code, RTS_ERROR_TEXT(mesg.mesg.path),
					 ERR_TEXT, 2, RTS_ERROR_STRING("Communicating with wrong GT.M version"));
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(13) MAKE_MSG_ERROR(ERR_GTMSECSHRSRVFIL), 7,
					RTS_ERROR_TEXT("Client"), process_id, mesg.pid, req_code, RTS_ERROR_TEXT(mesg.mesg.path),
					ERR_TEXT, 2, RTS_ERROR_STRING("Communicating with wrong GT.M version"));
				break;	/* rts_error should not return */
			}
			switch(req_code)
			{
				case REMOVE_FILE:
					/* Called from mutex_sock_init(). Path (and length) contain null terminator byte.
					 * See if file still exists (may have been deleted by earlier attempt). Caller
					 * handles actual error.
					 */
					if ((-1 != Stat(path, &stat_buf)) || (ENOENT != ret_code))
						send_msg_csa(CSA_ARG(NULL) VARLSTCNT(14) ERR_GTMSECSHRSRVFIL, 7,
								RTS_ERROR_TEXT("Client"),
							 	process_id, mesg.pid, req_code, RTS_ERROR_TEXT(mesg.mesg.path),
								ERR_TEXT, 2, RTS_ERROR_STRING(secshr_fail_mesg_code[req_code]),
								mesg.code);
					else
						ret_code = 0;	/* File is gone so this or a previous try actually worked */
					break;
				case REMOVE_SEM:
					/* See if semaphore still eixsts (may have been removed by earlier attempt that
					 * got a reply confused or lost). If not there, no error. Else error to op-log.
					 */
					if ((-1 != semctl(id, 0, GETVAL)) && !SEM_REMOVED(errno))
						send_msg_csa(CSA_ARG(NULL) VARLSTCNT(13) ERR_GTMSECSHRSRVFID, 6,
								RTS_ERROR_TEXT("Client"),
							 	process_id, mesg.pid, req_code, mesg.mesg.id, ERR_TEXT, 2,
								RTS_ERROR_STRING(secshr_fail_mesg_code[req_code]),
								mesg.code);
					else
						ret_code = 0;	/* File is gone so this or a previous try actually worked */
				case REMOVE_SHM:
					/* See if shmem still eixsts (may have been removed by earlier attempt that
					 * got a reply confused or lost). If not there, no error. Else error to op-log.
					 * Note -
					 */
					if ((-1 != shmctl(id, IPC_STAT, &shm_info)) && !SEM_REMOVED(errno))
						send_msg_csa(CSA_ARG(NULL) VARLSTCNT(13) ERR_GTMSECSHRSRVFID, 6,
								RTS_ERROR_TEXT("Client"),
							 	process_id, mesg.pid, req_code, mesg.mesg.id, ERR_TEXT, 2,
								RTS_ERROR_STRING(secshr_fail_mesg_code[req_code]),
								mesg.code);
					else
						ret_code = 0;	/* File is gone so this or a previous try actually worked */
					break;
				case FLUSH_DB_IPCS_INFO:	/* Errors handled by caller */
					break;
				default:
					if (EPERM != mesg.code && EACCES != mesg.code)
						send_msg_csa(CSA_ARG(NULL) VARLSTCNT(13) ERR_GTMSECSHRSRVFID, 6,
								RTS_ERROR_TEXT("Client"),
								process_id, mesg.pid, req_code, mesg.mesg.id, ERR_TEXT, 2,
							 	RTS_ERROR_STRING(secshr_fail_mesg_code[req_code]),
							 	mesg.code);
					break;
			}
		}
		break;
	}
	if (MAX_COMM_ATTEMPTS < loop_count)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(10) ERR_GTMSECSHRSRVF, 4,
				RTS_ERROR_TEXT("Client"), process_id, loop_count - 1,
			   	ERR_TEXT, 2, RTS_ERROR_TEXT("Unable to communicate with gtmsecshr"));
		if (FLUSH_DB_IPCS_INFO >= req_code)
		{
			if (ret_code)
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_TEXT, 2,
						RTS_ERROR_STRING(secshr_fail_mesg_code[req_code]), ret_code);
			else
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TEXT, 2,
						RTS_ERROR_STRING(secshr_fail_mesg_code[req_code]));
		}
		ret_code = -1;
		/* If ydb_tmp is not defined, show default path */
		if ((ydb_tmp_ptr = ydb_getenv(YDBENVINDX_TMP, NULL_SUFFIX, NULL_IS_YDB_ENV_MATCH)))
		{
			if (!IS_GTM_IMAGE)
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_GTMSECSHRTMPPATH, 2,
					RTS_ERROR_TEXT(ydb_tmp_ptr),
					ERR_TEXT, 2, RTS_ERROR_TEXT("(from $ydb_tmp/$gtm_tmp)"));
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_GTMSECSHRTMPPATH, 2,
				RTS_ERROR_TEXT(ydb_tmp_ptr), ERR_TEXT, 2, RTS_ERROR_TEXT("(from $ydb_tmp/$gtm_tmp)"));
		} else
		{
			if (!IS_GTM_IMAGE)
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4)
						ERR_GTMSECSHRTMPPATH, 2, RTS_ERROR_TEXT("/tmp"));

			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4)
					ERR_GTMSECSHRTMPPATH, 2, RTS_ERROR_TEXT("/tmp"));
		}
	}
	if (ONETIMESOCKET == init_ret_code)
		gtmsecshr_sock_cleanup(CLIENT);
	return ret_code;
}

int create_server(void)
{
	int		child_pid, done_pid, status = 0;
#	ifdef _BSD
	union	wait	chld_status;
#	define CSTAT	chld_status
#	else
#	define CSTAT	status
#	endif
	int		save_errno;

	FORK(child_pid);
	if (0 == child_pid)
	{
		SET_PROCESS_ID;
		/* Do exec using gtmsecshr_path, which was initialize in file check code - send_mesg2gtmsecshr */
		if (WBTEST_ENABLED(WBTEST_BADEXEC_SECSHR_PROCESS))
			STRCPY(gtmsecshr_path, "");
		status = EXECL(gtmsecshr_path, gtmsecshr_path, 0);
		if (-1 == status)
		{
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_GTMSECSHRSTART, 3, RTS_ERROR_TEXT("Client"), process_id,
				ERR_TEXT, 2, RTS_ERROR_STRING(secshrstart_error_code[UNABLETOEXECGTMSECSHR]));
			UNDERSCORE_EXIT(UNABLETOEXECGTMSECSHR);
		}
        } else
	{
		if (-1 == child_pid)
		{
			status = GNDCHLDFORKFLD;
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(10) ERR_GTMSECSHRSTART, 3, RTS_ERROR_TEXT("Client"), process_id,
				   ERR_TEXT,  2, RTS_ERROR_TEXT("Failed to fork off gtmsecshr"), errno);
			/* Sleep for a while and hope a subsequent fork will succeed */
			hiber_start(1000);
		}
		for (; !status ;)
		{
			/* To prevent a warning message that the compiler issues */
			done_pid = wait(&CSTAT);
			HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
			if (done_pid == child_pid)
			{
				status = WEXITSTATUS(CSTAT);
				break;
			} else if (-1 == done_pid)
			{
				if (ECHILD == errno) /* Assume normal exit status */
					break;
				else if (EINTR != errno)
				{
					status = GNDCHLDFORKFLD;
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(10) ERR_GTMSECSHRSTART, 3,
							RTS_ERROR_TEXT("Client"), process_id,
							ERR_TEXT, 2, RTS_ERROR_TEXT("Error spawning gtmsecshr"), errno);
				} else
					eintr_handling_check();
			}
		}
	}
	return status;
}
