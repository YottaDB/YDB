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

#include <stddef.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/param.h>
#include "gtm_ctype.h"
#include "gtm_stdlib.h"		/* for exit() */
#include "gtm_string.h"
#include "gtm_socket.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_stdio.h"
#include "gtm_stat.h"
#include "gt_timer.h"
#include "gtm_limits.h"
#include "gtm_syslog.h"

#include "gtmio.h"
#include "io.h"
#include "gtmsecshr.h"
#include "iosp.h"
#include "error.h"
#include "eintr_wrappers.h"
#include "util.h"
#include "send_msg.h"
#include "gtmmsg.h"
#include "wcs_backoff.h"
#include "trans_log_name.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtm_logicals.h"
#include "secshr_client.h"
#include "gtm_semutils.h"
#include "hashtab.h"		/* for STR_HASH macro */
#include "fork_init.h"

GBLREF struct sockaddr_un       gtmsecshr_sock_name;
GBLREF key_t                    gtmsecshr_key;
GBLREF int                      gtmsecshr_sockpath_len;
GBLREF int                      gtmsecshr_sockfd;
GBLREF mstr                     gtmsecshr_pathname;
GBLREF int			server_start_tries;
GBLREF boolean_t		gtmsecshr_sock_init_done;
GBLREF uint4			process_id;
GBLREF ipcs_mesg		db_ipcs;

LITREF char			gtm_release_name[];
LITREF int4			gtm_release_name_len;

static int			secshr_sem;
static boolean_t		gtmsecshr_file_check_done;
static mstr			gtmsecshr_logname;
static char			gtmsecshr_path[GTM_PATH_MAX];
static volatile boolean_t	client_timer_popped;
static unsigned long		cur_seqno;

/* The below messages match up with the gtmsecshr_mesg_type codes */
const static char readonly *secshr_fail_mesg_code[] = {
	"",
	"Wake Message Failed",
	"Remove Semaphore failed",
	"Remove Shared Memory segment failed",
	"Remove File failed",
	"Continue Process failed",
};
/* The below messages match up with gtmsecshr exit codes from gtmsecshr.h. */
const static char readonly *secshrstart_error_code[] = {
	"",
	"gtmsecshr unable to set-uid to root",
	"The environmental variable gtm_dist is pointing to an invalid path",
	"Unable to exec gtmsecshr",
	"gtmsecshr unable to create a  child process",
	"Error with gtmsecshr semaphore",
	"gtmsecshr already running - invalid invocation",
	"See syslog for cause of failure",
	"gtmsecshr startup failed - gtmsecshr unable to chdir to tmp directory",
	"gtmsecshr startup failed - gtmsecshr unable to determine invocation path",
	"gtmsecshr startup failed - gtmsecshr not named gtmsecshr",
	"gtmsecshr startup failed - $gtm_dist not same as startup path"
};

#define MAX_COMM_ATTEMPTS		4	/* 1 to start secshr, 2 maybe slow, 3 maybe really slow, 4 outside max */
#define CLIENT_ACK_TIMER		5

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
	hiber_start(3000); /* 3000 ms (3 sec) to allow server to come up */			\
}

#define SETUP_FOR_RECV										\
{												\
	recv_ptr = (char *)&mesg;								\
	recv_len = SIZEOF(mesg);								\
	client_timer_popped = FALSE;								\
	recv_complete = FALSE;									\
	save_errno = 0;										\
	msec_timeout = timeout2msec(CLIENT_ACK_TIMER);						\
	start_timer(timer_id, msec_timeout, client_timer_handler, 0, NULL);			\
}

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

void client_timer_handler(void)
{
	client_timer_popped = TRUE;
}

