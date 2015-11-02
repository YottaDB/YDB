/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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
#include "gtm_unistd.h"
#include "gtm_socket.h"

#include "gtm_logicals.h"
#include "io.h"
#include "error.h"
#include "gtmsecshr.h"
#include "iosp.h"
#include "send_msg.h"
#include "getjobnum.h"
#include "gtmmsg.h"
#include "trans_log_name.h"

GBLREF struct sockaddr_un 	gtmsecshr_sock_name;
GBLREF struct sockaddr_un 	gtmsecshr_cli_sock_name;
GBLREF key_t			gtmsecshr_key;
GBLREF int 			gtmsecshr_sockpath_len;
GBLREF int 			gtmsecshr_cli_sockpath_len;
GBLREF mstr 			gtmsecshr_pathname;
GBLREF boolean_t		gtmsecshr_sock_init_done;
GBLREF uint4			process_id;
GBLREF int			gtmsecshr_sockfd;

static char			gtmsecshr_sockpath[MAX_TRANS_NAME_LEN];
static char			gtmsecshr_path[MAX_TRANS_NAME_LEN];

static char hex_table[] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

unsigned char		*mypid2ascx(unsigned char *, pid_t);

#ifndef SUN_LEN
#  define SUN_LEN(x)	sizeof(*x)
#else
#  define EXACT_SIZE_SOCKNAME
#endif

int4 gtmsecshr_pathname_init(int caller)
{
	int			ret_status = 0;
	char			*error_mesg;
	mstr			secshrsock_lognam, secshrsock_transnam;
	mstr			gtmsecshr_logname;
	struct stat		buf;

	error_def(ERR_GTMSECSHRSOCKET);
	error_def(ERR_TEXT);

	if (!process_id)
		getjobnum();

	secshrsock_lognam.addr = GTMSECSHR_SOCK_DIR;
	secshrsock_lognam.len = sizeof(GTMSECSHR_SOCK_DIR) - 1;

	if ((SS_NORMAL != trans_log_name(&secshrsock_lognam, &secshrsock_transnam, gtmsecshr_sockpath))
			|| !ABSOLUTE_PATH(gtmsecshr_sockpath))
	{
		ret_status = INVLOGNAME;
		strcpy(gtmsecshr_sockpath, DEFAULT_GTMSECSHR_SOCK_DIR);
		gtmsecshr_sockpath_len = sizeof(DEFAULT_GTMSECSHR_SOCK_DIR) - 1;
	} else
		gtmsecshr_sockpath_len = secshrsock_transnam.len;

	if ((-1 == Stat(gtmsecshr_sockpath, &buf)) || !S_ISDIR(buf.st_mode) )
	{
		if (ret_status)
			error_mesg = "Unable to locate default tmp directory";
		else
			error_mesg = "$gtm_tmp not a directory";

		send_msg(VARLSTCNT(9) MAKE_MSG_SEVERE(ERR_GTMSECSHRSOCKET), 3,
			RTS_ERROR_STRING((SERVER == caller) ? "Server" : "Caller"), process_id,
			ERR_TEXT, 2, RTS_ERROR_STRING(error_mesg));
		gtm_putmsg(VARLSTCNT(9) ERR_GTMSECSHRSOCKET, 3,
			RTS_ERROR_STRING((SERVER == caller) ? "Server" : "Caller"), process_id, ERR_TEXT, 2,
			RTS_ERROR_STRING(error_mesg));
		return INVLOGNAME;
	}

	ret_status = 0;
	if ('/' != gtmsecshr_sockpath[gtmsecshr_sockpath_len - 1])
	 	gtmsecshr_sockpath[gtmsecshr_sockpath_len++] = '/';

	gtmsecshr_sockpath[gtmsecshr_sockpath_len] = '\0';

	strcpy(gtmsecshr_sockpath + gtmsecshr_sockpath_len , GTMSECSHR_SOCK_PREFIX);
	gtmsecshr_sockpath_len += (sizeof(GTMSECSHR_SOCK_PREFIX) - 1);

	gtmsecshr_logname.addr = GTMSECSHR_PATH;
	gtmsecshr_logname.len = sizeof(GTMSECSHR_PATH) - 1;

	if (SS_NORMAL != trans_log_name(&gtmsecshr_logname, &gtmsecshr_pathname, gtmsecshr_path))
	{
		gtmsecshr_pathname.len = 0;
		gtm_putmsg(VARLSTCNT(13) ERR_GTMSECSHRSOCKET, 3,
			RTS_ERROR_STRING((SERVER == caller) ? "Server" : "Caller"), process_id, ERR_TEXT, 2,
			RTS_ERROR_LITERAL("Environment variable gtm_dist pointing to an invalid path"),
			ERR_TEXT, 2, RTS_ERROR_STRING(gtmsecshr_logname.addr));
		ret_status = INVLOGNAME;
	}

	gtmsecshr_path[gtmsecshr_pathname.len] = 0;

	/* We have different project id here. This guarantees to avoid deadlock, if only one gtm installation is there */
	if (-1 == (gtmsecshr_key = FTOK(gtmsecshr_path, GTMSECSHR_ID)))
	{
		ret_status = FTOKERR;
		gtm_putmsg(VARLSTCNT(14) ERR_GTMSECSHRSOCKET, 3,
				RTS_ERROR_STRING((SERVER == caller) ? "Server" : "Caller"), process_id,
				ERR_TEXT, 2, RTS_ERROR_LITERAL("Error with gtmsecshr ftok :"),
				ERR_TEXT, 2, RTS_ERROR_STRING(gtmsecshr_path), errno);
	}

	return(ret_status);
}

