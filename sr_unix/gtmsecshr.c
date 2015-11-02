/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "main_pragma.h"

#include <sys/time.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include "gtm_stat.h"
#include "gtm_socket.h"
#include <sys/un.h>
#include <signal.h>
#if !defined(_AIX) && !defined(__linux__) && !defined(__hpux) && !defined(__CYGWIN__) && !defined(__MVS__)
#include <siginfo.h>
#endif
#include "gtm_stdlib.h"
#include "gtm_sem.h"
#include "gtm_string.h"
#include "gtm_fcntl.h"
#include <errno.h>
#include "gtm_time.h"
#include "gtm_unistd.h"
#include "gtm_stdio.h"
#include "gtm_permissions.h"

#if defined(__MVS__)
#include "gtm_zos_io.h"
#endif

#include "cli.h"
#include "error.h"
#include "gtm_logicals.h"
#include "io.h"
#include "gtmsecshr.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "mutex.h"
#include "iosp.h"
#include "gt_timer.h"
#include "gtm_c_stack_trace.h"
#include "eintr_wrappers.h"
#include "eintr_wrapper_semop.h"
#include "gtmimagename.h"
#include "util.h"
#include "send_msg.h"
#include "generic_signal_handler.h"
#include "gtmmsg.h"
#include "have_crit.h"
#include "trans_log_name.h"
#include "sig_init.h"
#include "gtmio.h"
#include "file_head_read.h"
#include "file_head_write.h"
#include "suspsigs_handler.h"
#include "gtm_env_init.h"	/* for gtm_env_init() prototype */
#include "gtm_imagetype_init.h"
#include "gtm_threadgbl_init.h"
#include "fork_init.h"

#ifdef UNICODE_SUPPORTED
#include "gtm_icu_api.h"
#include "gtm_utf8.h"
#endif

#define TIME_FORMAT	"_%Y%j%H%M%S"	/* .yearjuliendayhoursminutesseconds */

GBLDEF CLI_ENTRY	*cmd_ary = NULL; /* GTMSECSHR does not have any command tables so initialize command array to NULL */

GBLREF	int			gtmsecshr_log_file;
GBLREF	int			gtmsecshr_sockfd;
GBLREF	struct sockaddr_un	gtmsecshr_sock_name;
GBLREF	int			gtmsecshr_sockpath_len;
GBLREF	key_t			gtmsecshr_key;
GBLREF	uint4			process_id;
GBLREF	boolean_t		need_core;

static	volatile int		gtmsecshr_timer_popped;
static	volatile boolean_t	ready_to_switch_log_file = FALSE;
static 	int4			times_to_switch_log_file = 0;
static	int			gtmsecshr_logpath_len;
static	int			gtmsecshr_socket_dir_len;
static	char			gtmsecshr_logpath[MAX_TRANS_NAME_LEN];

void clean_client_sockets(char *path);
void gtmsecshr_timer_handler(void);
void gtmsecshr_signal_handler(int sig, siginfo_t *info, void *context);
ZOS_ONLY(boolean_t gtm_tag_error(char *filename, int realtag, int desiredtag);)

error_def(ERR_ASSERT);
error_def(ERR_BADTAG);
error_def(ERR_DBFILOPERR);
error_def(ERR_DBNOTGDS);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMCHECK);
error_def(ERR_GTMSECSHR);
error_def(ERR_GTMSECSHRCHDIRF);
error_def(ERR_GTMSECSHRDEFLOG);
error_def(ERR_GTMSECSHRFORKF);
error_def(ERR_GTMSECSHRLOGF);
error_def(ERR_GTMSECSHRLOGSWH);
error_def(ERR_GTMSECSHROPCMP);
error_def(ERR_GTMSECSHRRECVF);
error_def(ERR_GTMSECSHRSCKSEL);
error_def(ERR_GTMSECSHRSENDF);
error_def(ERR_GTMSECSHRSGIDF);
error_def(ERR_GTMSECSHRSOCKET);
error_def(ERR_GTMSECSHRSRVF);
error_def(ERR_GTMSECSHRSRVFID);
error_def(ERR_GTMSECSHRSRVFIL);
error_def(ERR_GTMSECSHRSSIDF);
error_def(ERR_GTMSECSHRSTART);
error_def(ERR_GTMSECSHRSUIDF);
error_def(ERR_GTMSECSHRTMOUT);
error_def(ERR_GTMSECSHRTMPPATH);
error_def(ERR_LOGTOOLONG);
error_def(ERR_MEMORY);
error_def(ERR_OUTOFSPACE);
error_def(ERR_STACKOFLOW);
error_def(ERR_TEXT);

/* Note that this condition handler is not really properly setup as a condition handler
   in that it has none of the required condition handler macros in it. It's job is just
   to perform shutdown logic when it is called. No further handlers are called hence
   the streamlined nature.
*/
CONDITION_HANDLER(gtmsecshr_cond_hndlr)
{
	gtmsecshr_exit(arg, DUMPABLE ? TRUE : FALSE);
}

/*	If there was a leftover socket, the client will append a lower case letter
	which we take as a flag to delete all sockets for the current client pid
*/
void clean_client_sockets(char *path)
{
	char		last, suffix;
	int		len;

	len = STRLEN(path);
	last = path[len - 1];
	for (suffix = 'a'; last > suffix; suffix++)
	{
		path[len - 1] = suffix;		/* OK since main loop is done with this */
		UNLINK(path);			/* We could care less about errors */
	}
	path[len - 1] = '\0';			/* The normal name for the pid */
	UNLINK(path);
	return;
}

