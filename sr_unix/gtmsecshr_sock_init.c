/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include <sys/un.h>

#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_ipc.h"
#include "gtm_stat.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_socket.h"
#include "gtm_limits.h"

#include "gtm_logicals.h"
#include "io.h"
#include "error.h"
#include "gtmsecshr.h"
#include "iosp.h"
#include "send_msg.h"
#include "getjobnum.h"
#include "gtmmsg.h"
#include "trans_log_name.h"
#include "eintr_wrappers.h"
#include "gtm_permissions.h"

GBLREF struct sockaddr_un 	gtmsecshr_sock_name;
GBLREF struct sockaddr_un 	gtmsecshr_cli_sock_name;
GBLREF key_t			gtmsecshr_key;
GBLREF int 			gtmsecshr_sockpath_len;
GBLREF int 			gtmsecshr_cli_sockpath_len;
GBLREF mstr 			gtmsecshr_pathname;
GBLREF boolean_t		gtmsecshr_sock_init_done;
GBLREF uint4			process_id;
GBLREF int			gtmsecshr_sockfd;

static char			gtmsecshr_sockpath[GTM_PATH_MAX];
static char			gtmsecshr_path[GTM_PATH_MAX];
static char hex_table[] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

unsigned char		*mypid2ascx(unsigned char *, pid_t);

#ifndef SUN_LEN
#  define SUN_LEN(x)	SIZEOF(*x)
#else
#  define EXACT_SIZE_SOCKNAME
#endif

error_def(ERR_GTMSECSHRSOCKET);
error_def(ERR_LOGTOOLONG);
error_def(ERR_TEXT);