int send_mesg2gtmsecshr(unsigned int code, unsigned int id, char *path, int path_len)
{
	int                     client_sockfd, create_server_status, fcntl_res;
	int			req_code, wait_count = 0;
	int			recv_len, send_len;
	ssize_t			num_chars_recvd, num_chars_sent;
	int 			save_errno, ret_code = 0, init_ret_code = 0;
	int			loop_count = 0;
	int			recv_complete, send_complete;
	boolean_t		retry = FALSE;
	size_t			server_proc_len;
	int			semop_res;
	int			selstat, status;
	char			*recv_ptr, *send_ptr;
	struct sockaddr_un	server_proc;
	struct sembuf		sop[4];
	fd_set			wait_on_fd;
	gtmsecshr_mesg		mesg;
	TID			timer_id;
	int4			msec_timeout;
	char			*gtm_tmp_ptr;
	struct stat		stat_buf;
	struct shmid_ds		shm_info;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	DBGGSSHR((LOGFLAGS, "secshr_client: New send request\n"));
	/* Create communication key (hash of release name) if it has not already been done */
	if (0 == TREF(gtmsecshr_comkey))
	{
		STR_HASH((char *)gtm_release_name, gtm_release_name_len, TREF(gtmsecshr_comkey), 0);
	}
	timer_id = (TID)send_mesg2gtmsecshr;
	if (!gtmsecshr_file_check_done)
	{
		gtmsecshr_logname.addr = GTMSECSHR_PATH;
                gtmsecshr_logname.len = SIZEOF(GTMSECSHR_PATH);
		status = TRANS_LOG_NAME(&gtmsecshr_logname, &gtmsecshr_pathname, gtmsecshr_path, SIZEOF(gtmsecshr_path),
					dont_sendmsg_on_log2long);
                if (SS_NORMAL != status)
		{
			if (SS_LOG2LONG == status)
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_LOGTOOLONG, 3,
					gtmsecshr_logname.len, gtmsecshr_logname.addr, SIZEOF(gtmsecshr_path) - 1);
                        send_msg_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_GTMSECSHRSTART, 3,
					RTS_ERROR_TEXT("Client"), process_id, ERR_TEXT, 2,
					RTS_ERROR_STRING(secshrstart_error_code[INVTRANSGTMSECSHR]));
                        rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_GTMSECSHRSTART, 3,
					RTS_ERROR_TEXT("Client"), process_id, ERR_TEXT, 2,
					RTS_ERROR_STRING(secshrstart_error_code[INVTRANSGTMSECSHR]));
		}
		gtmsecshr_pathname.addr[gtmsecshr_pathname.len] = '\0';
		if (-1 == Stat(gtmsecshr_pathname.addr, &stat_buf))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
					LEN_AND_LIT("stat"), CALLFROM, errno);
		if ((ROOTUID != stat_buf.st_uid) ||
		    !(stat_buf.st_mode & S_ISUID))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_GTMSECSHRPERM);
		gtmsecshr_file_check_done = TRUE;
	}
	if (!gtmsecshr_sock_init_done && (0 < (init_ret_code = gtmsecshr_sock_init(CLIENT))))	/* Note assignment */
		return init_ret_code;
	DEBUG_ONLY(mesg.usesecshr = TREF(gtm_usesecshr));	/* Flag ignored in PRO build */
	while (MAX_COMM_ATTEMPTS >= loop_count)
	{	/* first, try the sendto */
		req_code = mesg.code = code;
		send_len = (int4)(GTM_MESG_HDR_SIZE);
  		if (REMOVE_FILE == code)
		{
			assert(GTM_PATH_MAX > path_len);	/* Name is not user supplied so simple check */
			memcpy(mesg.mesg.path, path, path_len);
			send_len += path_len;
		} else if (FLUSH_DB_IPCS_INFO == code)
		{
			assert(GTM_PATH_MAX > db_ipcs.fn_len);
			memcpy(&mesg.mesg.db_ipcs, &db_ipcs, (offsetof(ipcs_mesg, fn[0]) + db_ipcs.fn_len + 1));
			/* Most of the time file length is much smaller than GTM_PATH_MAX */
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
		send_complete = FALSE;
		SENDTO_SOCK(gtmsecshr_sockfd, send_ptr, send_len, 0, (struct sockaddr *)&gtmsecshr_sock_name,
			    (GTM_SOCKLEN_TYPE)gtmsecshr_sockpath_len, num_chars_sent);	/* This form handles EINTR internally */
		save_errno = errno;
		DBGGSSHR((LOGFLAGS, "secshr_client: sendto rc:    %d  errno: %d (only important if rc=-1)\n", (int)num_chars_sent,
			  save_errno));
		if (0 >= num_chars_sent)
		{	/* SENDTO_SOCK failed - start server and attempt to resend */
			if ((EISCONN == save_errno) || (EBADF == save_errno))
			{
				gtmsecshr_sock_cleanup(CLIENT);
				gtmsecshr_sock_init(CLIENT);
				wcs_backoff(loop_count + 1);
				DBGGSSHR((LOGFLAGS, "secshr_client: Connection error, reset socket\n"));
			} else
			{
				if (0 < loop_count)
					/* No message unless attempted server start at least once */
					send_msg_csa(CSA_ARG(NULL) VARLSTCNT(11) ERR_GTMSECSHRSRVF, 4,
							RTS_ERROR_TEXT("Client"), process_id,
							loop_count - 1, ERR_TEXT, 2,
							RTS_ERROR_TEXT("sendto to gtmsecshr failed"), save_errno);
				START_SERVER;
				DBGGSSHR((LOGFLAGS, "secshr_client: sendto() failed - restarting server\n"));
			}
			loop_count++;
			continue;
		}
		SETUP_FOR_RECV;		/* Sets timer, recvcomplete = FALSE */
		do
		{	/* Note RECVFROM does not loop on EINTR return codes so must be handled. Note also we only expect
			 * to receive the message header back as an acknowledgement.
			 */
			num_chars_recvd = RECVFROM(gtmsecshr_sockfd, recv_ptr, GTM_MESG_HDR_SIZE, 0, (struct sockaddr *)0,
						   (GTM_SOCKLEN_TYPE *)0);
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
					cancel_timer(timer_id);
					/* Print True/False for the possibilities we failed */
					DBGGSSHR((LOGFLAGS, "secshr_client: Message incorrect - chars: %d, seq: %d\n",
						  (GTM_MESG_HDR_SIZE <= num_chars_recvd), (mesg.seqno == cur_seqno)));
					SETUP_FOR_RECV;
					continue;
				}
			} else
			{	/* Something untoward happened */
				if (client_timer_popped)
					break;
				if (EINTR == save_errno)	/* Had an irrelevant interrupt - ignore */
					continue;
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
		cancel_timer(timer_id);
		if (client_timer_popped || (EBADF == save_errno) || (0 == num_chars_recvd))
		{	/* Timeout, connection issues, bad descriptor block - retry */
			gtmsecshr_sock_cleanup(CLIENT);
			gtmsecshr_sock_init(CLIENT);
			retry = TRUE;
			if (client_timer_popped)
			{
				START_SERVER;
				DBGGSSHR((LOGFLAGS, "secshr_client: Read timer popped - restarting server\n"));
			} else
				DBGGSSHR((LOGFLAGS, "secshr_client: Read error - socket reset, retrying\n"));
			loop_count++;
			continue;
		}
		/* Response to *our* latest message available */
		assert(recv_complete);
		if (ret_code = mesg.code)		/* Warning - assignment */
		{
			DBGGSSHR((LOGFLAGS, "secshr_client: non-zero response from gtmsecshr - request: %d  retcode: %d\n",
				  req_code, ret_code));
			if (INVALID_COMKEY == ret_code)
			{	/* Comkey mismatch means for a different version of GT.M - we will not handle it */
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(13) ERR_GTMSECSHRSRVFIL, 7, RTS_ERROR_TEXT("Client"),
					 process_id, mesg.pid, req_code, RTS_ERROR_TEXT(mesg.mesg.path),
					 ERR_TEXT, 2, RTS_ERROR_STRING("Communicating with wrong GT.M version"));
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(13) ERR_GTMSECSHRSRVFIL, 7, RTS_ERROR_TEXT("Client"),
					  process_id, mesg.pid, req_code, RTS_ERROR_TEXT(mesg.mesg.path),
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
		ret_code = -1;
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(10) ERR_GTMSECSHRSRVF, 4,
				RTS_ERROR_TEXT("Client"), process_id, loop_count - 1,
			   	ERR_TEXT, 2, RTS_ERROR_TEXT("Unable to communicate with gtmsecshr"));
		/* If gtm_tmp is not defined, show default path */
		if (gtm_tmp_ptr = GETENV("gtm_tmp"))
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_GTMSECSHRTMPPATH, 2,
				RTS_ERROR_TEXT(gtm_tmp_ptr),
				ERR_TEXT, 2, RTS_ERROR_TEXT("(from $gtm_tmp)"));
		else
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4)
					ERR_GTMSECSHRTMPPATH, 2, RTS_ERROR_TEXT("/tmp"));
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

	FORK(child_pid);	/* BYPASSOK: we exec immediately, no FORK_CLEAN needed */
	if (0 == child_pid)
	{
		process_id = getpid();
		/* Do exec using gtmsecshr_path, which was initialize in file check code - send_mesg2gtmsecshr */
		status = EXECL(gtmsecshr_path, gtmsecshr_path, 0);
		if (-1 == status)
		{
                        send_msg_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_GTMSECSHRSTART, 3, RTS_ERROR_TEXT("Client"), process_id,
				 ERR_TEXT, 2, RTS_ERROR_STRING(secshrstart_error_code[INVTRANSGTMSECSHR]));
			exit(UNABLETOEXECGTMSECSHR);
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
				}
			}
		}
	}
	return status;
}