/* For gtmsecshr, override this routine since crit doesn't exist here */
uint4 have_crit(uint4 crit_state)
{
	return 0;
}
/* For gtmsecshr, override this routine so no fork is done */
void gtm_fork_n_core(void)
{
}

int main(void)
{
	int			serv_fail = 0;
	int			selstat;
	int			loop_limit = 0;
	int			save_errno;
	int			mesg_len;
	int			recd, sent;
	int			recv_len, send_len;
	int			recv_complete, send_complete;
	int			num_chars_recd, num_chars_sent;
	int4			msec_timeout;			/* timeout in milliseconds */
	TID			timer_id;
	GTM_SOCKLEN_TYPE	client_addr_len;
	char			*recv_ptr, *send_ptr;
	fd_set			wait_on_fd;
	struct sockaddr_un	client_addr;
	struct timeval		input_timeval;
	gtmsecshr_mesg		mesg;
	DCL_THREADGBL_ACCESS;

	GTM_THREADGBL_INIT;
	gtm_imagetype_init(GTMSECSHR_IMAGE); /* Side-effect : Sets skip_dbtriggers to TRUE for trigger non-supporting platforms */
	gtm_wcswidth_fnptr = NULL;
	gtm_env_init();	/* read in all environment variables */
	err_init(gtmsecshr_cond_hndlr);
	gtmsecshr_init();
	input_timeval.tv_sec  = MAX_TIMEOUT_VALUE;
	input_timeval.tv_usec = 0;
	timer_id = (TID)main;
	while (!loop_limit)
	{
		FD_ZERO(&wait_on_fd);
		FD_SET(gtmsecshr_sockfd, &wait_on_fd);

		gtmsecshr_timer_popped = FALSE;
		if (times_to_switch_log_file > 0)
		{
			gtmsecshr_switch_log_file(0);
			times_to_switch_log_file = 0;
		}
		ready_to_switch_log_file = TRUE;
		SELECT(gtmsecshr_sockfd+1, (void *)&wait_on_fd, (void *)NULL, (void *)NULL, &input_timeval, selstat);
		ready_to_switch_log_file = FALSE;
		if (0 > selstat)
			rts_error(VARLSTCNT(6) ERR_GTMSECSHR, 1, process_id, ERR_GTMSECSHRSCKSEL, 0, errno);
		else if (0 == selstat)
		{
			gtm_putmsg(VARLSTCNT(1) ERR_GTMSECSHRTMOUT);
			loop_limit = 1;
			gtmsecshr_exit(0,0);	/* doesn't return */
		}
		recd = 0;
		mesg_len = 0;
		recv_ptr = (char *)&mesg;
		recv_len = SIZEOF(mesg);
		recv_complete = 0;
		client_addr_len = SIZEOF(struct sockaddr_un);
		msec_timeout = timeout2msec(GTMSECSHR_MESG_TIMEOUT);
		start_timer(timer_id, msec_timeout, gtmsecshr_timer_handler, 0, NULL);
		while (!recv_complete)
		{
			num_chars_recd = (int)(RECVFROM(gtmsecshr_sockfd, (void *)recv_ptr, recv_len, 0,
							(struct sockaddr *)&client_addr, &client_addr_len));
			if ((-1 == num_chars_recd) && (gtmsecshr_timer_popped || EINTR != errno))
			{
				rts_error(VARLSTCNT(6) ERR_GTMSECSHR, 1, process_id, ERR_GTMSECSHRRECVF, 0, errno);
				assert(FALSE);	/* the above rts_error should never return */
			} else if (0 == num_chars_recd)
				recv_complete = 1;
			else
			{
				recd += num_chars_recd;
				if ((0 == mesg_len) && (SIZEOF(int) <= recd))
					mesg_len = mesg.len;
				if (recd == mesg_len)
					recv_complete = 1;
				else
				{
					recv_ptr += num_chars_recd;
					recv_len -= num_chars_recd;
				}
			}
		}
		cancel_timer(timer_id);

		sent = send_complete = 0;
		serv_fail = service_request(&mesg);
		send_len = mesg_len = mesg.len;  /* We may not need to send same message size. Only return code is enough ??? */
		send_ptr = (char *)&mesg;
		msec_timeout = timeout2msec(GTMSECSHR_MESG_TIMEOUT);
		start_timer(timer_id, msec_timeout, gtmsecshr_timer_handler, 0, NULL);
		while (!send_complete)
		{
			num_chars_sent = (int)(SENDTO(gtmsecshr_sockfd, send_ptr, send_len, 0,
						(struct sockaddr *)&client_addr, (GTM_SOCKLEN_TYPE)client_addr_len));
			if ((-1 == num_chars_sent) && (gtmsecshr_timer_popped || errno != EINTR))
			{
				 rts_error(VARLSTCNT(6) ERR_GTMSECSHR, 1, process_id, ERR_GTMSECSHRSENDF, 0, errno);
				 send_complete = 1;
			} else
			{
				sent += num_chars_sent;
				if (sent == mesg_len)
					send_complete = 1;
				else
				{
					send_ptr += num_chars_sent;
					send_len -= num_chars_sent;
				}
			}
		}
		cancel_timer(timer_id);

		/* Note :- The condition serv_fail should happen only when gtmsecshr does
		 * not have permission to service a request and should happen only when
		 * the gtmsecshr is not installed set-uid to root.
		 */
		if (serv_fail)
			gtmsecshr_exit(serv_fail, FALSE);

		assert('a' > 'F' && 'a' > '9');
		if ('a' <= client_addr.sun_path[strlen(client_addr.sun_path) - 1])
			clean_client_sockets(client_addr.sun_path);
	}
}