int4 gtmsecshr_pathname_init(int caller, char *execpath, int execpathln)
{
	int			ret_status = 0, status;
	char			*error_mesg;
	mstr			secshrsock_lognam, secshrsock_transnam;
	mstr			gtmsecshr_logname;
	struct stat		buf;
	int4			max_sock_path_len;

	if (!process_id)
		getjobnum();
	secshrsock_lognam.addr = GTMSECSHR_SOCK_DIR;
	secshrsock_lognam.len = SIZEOF(GTMSECSHR_SOCK_DIR) - 1;
	/* Get the maximum size of the path excluding the socket filename */
	max_sock_path_len = SIZEOF(gtmsecshr_sock_name.sun_path) - MAX_SECSHR_SOCKFILE_NAME_LEN;
	/* Make sure this length is atmost equal to the size of the buffer that will hold the socket path */
	if (GTM_PATH_MAX < max_sock_path_len)
		max_sock_path_len = GTM_PATH_MAX - MAX_SECSHR_SOCKFILE_NAME_LEN;
	/* Get the value of the GTMSECSHR_SOCK_DIR logical from the environment. status will be SS_LOG2LONG if
	 * the value is greater than max_sock_path_len
	 */
	status = TRANS_LOG_NAME(&secshrsock_lognam, &secshrsock_transnam, gtmsecshr_sockpath, max_sock_path_len,
				do_sendmsg_on_log2long);
	if ((SS_NORMAL != status) || !ABSOLUTE_PATH(gtmsecshr_sockpath))
	{
		if (SS_LOG2LONG == status)
		{
			if (SERVER == caller)
				send_msg(VARLSTCNT(5) ERR_LOGTOOLONG, 3, secshrsock_lognam.len, secshrsock_lognam.addr,
					 max_sock_path_len);
			else
				gtm_putmsg(VARLSTCNT(5) ERR_LOGTOOLONG, 3, secshrsock_lognam.len, secshrsock_lognam.addr,
					   max_sock_path_len);
		}
		ret_status = INVLOGNAME;
		strcpy(gtmsecshr_sockpath, DEFAULT_GTMSECSHR_SOCK_DIR);
		gtmsecshr_sockpath_len = SIZEOF(DEFAULT_GTMSECSHR_SOCK_DIR) - 1;
	} else
		gtmsecshr_sockpath_len = secshrsock_transnam.len;
	if ((-1 == Stat(gtmsecshr_sockpath, &buf)) || !S_ISDIR(buf.st_mode) )
	{
		if (ret_status)
			error_mesg = "Unable to locate default tmp directory";
		else
			error_mesg = "$gtm_tmp not a directory";
		if (SERVER == caller)
			send_msg(VARLSTCNT(9) MAKE_MSG_SEVERE(ERR_GTMSECSHRSOCKET), 3,
				 RTS_ERROR_STRING((SERVER == caller) ? "Server" : "Caller"), process_id,
				 ERR_TEXT, 2, RTS_ERROR_STRING(error_mesg));
		else
			gtm_putmsg(VARLSTCNT(9) MAKE_MSG_SEVERE(ERR_GTMSECSHRSOCKET), 3,
				   RTS_ERROR_STRING((SERVER == caller) ? "Server" : "Caller"), process_id,
				   ERR_TEXT, 2, RTS_ERROR_STRING(error_mesg));
		return INVLOGNAME;
	}
	ret_status = 0;
	if ('/' != gtmsecshr_sockpath[gtmsecshr_sockpath_len - 1])
	 	gtmsecshr_sockpath[gtmsecshr_sockpath_len++] = '/';
	gtmsecshr_sockpath[gtmsecshr_sockpath_len] = '\0';
	strcpy(gtmsecshr_sockpath + gtmsecshr_sockpath_len , GTMSECSHR_SOCK_PREFIX);
	gtmsecshr_sockpath_len += (SIZEOF(GTMSECSHR_SOCK_PREFIX) - 1);
	/* Servers have already determined the executable name. Use that instead of below. */
	if (SERVER == caller)
	{	/* Use path name discovered by gtmsecshr_init() */
		strcpy(gtmsecshr_path, execpath);
		gtmsecshr_path[execpathln++] = '/';
		gtmsecshr_path[execpathln] = '\0';
		strcat(gtmsecshr_path, GTMSECSHR_EXECUTABLE);
		gtmsecshr_pathname.addr = gtmsecshr_path;
		gtmsecshr_pathname.len = execpathln + STRLEN(GTMSECSHR_EXECUTABLE);
	} else
	{	/* Discover path name */
		gtmsecshr_logname.addr = GTMSECSHR_PATH;
		gtmsecshr_logname.len = SIZEOF(GTMSECSHR_PATH) - 1;
		if (SS_NORMAL !=
		    (status = TRANS_LOG_NAME(&gtmsecshr_logname, &gtmsecshr_pathname, gtmsecshr_path, SIZEOF(gtmsecshr_path),
					     dont_sendmsg_on_log2long)))
		{
			if (SS_LOG2LONG == status)
				gtm_putmsg(VARLSTCNT(5) ERR_LOGTOOLONG, 3, gtmsecshr_logname.len, gtmsecshr_logname.addr,
					   SIZEOF(gtmsecshr_path) - 1);
			gtmsecshr_pathname.len = 0;
			gtm_putmsg(VARLSTCNT(13) ERR_GTMSECSHRSOCKET, 3,
				   RTS_ERROR_STRING("Caller"), process_id, ERR_TEXT, 2,
				   RTS_ERROR_LITERAL("Environment variable gtm_dist pointing to an invalid path"),
				   ERR_TEXT, 2, RTS_ERROR_STRING(gtmsecshr_logname.addr));
			ret_status = INVLOGNAME;
		}
		gtmsecshr_path[gtmsecshr_pathname.len] = '\0';
	}
	/* We have different project id here. This guarantees to avoid deadlock, if only one gtm installation is there */
	if (-1 == (gtmsecshr_key = FTOK(gtmsecshr_path, GTMSECSHR_ID)))
	{
		ret_status = FTOKERR;
		if (SERVER == caller)
			gtm_putmsg(VARLSTCNT(14) ERR_GTMSECSHRSOCKET, 3,
				   RTS_ERROR_STRING((SERVER == caller) ? "Server" : "Caller"), process_id,
				   ERR_TEXT, 2, RTS_ERROR_LITERAL("Error with gtmsecshr ftok :"),
				   ERR_TEXT, 2, RTS_ERROR_STRING(gtmsecshr_path), errno);
		else
			send_msg(VARLSTCNT(14) ERR_GTMSECSHRSOCKET, 3,
				 RTS_ERROR_STRING((SERVER == caller) ? "Server" : "Caller"), process_id,
				 ERR_TEXT, 2, RTS_ERROR_LITERAL("Error with gtmsecshr ftok :"),
				 ERR_TEXT, 2, RTS_ERROR_STRING(gtmsecshr_path), errno);
	}
	return ret_status;
}