int4 gtmsecshr_sock_init(int caller)
{
	int			ret_status = 0;
	int			save_errno, gtmsecshr_cli_sockpath_end;
	int			id_str_len;
	int4			init_pathname_status;
	unsigned char		id_str[MAX_ID_LEN+1], suffix;
	unsigned char		pid_str[2 * sizeof(pid_t) + 1];
	int			i2hex_nofill(int , uchar_ptr_t, int);

	error_def(ERR_GTMSECSHRSOCKET);
	error_def(ERR_TEXT);


	assert (FALSE == gtmsecshr_sock_init_done);

	if (!process_id)
		getjobnum();

	if (CLIENT == caller)
	{
		if ((init_pathname_status = gtmsecshr_pathname_init(CLIENT)) != 0)
			return(init_pathname_status);
		gtmsecshr_pathname_init(CLIENT);
		gtmsecshr_cli_sock_name.sun_family = AF_UNIX;
		memcpy(gtmsecshr_cli_sock_name.sun_path, gtmsecshr_sockpath, gtmsecshr_sockpath_len);
		strcpy(gtmsecshr_cli_sock_name.sun_path + gtmsecshr_sockpath_len, (char *)mypid2ascx(pid_str, process_id));
		gtmsecshr_cli_sockpath_len = SUN_LEN(&gtmsecshr_cli_sock_name);
	}

	id_str[i2hex_nofill((unsigned int)gtmsecshr_key, (uchar_ptr_t )id_str, MAX_ID_LEN)] = 0;
	id_str_len = STRLEN((char *)id_str);
	memcpy(gtmsecshr_sockpath + gtmsecshr_sockpath_len, (char *)id_str, id_str_len);
	gtmsecshr_sockpath_len += id_str_len;
	gtmsecshr_sock_name.sun_family = AF_UNIX;
	memcpy(gtmsecshr_sock_name.sun_path, gtmsecshr_sockpath, gtmsecshr_sockpath_len);
	gtmsecshr_sockpath_len = SUN_LEN(&gtmsecshr_sock_name);

	if (-1 == (gtmsecshr_sockfd = socket(AF_UNIX, SOCK_DGRAM, 0)))
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
	}
	else /* CLIENT */
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
#ifdef EXACT_SIZE_SOCKNAME
						gtmsecshr_cli_sockpath_len++; /* Account for socket name growth (suffix) */
#endif
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
		}
	}

	gtmsecshr_sock_init_done = TRUE;

	return ret_status;
}

unsigned char *mypid2ascx(unsigned char *pid_str, pid_t pid)
{
	/* pid_str should accommodate atleast 2*sizeof(pid_t) + 1 characters */

	register unsigned char *cp;

	cp = &pid_str[2*sizeof(pid_t)];
	*cp = '\0'; /* Null terminate the string */
	while(cp > pid_str)
	{
		*--cp = hex_table[pid & 0xF];
		pid >>= 4;
	}
	return(pid_str);
}