void gtmsecshr_init(void)
{
	int		file_des, save_errno, len = 0;
	int		create_attempts = 0;
	int		secshr_sem;
	int		semop_res;
	char		*name_ptr;
	now_t		now;	/* for GET_CUR_TIME macro */
	char		time_str[CTIME_BEFORE_NL + 2], *time_ptr; /* for GET_CUR_TIME macro */
	pid_t		pid;
	struct sembuf	sop[4];
	gtmsecshr_mesg	mesg;
	struct stat	stat_buf;
	char		*gtm_tmp_ptr;
	int		rc;
	int		lib_gid;
	struct stat	dist_stat_buff;

	process_id = getpid();
	set_blocksig();
	if (-1 == setuid(ROOTUID))
	{
		send_msg(VARLSTCNT(10) MAKE_MSG_WARNING(ERR_GTMSECSHRSTART), 3, RTS_ERROR_LITERAL("Server"), process_id,
				ERR_GTMSECSHRSUIDF, 0, ERR_GTMSECSHROPCMP, 0, errno);
		gtmsecshr_exit(SETUIDROOT, FALSE);
	}
	pid = getsid(process_id);
	if ((pid != process_id) && ((pid_t)-1 == setsid()))
	{
		save_errno = errno;
		send_msg(VARLSTCNT(8) MAKE_MSG_WARNING(ERR_GTMSECSHRSTART), 3, RTS_ERROR_LITERAL("Server"), process_id,
				ERR_GTMSECSHRSSIDF, 0, save_errno);
	}
	gtmsecshr_open_log_file();
	DO_FORK(pid);
	if (0 > pid)
	{
		save_errno = errno;
		send_msg(VARLSTCNT(8) ERR_GTMSECSHRSTART, 3, RTS_ERROR_LITERAL("Server"), process_id,
			ERR_GTMSECSHRFORKF, 0, save_errno);
		gtm_putmsg(VARLSTCNT(8) ERR_GTMSECSHRSTART, 3, RTS_ERROR_LITERAL("Server"), process_id,
			ERR_GTMSECSHRFORKF, 0, save_errno);
		exit (GNDCHLDFORKFLD);
	} else if (0 < pid)
		exit(0);
	process_id = getpid();
	GET_CUR_TIME;
	util_out_print("gtmsecshr started at !AD", TRUE, RTS_ERROR_STRING(time_ptr));
	gtmsecshr_sig_init();
	CLOSEFILE(0, rc);
	for (file_des = (int)sysconf(_SC_OPEN_MAX)-1; file_des >= 3; file_des--)
	{	/* Close the file only if we have it open. This is to avoid a CLOSEFAIL error in case of
		 * trying to close an invalid file descriptor.
		 */
		CLOSEFILE_IF_OPEN(file_des, rc);
	}
	if (-1 == CHDIR(P_tmpdir))
	{
		save_errno = errno;
		send_msg(VARLSTCNT(10) ERR_GTMSECSHRSTART, 3, RTS_ERROR_LITERAL("Server"), process_id,
			ERR_GTMSECSHRCHDIRF, 2, LEN_AND_STR(P_tmpdir), save_errno);
		gtm_putmsg(VARLSTCNT(10) ERR_GTMSECSHRSTART, 3, RTS_ERROR_LITERAL("Server"), process_id,
			ERR_GTMSECSHRCHDIRF, 2, LEN_AND_STR(P_tmpdir), save_errno);
		exit (UNABLETOCHDIR);
	}
	umask(0);
	if (gtmsecshr_pathname_init(SERVER) != 0)
	{
		send_msg(VARLSTCNT(5) ERR_GTMSECSHRSOCKET, 3, RTS_ERROR_LITERAL("Server path"), process_id);
		rts_error(VARLSTCNT(5) ERR_GTMSECSHRSOCKET, 3, RTS_ERROR_LITERAL("Server path"), process_id);
	}
	if (-1 == (secshr_sem = semget(gtmsecshr_key, FTOK_SEM_PER_ID, RWDALL | IPC_NOWAIT)))
	{
		secshr_sem = INVALID_SEMID;
		if (ENOENT != errno ||		/* below will fail otherwise */
		    -1 == (secshr_sem = semget(gtmsecshr_key, FTOK_SEM_PER_ID, RWDALL | IPC_CREAT | IPC_NOWAIT | IPC_EXCL)))
		{
			secshr_sem = INVALID_SEMID;
			util_out_print("semget error errno = !UL", TRUE, errno);
			gtmsecshr_exit(SEMGETERROR, FALSE);
		}
	}
	sop[0].sem_num = 0;
	sop[0].sem_op = 0;
	sop[0].sem_flg  = IPC_NOWAIT;
	sop[1].sem_num = 0;
	sop[1].sem_op = 1;
	sop[1].sem_flg  = IPC_NOWAIT | SEM_UNDO;
	SEMOP(secshr_sem, sop, 2, semop_res, NO_WAIT);
	if (0 > semop_res)
	{
		send_msg(VARLSTCNT(10) MAKE_MSG_SEVERE(ERR_GTMSECSHRSTART), 3, RTS_ERROR_LITERAL("Server"), process_id,
			ERR_TEXT, 2, RTS_ERROR_LITERAL("server already running"), errno);
		/* if gtm_tmp is not defined, show default path */
		if (gtm_tmp_ptr = GETENV("gtm_tmp"))
			send_msg(VARLSTCNT(8) ERR_GTMSECSHRTMPPATH, 2, RTS_ERROR_TEXT(gtm_tmp_ptr),
				ERR_TEXT, 2, RTS_ERROR_TEXT("(from $gtm_tmp)"));
		else
			send_msg(VARLSTCNT(4) ERR_GTMSECSHRTMPPATH, 2, RTS_ERROR_TEXT("/tmp"));
		util_out_print("A gtmsecshr server is already running - exiting", TRUE);
		gtmsecshr_exit(SEMAPHORETAKEN, FALSE);
	}
	if (gtmsecshr_sock_init(SERVER) != 0)
	{
		send_msg(VARLSTCNT(5) ERR_GTMSECSHRSOCKET, 3, RTS_ERROR_LITERAL("Server"), process_id);
		rts_error(VARLSTCNT(5) ERR_GTMSECSHRSOCKET, 3, RTS_ERROR_LITERAL("Server"), process_id);
	}
	if (-1 == Stat(gtmsecshr_sock_name.sun_path, &stat_buf))
		gtm_putmsg(VARLSTCNT(10) MAKE_MSG_WARNING(ERR_GTMSECSHRSTART), 3, RTS_ERROR_LITERAL("Server"), process_id,
				ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to get status of socket file"), errno);
	/* Get the distribution group */
	lib_gid = gtm_get_group_id(&dist_stat_buff);
	/* if it is world accessible then make mode 666 */
	if ((-1 != lib_gid) && (dist_stat_buff.st_mode & 04))
		lib_gid = -1;
	if (-1 == lib_gid)
		stat_buf.st_mode = 0666;
	else
	{
		/* change group if different from current user group */
		if (lib_gid != GETGID() && (-1 == CHOWN(gtmsecshr_sock_name.sun_path, -1, lib_gid)))
		{
			gtm_putmsg(VARLSTCNT(10) MAKE_MSG_WARNING(ERR_GTMSECSHRSTART), 3,
				   RTS_ERROR_LITERAL("Server"), process_id, ERR_TEXT, 2,
				   RTS_ERROR_LITERAL("Unable to change socket file group"), errno);
		}
		/* change mode to 660 */
		stat_buf.st_mode = 0660;
	}
	if (-1 == CHMOD(gtmsecshr_sock_name.sun_path, stat_buf.st_mode))
		gtm_putmsg(VARLSTCNT(10) MAKE_MSG_WARNING(ERR_GTMSECSHRSTART), 3, RTS_ERROR_LITERAL("Server"), process_id,
				ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to change socket file permisions"), errno);
	name_ptr = strrchr(gtmsecshr_sock_name.sun_path, '/');
	while (*name_ptr == '/')	/* back off in case of double-slash */
		name_ptr--;
	gtmsecshr_socket_dir_len = (int)(name_ptr - gtmsecshr_sock_name.sun_path + 1);
	/* Preallocate some timer blocks. */
	prealloc_gt_timers();
	return;
}

void gtmsecshr_exit (int exit_code, boolean_t dump)
{
	int		gtmsecshr_sem;
	now_t		now;	/* for GET_CUR_TIME macro */
	char		time_str[CTIME_BEFORE_NL + 2], *time_ptr; /* for GET_CUR_TIME macro */

	if (dump)
		DUMP_CORE;
	gtmsecshr_sock_cleanup(SERVER);
	gtmsecshr_sockfd = FD_INVALID;
	if (SEMAPHORETAKEN != exit_code)
	{	/* remove semaphore */
		if (-1 == (gtmsecshr_sem = semget(gtmsecshr_key, FTOK_SEM_PER_ID, RWDALL | IPC_NOWAIT)))
		{
			gtmsecshr_sem = INVALID_SEMID;
			util_out_print("error getting semaphore errno = !UL", TRUE, errno);
		}
		if (-1 == semctl(gtmsecshr_sem, 0, IPC_RMID, 0))
			util_out_print("error removing semaphore errno = !UL", TRUE, errno);
	}
	util_out_print("", TRUE);
	GET_CUR_TIME;
	util_out_print("gtmsecshr exited on !AD", TRUE, RTS_ERROR_STRING(time_ptr));
	util_out_close();
	exit(exit_code);
}

int gtmsecshr_open_log_file (void)
{
	struct stat 	buf;
	int     	save_errno = 0, status;
	char    	gtmsecshr_path[MAX_TRANS_NAME_LEN], *error_mesg;
	mstr    	gtmsecshr_lognam, gtmsecshr_transnam;
#	ifdef __MVS__
	int		create_logfile = 0, realfiletag;
#	endif

	gtmsecshr_lognam.addr = GTMSECSHR_LOG_DIR;
	gtmsecshr_lognam.len = SIZEOF(GTMSECSHR_LOG_DIR) - 1;
	status = TRANS_LOG_NAME(&gtmsecshr_lognam, &gtmsecshr_transnam, gtmsecshr_logpath, SIZEOF(gtmsecshr_logpath),
					dont_sendmsg_on_log2long);
	if ((SS_NORMAL != status) || !ABSOLUTE_PATH(gtmsecshr_logpath))
	{	/* gtm_log not defined or not defined to absolute path, use default gtm_log
		 * This is not considered error, just informational
		 */
		if (SS_LOG2LONG == status)
			send_msg(VARLSTCNT(5) ERR_LOGTOOLONG, 3, gtmsecshr_lognam.len, gtmsecshr_lognam.addr,
				SIZEOF(gtmsecshr_logpath) - 1);
		send_msg(VARLSTCNT(4) ERR_GTMSECSHRDEFLOG, 2, RTS_ERROR_TEXT(DEFAULT_GTMSECSHR_LOG_DIR));
		strcpy(gtmsecshr_logpath, DEFAULT_GTMSECSHR_LOG_DIR);
		gtmsecshr_logpath_len = SIZEOF(DEFAULT_GTMSECSHR_LOG_DIR) - 1;
	} else
		gtmsecshr_logpath_len = gtmsecshr_transnam.len;
	if (-1 == Stat(gtmsecshr_logpath, &buf))
	{
		save_errno = errno;
		send_msg(VARLSTCNT(14) ERR_GTMSECSHRLOGF, 3, RTS_ERROR_TEXT("Server"), process_id, ERR_TEXT, 2,
					RTS_ERROR_TEXT("Unable to locate default log directory"), ERR_TEXT, 2,
					RTS_ERROR_STRING(gtmsecshr_logpath), save_errno);
		exit(UNABLETOOPNLOGFILEFTL);
	} else if (!S_ISDIR(buf.st_mode))
	{
		send_msg(VARLSTCNT(13) ERR_GTMSECSHRLOGF, 3, RTS_ERROR_LITERAL("Server"), process_id, ERR_TEXT, 2,
					RTS_ERROR_LITERAL("$gtm_log not a directory"), ERR_TEXT, 2,
					RTS_ERROR_STRING(gtmsecshr_logpath));
		exit(UNABLETOOPNLOGFILEFTL);
	}
	if (gtmsecshr_logpath[gtmsecshr_logpath_len - 1] != '/')
		gtmsecshr_logpath[gtmsecshr_logpath_len++] = '/';
	strcpy(gtmsecshr_logpath + gtmsecshr_logpath_len , GTMSECSHR_LOG_PREFIX);
#	if defined(__MVS__)
	if (-1 == Stat(gtmsecshr_logpath, &buf))
		create_logfile = 1;
#endif
	if (0 > (gtmsecshr_log_file = OPEN3(gtmsecshr_logpath, O_RDWR|O_CREAT|O_APPEND, GTMSECSHR_PERMS)))
	{
		send_msg(VARLSTCNT(14) ERR_GTMSECSHRLOGF, 3, RTS_ERROR_LITERAL("Server"), process_id, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("gtmsecshr unable to open log file"),
				ERR_TEXT, 2, RTS_ERROR_STRING(gtmsecshr_logpath), errno);
		exit(UNABLETOOPNLOGFILEFTL);
	}
#	if defined(__MVS__)
	if ((1 == create_logfile) && (-1 == gtm_zos_set_tag(gtmsecshr_log_file, TAG_EBCDIC, TAG_TEXT, TAG_FORCE, &realfiletag)))
			gtm_tag_error(gtmsecshr_logpath, realfiletag, TAG_EBCDIC);
	else if (!gtm_zos_autocvt_enabled())
	{
		send_msg(VARLSTCNT(14) ERR_GTMSECSHRLOGF, 3, RTS_ERROR_LITERAL("Server"), process_id, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("_BPXK_AUTOCVT='ON' not set, policy tagging the log file"),
				ERR_TEXT, 2, RTS_ERROR_STRING(gtmsecshr_logpath), errno);
		if (-1 == gtm_zos_tag_to_policy(gtmsecshr_log_file, TAG_UNTAGGED, &realfiletag))
			gtm_tag_error(gtmsecshr_logpath, realfiletag, TAG_UNTAGGED);
	}
#endif
	assert(0 <= gtmsecshr_log_file);
	if (-1 == dup2(gtmsecshr_log_file, 1))
	{
		send_msg(VARLSTCNT(10) ERR_GTMSECSHRLOGF, 3, RTS_ERROR_LITERAL("Server"), process_id, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Unable to dup standard out"), errno);
		exit(UNABLETODUPLOG);
	}
	if (-1 == dup2(gtmsecshr_log_file, 2))
	{
		send_msg(VARLSTCNT(10) ERR_GTMSECSHRLOGF, 3, RTS_ERROR_LITERAL("Server"), process_id, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Unable to dup standard error"), errno);
		exit(UNABLETODUPLOG);
	}
	return 0;
}

void gtmsecshr_switch_log_file(int sig)
{
	now_t		now;	/* for GET_CUR_TIME macro */
	char		time_str[CTIME_BEFORE_NL + 2], *time_ptr; /* for GET_CUR_TIME macro */
	char		newname[MAX_TRANS_NAME_LEN], *err_msg;
	struct tm       *tm_struct;
	size_t		dummy;
	struct stat	fs;
	int		newname_len, suffix, save_errno, temp_fd, rc;
	ZOS_ONLY(int	realfiletag;)

	assert((SIGHUP == sig) || (0 == sig)); 	/* sig could be either SIGHUP or 0 */
	assert((SIGHUP == sig) || (!ready_to_switch_log_file));

	if ((SIGHUP == sig) && (!ready_to_switch_log_file))
	{
		times_to_switch_log_file++;
		return;
	}
	/* --- log this switching action in the old log file --- */
	GET_CUR_TIME;
	util_out_print("!/gtmsecshr log was switched from this file at !AD -- process id !UL",
		TRUE, RTS_ERROR_STRING(time_ptr), process_id);
	/* --- construct a name for the old gtmsecshr log file --- */
	strcpy(newname, gtmsecshr_logpath);
	if (((time_t)-1 == (now = time(NULL))) || (!(tm_struct = localtime(&now))))
	{
		SPRINTF(&newname[gtmsecshr_logpath_len + SIZEOF(GTMSECSHR_LOG_PREFIX) - 1],
			".timerrno%d", errno);
	} else
	{
		STRFTIME(&newname[gtmsecshr_logpath_len + SIZEOF(GTMSECSHR_LOG_PREFIX) - 1],
			MAX_TRANS_NAME_LEN - gtmsecshr_logpath_len - SIZEOF(GTMSECSHR_LOG_PREFIX),
			TIME_FORMAT, tm_struct, dummy);
	}
	newname_len = STRLEN(newname);
	suffix = 1;
	while ((0 == Stat(newname, &fs)) || (ENOENT != errno))	/* This file exists */
	{
		SPRINTF(&newname[newname_len], ".%d", suffix);
		suffix++;
	}
	/* --- switch to use the new log file --- */
	if ((-1 == RENAME(gtmsecshr_logpath, &newname[0]))
		|| (FD_INVALID == (temp_fd = OPEN3(gtmsecshr_logpath, O_RDWR | O_CREAT | O_APPEND, GTMSECSHR_PERMS)))
		|| (-1 == dup2(temp_fd, gtmsecshr_log_file))
#	if defined(__MVS__)
		|| (-1 == gtm_zos_set_tag(temp_fd, TAG_EBCDIC, TAG_TEXT, TAG_FORCE, &realfiletag)
		    && gtm_tag_error(gtmsecshr_logpath, realfiletag, TAG_EBCDIC))
		|| ((!gtm_zos_autocvt_enabled()) && ((-1 == gtm_zos_tag_to_policy(gtmsecshr_log_file, TAG_UNTAGGED, &realfiletag))
 						 && gtm_tag_error(gtmsecshr_logpath, realfiletag, TAG_UNTAGGED)))
#endif
		|| (-1 == dup2(temp_fd, 1))
		|| (-1 == dup2(temp_fd, 2)))
	{
		save_errno = errno;
		send_msg(VARLSTCNT(14) ERR_GTMSECSHRLOGSWH, 7, RTS_ERROR_STRING(gtmsecshr_logpath),
				RTS_ERROR_STRING(&newname[0]), RTS_ERROR_LITERAL("rename"), process_id,
				ERR_TEXT, 2, RTS_ERROR_LITERAL("Recycling gtmsecshr is suggested."), save_errno);
		gtm_putmsg(VARLSTCNT(14) ERR_GTMSECSHRLOGSWH, 7, RTS_ERROR_STRING(gtmsecshr_logpath),
				RTS_ERROR_STRING(&newname[0]), RTS_ERROR_LITERAL("rename"), process_id,
				ERR_TEXT, 2, RTS_ERROR_LITERAL("Recycling gtmsecshr is suggested."), save_errno);
		return;
	}
	CLOSEFILE_RESET(gtmsecshr_log_file, rc);	/* resets "gtmsecshr_log_file" to FD_INVALID */
	gtmsecshr_log_file = temp_fd;
	/* --- log this switching action in the new log file --- */
	GET_CUR_TIME;
	util_out_print("!/gtmsecshr log was switched to this file at !AD -- process id !UL",
		TRUE, RTS_ERROR_STRING(time_ptr), process_id);
}

void gtmsecshr_timer_handler(void)
{
	gtmsecshr_timer_popped = TRUE;
}

void gtmsecshr_signal_handler(int sig, siginfo_t *info, void *context)
{
	boolean_t	process_signal(enum gtmImageTypes, int, siginfo_t *, void *);

	/* Do standard signal handling */
	(void)generic_signal_handler(sig, info, context);
	/* Note that we are not letting process_signal run gtm_fork_n_core. In testing,
	   bad things occurred when the root process ran into trouble trying to fork
	   so our object is to avoid the problem entirely and core here if we need to
	   and if the OS will do it (Linux seems not to core if root process). */
	gtmsecshr_exit(sig, need_core);
}

void gtmsecshr_sig_init(void)
{
	struct sigaction	act;

	sig_init(gtmsecshr_signal_handler, gtmsecshr_signal_handler, suspsigs_handler);
	/* Redefine handler for SIGHUP to switch log file */
	memset(&act, 0, SIZEOF(act));
	act.sa_handler  = gtmsecshr_switch_log_file;
	sigaction(SIGHUP, &act, 0);
}

int service_request(gtmsecshr_mesg *buf)
{
	int			ret_status = 0;
	int			fn_len, index, basind, save_errno, save_code;
	int			stat_res;
	now_t			now;	/* for GET_CUR_TIME macro */
	char			time_str[CTIME_BEFORE_NL + 2], *time_ptr; /* for GET_CUR_TIME macro */
	char			*basnam, *fn;
	struct shmid_ds		temp_shmctl_buf;
	struct stat		statbuf;
	sgmnt_data		header;

	GET_CUR_TIME;
	save_code = buf->code;
	switch(buf->code)
	{
	    case WAKE_MESSAGE:
		if (buf->code = (-1 == kill((pid_t )buf->mesg.id, SIGALRM)) ? errno : 0)
		{
			save_errno = errno;
			send_msg(VARLSTCNT(13) ERR_GTMSECSHRSRVFID, 6, RTS_ERROR_LITERAL("Server"), process_id, buf->pid, save_code,
					buf->mesg.id, ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to wake up process"), save_errno);
			util_out_print("!AD [client pid !UL]", TRUE, CTIME_BEFORE_NL, time_ptr, buf->pid);
			gtm_putmsg(VARLSTCNT(13)
				ERR_GTMSECSHRSRVFID, 6, RTS_ERROR_LITERAL("Server"), process_id, buf->pid, save_code, buf->mesg.id,
				ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to wake up process"), save_errno);
		}
		DEBUG_ONLY(
		else
			util_out_print("!AD [client pid !UL] Process !UL was awakened", TRUE, CTIME_BEFORE_NL, time_ptr, buf->pid,
				buf->mesg.id);
		)
		break ;
	    case REMOVE_SEM:
		buf->code = ( -1 == semctl((int )buf->mesg.id, 0, IPC_RMID, 0)) ? errno : 0;
		if (!buf->code)
			util_out_print("!AD [client pid !UL] Semaphore (!UL) removed", TRUE, CTIME_BEFORE_NL, time_ptr, buf->pid,
				buf->mesg.id);
		else
		{
			send_msg(VARLSTCNT(13) ERR_GTMSECSHRSRVFID, 6, RTS_ERROR_LITERAL("Server"), process_id, buf->pid, save_code,
					buf->mesg.id, ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to remove semaphore"), buf->code);
			gtm_putmsg(VARLSTCNT(13)
				ERR_GTMSECSHRSRVFID, 6, RTS_ERROR_LITERAL("Server"), process_id, buf->pid, save_code, buf->mesg.id,
				ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to remove semaphore"), buf->code);
		}
		break;
	    case REMOVE_SHMMEM:
		buf->code = (-1 == shmctl((int )buf->mesg.id, IPC_STAT, &temp_shmctl_buf)) ? errno : 0;
		if (buf->code)
		{
			send_msg(VARLSTCNT(13) ERR_GTMSECSHRSRVFID, 6, RTS_ERROR_LITERAL("Server"), process_id, buf->pid, save_code,
				buf->mesg.id, ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to get shared memory statistics"), buf->code);
			gtm_putmsg(VARLSTCNT(13)
				ERR_GTMSECSHRSRVFID, 6, RTS_ERROR_LITERAL("Server"), process_id, buf->pid, save_code, buf->mesg.id,
				ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to get shared memory statistics"), buf->code);
			break;
		}
		else if (1 < temp_shmctl_buf.shm_nattch)
		{
			util_out_print("More than one process attached to Shared memory segment (!UL) not removed (!UL)", TRUE,
					buf->mesg.id, temp_shmctl_buf.shm_nattch);
			buf->code = EBUSY;
			break;
		}
		buf->code = (-1 == shmctl((int )buf->mesg.id, IPC_RMID, 0)) ? errno : 0;
		if (!buf->code)
			util_out_print("!AD [client pid !UL] Shared memory segment (!UL) removed, nattch = !UL", TRUE,
				CTIME_BEFORE_NL, time_ptr, buf->pid, buf->mesg.id, temp_shmctl_buf.shm_nattch);
		else
		{
			 send_msg(VARLSTCNT(13)
			 	ERR_GTMSECSHRSRVFID, 6, RTS_ERROR_LITERAL("Server"), process_id, buf->pid, save_code, buf->mesg.id,
				ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to remove shared memory segment"), buf->code);
			 gtm_putmsg(VARLSTCNT(13)
			 	ERR_GTMSECSHRSRVFID, 6, RTS_ERROR_LITERAL("Server"), process_id, buf->pid, save_code, buf->mesg.id,
				ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to remove shared memory segment"), buf->code);
		}
		break;
	   case REMOVE_FILE :
#		ifndef MUTEX_MSEM_WAKE
		for (index = 0; index < SIZEOF(buf->mesg.path); index++)
		{
			if ('\0' == buf->mesg.path[index])
				break;
		}
		if ((0 == index) || (index >= SIZEOF(buf->mesg.path)))
		{
			gtm_putmsg(VARLSTCNT(13) ERR_GTMSECSHRSRVFIL, 7, RTS_ERROR_LITERAL("Server"), process_id, buf->pid,
				buf->code, index >= SIZEOF(buf->mesg.path) ? SIZEOF(buf->mesg.path) - 1 : index,
				buf->mesg.path, ERR_TEXT, 2, RTS_ERROR_LITERAL("no file or length too long"));
			buf->code = EINVAL;
		} else
		{
			if ('/' == buf->mesg.path[index - 1] && 1 < index)
			{	/* remove trailing slash unless only slash */
				buf->mesg.path[index - 1] = '\0';
				index--;
			}
			for (basind = index - 1; 0 <= basind; basind--)
			{
				if ('/' == buf->mesg.path[basind])
					break;
			}
			if (0 < basind || (0 == basind && '/' == buf->mesg.path[basind]))
				basnam = &buf->mesg.path[basind + 1];
			else
				basnam = &buf->mesg.path[0];
			STAT_FILE(buf->mesg.path, &statbuf, stat_res);
			if (-1 == stat_res)
			{
				buf->code = errno;
				gtm_putmsg(VARLSTCNT(14) ERR_GTMSECSHRSRVFIL, 7,
					RTS_ERROR_LITERAL("Server"), process_id, buf->pid, buf->code,
					index >= SIZEOF(buf->mesg.path) ? SIZEOF(buf->mesg.path) - 1 : index, buf->mesg.path,
					ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to get file status"), errno);
#			ifndef __MVS__
			} else if (!S_ISSOCK(statbuf.st_mode))
#			else
			} else if (!(S_ISSOCK(statbuf.st_mode) || S_ISCHR(statbuf.st_mode)))
#			endif
			{
				gtm_putmsg(VARLSTCNT(13) ERR_GTMSECSHRSRVFIL, 7,
					RTS_ERROR_LITERAL("Server"), process_id, buf->pid, buf->code,
					index >= SIZEOF(buf->mesg.path) ? SIZEOF(buf->mesg.path) - 1 : index, buf->mesg.path,
					ERR_TEXT, 2, RTS_ERROR_LITERAL("File is not a GTM mutex socket file"));
				buf->code = EINVAL;
			} else if (0 != MEMCMP_LIT(basnam, MUTEX_SOCK_FILE_PREFIX))
			{
				gtm_putmsg(VARLSTCNT(13) ERR_GTMSECSHRSRVFIL, 7,
			  	 RTS_ERROR_LITERAL("Server"), process_id, buf->pid,
				 buf->code, index >= SIZEOF(buf->mesg.path) ? SIZEOF(buf->mesg.path) - 1 : index,
				 buf->mesg.path, ERR_TEXT, 2,
				 RTS_ERROR_LITERAL("File name does not match the naming convention for a GT.M mutex socket file"));
				buf->code = EINVAL;
			} else if (0 != memcmp(gtmsecshr_sock_name.sun_path, buf->mesg.path, gtmsecshr_socket_dir_len))
			{
				gtm_putmsg(VARLSTCNT(13) ERR_GTMSECSHRSRVFIL, 7,
				 RTS_ERROR_LITERAL("Server"), process_id, buf->pid,
				 buf->code, index >= SIZEOF(buf->mesg.path) ? SIZEOF(buf->mesg.path) - 1 : index,
				 buf->mesg.path, ERR_TEXT, 2,
				 RTS_ERROR_LITERAL("File does not reside in the normal directory for a GT.M mutex socket file"));
				buf->code = EINVAL;
			} else if (buf->code = (-1 == UNLINK(buf->mesg.path)) ? errno : 0)
			{
				gtm_putmsg(VARLSTCNT(14) ERR_GTMSECSHRSRVFIL, 7, RTS_ERROR_LITERAL("Server"), process_id,
					buf->pid, save_code, RTS_ERROR_STRING(buf->mesg.path),
					ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to remove file"), buf->code);
			} else
			{
				util_out_print("!AD [client pid !UL] File (!AD) removed", TRUE, CTIME_BEFORE_NL, time_ptr, buf->pid,
					RTS_ERROR_STRING(buf->mesg.path));
			}
		}
#		else
		/* For platforms that use MSEMs for mutex inter process communication, mutex socket files are not used
		 * by GT.M so the need to remove those (i.e. using the REMOVE_FILE command should not be necessary).
		 */
	   	assert(FALSE);
#		endif
		break;
	    case CONTINUE_PROCESS:
		if (buf->code = (-1 == kill((pid_t)buf->mesg.id, SIGCONT)) ? errno : 0)
		{
			save_errno = errno;
			send_msg(VARLSTCNT(13)
				ERR_GTMSECSHRSRVFID, 6, RTS_ERROR_LITERAL("Server"), process_id, buf->pid, save_code, buf->mesg.id,
				ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to request process to resume processing"), save_errno);
			util_out_print("!AD [client pid !UL]", TRUE, CTIME_BEFORE_NL, time_ptr, buf->pid);
			gtm_putmsg(VARLSTCNT(13)
				ERR_GTMSECSHRSRVFID, 6, RTS_ERROR_LITERAL("Server"), process_id, buf->pid, save_code, buf->mesg.id,
				ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to request process to resume processing"), save_errno);
		}
		DEBUG_ONLY(
		else
			util_out_print("!AD [client pid !UL] Process !UL was requested to resume processing", TRUE, CTIME_BEFORE_NL,
				time_ptr, buf->pid, buf->mesg.id);
		)
		break ;
	   case FLUSH_DB_IPCS_INFO:
		fn = buf->mesg.db_ipcs.fn;
		fn_len = buf->mesg.db_ipcs.fn_len;
		*(fn + fn_len) = 0;	/* We assumed we have one extra byte. fn must be null terminated */
		if (!file_head_read(fn, &header, SIZEOF(header)))
		{
			buf->code = errno;
			send_msg(VARLSTCNT(12) ERR_GTMSECSHRSRVFID, 6, RTS_ERROR_LITERAL("Server"), process_id, buf->pid, save_code,
				buf->mesg.id, ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to read database file header"));
			break;
		}
		header.semid = buf->mesg.db_ipcs.semid;
		header.shmid = buf->mesg.db_ipcs.shmid;
		header.gt_sem_ctime.ctime = buf->mesg.db_ipcs.gt_sem_ctime;
		header.gt_shm_ctime.ctime = buf->mesg.db_ipcs.gt_shm_ctime;
		if (!file_head_write(fn, &header, SIZEOF(header)))
		{
			buf->code = errno;
			send_msg(VARLSTCNT(12) ERR_GTMSECSHRSRVFID, 6, RTS_ERROR_LITERAL("Server"), process_id, buf->pid, save_code,
				buf->mesg.id, ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to write database file header"));
		}
		util_out_print("!AD [client pid !UL] database fileheader (!AD) updated", TRUE, CTIME_BEFORE_NL, time_ptr, buf->pid,
			fn_len, fn);
		buf->code = 0;
		break;
	   default:
		buf->ack = ACK_NOT_REQUIRED;
		send_msg(VARLSTCNT(12) ERR_GTMSECSHRSRVFID, 6, RTS_ERROR_LITERAL("Server"), process_id, buf->pid, buf->code,
				buf->mesg.id, ERR_TEXT, 2, RTS_ERROR_LITERAL("Invalid Service Request"));
		send_msg(VARLSTCNT(12) ERR_GTMSECSHRSRVFID, 6, RTS_ERROR_LITERAL("Server"), process_id, buf->pid, buf->code,
				buf->mesg.id, ERR_TEXT, 2, RTS_ERROR_LITERAL("Invalid Service Request"));
		buf->code = 0;
	}
	return ret_status;
}

#ifdef __MVS__
boolean_t gtm_tag_error(char *filename, int realtag, int desiredtag)
{
	char *errmsg = STRERROR(errno);
	send_msg(VARLSTCNT(10) ERR_BADTAG, 4, LEN_AND_STR(filename), realtag,
			desiredtag, ERR_TEXT, 2, RTS_ERROR_STRING(errmsg));
	return 0;
}
#endif