/* Note - only the server passes in the executable name/len - ignore for client */
int4 gtmsecshr_sock_init(int caller)
{
	int			ret_status = 0;
	int			save_errno, gtmsecshr_cli_sockpath_end;
	int			id_str_len;
	int4			init_pathname_status;
	unsigned char		id_str[MAX_ID_LEN+1], suffix;
	unsigned char		pid_str[2 * SIZEOF(pid_t) + 1];
	int			i2hex_nofill(int , uchar_ptr_t, int);
	int			stat_res;
	struct stat     	stat_buf;
	struct stat     	dist_stat_buff;
	int			lib_gid;

	assert(FALSE == gtmsecshr_sock_init_done);
	if (!process_id)
		getjobnum();
	if (CLIENT == caller)
	{
		if (0 != (init_pathname_status = gtmsecshr_pathname_init(CLIENT, NULL, 0)))
			return init_pathname_status;
		gtmsecshr_cli_sock_name.sun_family = AF_UNIX;
		memcpy(gtmsecshr_cli_sock_name.sun_path, gtmsecshr_sockpath, gtmsecshr_sockpath_len);
		strcpy(gtmsecshr_cli_sock_name.sun_path + gtmsecshr_sockpath_len, (char *)mypid2ascx(pid_str, process_id));
		gtmsecshr_cli_sockpath_len = (int)(SUN_LEN(&gtmsecshr_cli_sock_name));
	}
	id_str[i2hex_nofill((unsigned int)gtmsecshr_key, (uchar_ptr_t )id_str, MAX_ID_LEN)] = 0;
	id_str_len = STRLEN((char *)id_str);
	memcpy(gtmsecshr_sockpath + gtmsecshr_sockpath_len, (char *)id_str, id_str_len);
	gtmsecshr_sockpath_len += id_str_len;
	gtmsecshr_sock_name.sun_family = AF_UNIX;
	memcpy(gtmsecshr_sock_name.sun_path, gtmsecshr_sockpath, gtmsecshr_sockpath_len);
	gtmsecshr_sockpath_len = (int)(SUN_LEN(&gtmsecshr_sock_name));
	if (FD_INVALID == (gtmsecshr_sockfd = socket(AF_UNIX, SOCK_DGRAM, 0)))
	{
		rts_error(VARLSTCNT(10) ERR_GTMSECSHRSOCKET, 3, RTS_ERROR_STRING((SERVER == caller) ? "Server" : "Caller"),
			  process_id, ERR_TEXT, 2, RTS_ERROR_LITERAL("Error with gtmsecshr socket create"), errno);
		ret_status = SOCKETERR;
	}
	if (SERVER == caller)
	{
		if (!ret_status)
		{
			if (-1 == UNLINK(gtmsecshr_sock_name.sun_path))
			{
				if (ENOENT != errno)
				{
					save_errno = errno;
					send_msg(VARLSTCNT(9) ERR_GTMSECSHRSOCKET, 3,
						 RTS_ERROR_STRING((SERVER == caller) ? "Server" : "Caller"), process_id,
						 ERR_TEXT, 2, RTS_ERROR_LITERAL("Error unlinking leftover gtmsecshr socket"),
						save_errno);
					ret_status = UNLINKERR;
				}
			}
		}
		if (!ret_status)
		{
			if (0 > BIND(gtmsecshr_sockfd, (struct sockaddr *)&gtmsecshr_sock_name, gtmsecshr_sockpath_len))
			{
				rts_error(VARLSTCNT(10) ERR_GTMSECSHRSOCKET, 3,
					  RTS_ERROR_STRING((SERVER == caller) ? "Server" : "Caller"), process_id,
					  ERR_TEXT, 2, RTS_ERROR_LITERAL("Error with gtmsecshr socket bind"),
			  		errno);
				ret_status = BINDERR;
			}
		}
	} else /* CLIENT */
	{
		for (suffix = '\0'; !ret_status && 'z' >= suffix; )
		{
			if (-1 == UNLINK(gtmsecshr_cli_sock_name.sun_path))
			{
				if (EPERM == errno || EACCES == errno)
				{
					if (!suffix)
					{
						suffix = 'a';
						gtmsecshr_cli_sockpath_end = STRLEN(gtmsecshr_cli_sock_name.sun_path);
						gtmsecshr_cli_sock_name.sun_path[gtmsecshr_cli_sockpath_end + 1] = '\0';
#						ifdef EXACT_SIZE_SOCKNAME
						gtmsecshr_cli_sockpath_len++; /* Account for socket name growth (suffix) */
#						endif
					} else
						suffix++;
					gtmsecshr_cli_sock_name.sun_path[gtmsecshr_cli_sockpath_end] = suffix;
					continue;
				} else if (ENOENT != errno)
				{
					save_errno = errno;
					send_msg(VARLSTCNT(10) ERR_GTMSECSHRSOCKET, 3,
						 RTS_ERROR_STRING((SERVER == caller) ? "Server" : "Caller"), process_id,
						 ERR_TEXT, 2, RTS_ERROR_LITERAL("Error unlinking leftover gtmsecshr_cli socket"),
						save_errno);
					ret_status = UNLINKERR;
				} else
					break;
			} else
				break;
		}
                if ( 'z' < suffix)
		{
			send_msg(VARLSTCNT(9) ERR_GTMSECSHRSOCKET, 3, RTS_ERROR_LITERAL("Client"), process_id,
				ERR_TEXT, 2, RTS_ERROR_LITERAL("Too many left over gtmsecshr_cli sockets"));
			ret_status = UNLINKERR;
		}
		if (!ret_status)
		{
			if (0 > BIND(gtmsecshr_sockfd, (struct sockaddr *)&gtmsecshr_cli_sock_name, gtmsecshr_cli_sockpath_len))
			{
				rts_error(VARLSTCNT(10) ERR_GTMSECSHRSOCKET, 3,
					  RTS_ERROR_STRING((SERVER == caller) ? "Server" : "Caller"), process_id,
					  ERR_TEXT, 2, RTS_ERROR_LITERAL("Error with gtmsecshr_cli socket bind"), errno);
				ret_status = BINDERR;
			} else if ('\0' != suffix)
				ret_status = ONETIMESOCKET;
			/* If ret_status is zero do the following checks if $gtm_dist/libgtmshr.so is not world accessible
			 * then set mode to 0660 and change the gid to the gid of $gtm_dist/libgtmshr.so if different from
			 * current user.
			 */
			if (!ret_status)
			{
				lib_gid = gtm_get_group_id(&dist_stat_buff);
				if ((-1 != lib_gid) && (dist_stat_buff.st_mode & 04))
					lib_gid = -1; /* don't change it */
				if ((-1 != lib_gid)
				    && (-1 == CHMOD(gtmsecshr_cli_sock_name.sun_path, 0660)
					|| ((lib_gid != GETGID())
					    && (-1 == CHOWN(gtmsecshr_cli_sock_name.sun_path, -1, lib_gid)))))
				{
					rts_error(VARLSTCNT(10) ERR_GTMSECSHRSOCKET, 3,
						  RTS_ERROR_STRING("Caller"), process_id, ERR_TEXT, 2,
						  RTS_ERROR_LITERAL("Error changing socket permissions/group"), errno);
				}
			}
		}
	}
	gtmsecshr_sock_init_done = TRUE;
	return ret_status;
}

unsigned char *mypid2ascx(unsigned char *pid_str, pid_t pid)
{	/* pid_str should accommodate at least 2 * SIZEOF(pid_t) + 1 characters */
	register unsigned char *cp;

	cp = &pid_str[2 * SIZEOF(pid_t)];
	*cp = '\0'; 		/* Null terminate the string */
	while (cp > pid_str)
	{
		*--cp = hex_table[pid & 0xF];
		pid >>= 4;
	}
	return pid_str;
}
