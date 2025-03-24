/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2019-2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#if defined(__MVS__) && !defined(_ISOC99_SOURCE)
#define _ISOC99_SOURCE
#endif

#include "mdef.h"

#include <sys/time.h>
#include <errno.h>

#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_socket.h"
#include "gtm_netdb.h"
#include "gtm_ipv6.h"
#include "gtm_inet.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_sp.h"
#include "repl_comm.h"
#include "repl_log.h"
#include "dogetaddrinfo.h"
#include "util.h"	/* util_out_print in GTM_PUTMSG_CSA_ADDRINFO */

#define RESOLUTION_FAILURE_PREFIX	"Failure in resolving "
GBLDEF	int			gtmsource_sock_fd = FD_INVALID;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF  FILE			*gtmsource_log_fp;
GBLREF  gtmsource_options_t	gtmsource_options;

error_def(ERR_REPLCOMM);
error_def(ERR_GETADDRINFO);
error_def(ERR_TEXT);

int gtmsource_comm_init(boolean_t print_addresolve_error)
{
	/* Initialize communication stuff */
<<<<<<< HEAD
	struct addrinfo 	*ai_ptr = NULL, *ai_head = NULL, hints;
=======
	struct	linger	disable_linger = {0, 0};
	char	error_string[1024];
	int	err_status, send_buffsize, recv_buffsize, tcp_s_buffsize;
	struct addrinfo *ai_ptr = NULL, *ai_head = NULL, hints;
>>>>>>> fdfdea1e (GT.M V7.1-002)
	gtmsource_local_ptr_t   gtmsource_local;
	intrpt_state_t  	prev_intrpt_state;
	struct linger		disable_linger = {0, 0};
	char			error_string[1024];
	int			err_status;
	char			*host;
	char			port_buffer[NI_MAXSERV];
	char			hostinfo[SIZEOF(RESOLUTION_FAILURE_PREFIX) + MAX_HOST_NAME_LEN + NI_MAXSERV];
	int			port_len;
	int			errcode;

	if (FD_INVALID != gtmsource_sock_fd) /* Initialization done already */
		return(0);
	gtmsource_local = jnlpool->gtmsource_local;
	port_len = 0;
	I2A(port_buffer, port_len,  gtmsource_local->secondary_port);
	port_buffer[port_len] = '\0';
	host = gtmsource_local->secondary_host;
	CLIENT_HINTS(hints);
	errcode = dogetaddrinfo(host, port_buffer, &hints, &ai_head);
	if ((0 != errcode) && print_addresolve_error)
	{
		SNPRINTF(hostinfo, SIZEOF(hostinfo), "%s%s:%s", RESOLUTION_FAILURE_PREFIX, host, port_buffer);
		GTM_PUTMSG_CSA_ADDRINFO(NULL, ERR_GETADDRINFO, errcode, hostinfo);
	}
	if (ai_head)
	{
		for(ai_ptr = ai_head; NULL != ai_ptr; ai_ptr = ai_ptr->ai_next)
		{
			if (FD_INVALID == (gtmsource_sock_fd = socket(ai_ptr->ai_family, ai_ptr->ai_socktype,
									ai_ptr->ai_protocol)))
					err_status = errno;
			else
			{
				err_status = 0;
				break;
			}
		}
		if (0 != err_status)
		{
			FREEADDRINFO(ai_head); /* prevent mem-leak */
			SNPRINTF(error_string, SIZEOF(error_string), "Error with source server socket create : %s",
				 STRERROR(err_status));
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, RTS_ERROR_STRING(error_string));
		}
		assert(NULL != ai_ptr);
		assert(SIZEOF(gtmsource_local->secondary_inet_addr) >= ai_ptr->ai_addrlen);
		/* only save the addrinfo and address after the socket is successfuly created */
		gtmsource_local->secondary_af = ai_ptr->ai_family;
		gtmsource_local->secondary_addrlen = ai_ptr->ai_addrlen;
		memcpy((struct sockaddr*)(&gtmsource_local->secondary_inet_addr), ai_ptr->ai_addr, ai_ptr->ai_addrlen);
		FREEADDRINFO(ai_head); /* prevent mem-leak */
		/* A connection breakage should get rid of the socket */
		if (-1 == setsockopt(gtmsource_sock_fd, SOL_SOCKET, SO_LINGER,
				(const void *)&disable_linger, SIZEOF(disable_linger)))
		{
			err_status = ERRNO;
			SNPRINTF(error_string, SIZEOF(error_string), "Error with source server socket disable linger : %s",
					STRERROR(err_status));
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, RTS_ERROR_STRING(error_string));
		}
		if (0 != (err_status = get_send_sock_buff_size(gtmsource_sock_fd, &send_buffsize)))
		{
			SNPRINTF(error_string, SIZEOF(error_string), "Error getting socket send buffsize : %s",
					STRERROR(err_status));
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(error_string));
		}
		if (send_buffsize < gtmsource_options.send_buffsize)
		{
			for (tcp_s_buffsize = gtmsource_options.send_buffsize;
					tcp_s_buffsize >= MAX(send_buffsize, GTMSOURCE_MIN_TCP_SEND_BUFSIZE)
					&&  0 != (err_status = set_send_sock_buff_size(gtmsource_sock_fd, tcp_s_buffsize));
					tcp_s_buffsize -= GTMSOURCE_TCP_SEND_BUFSIZE_INCR)
				;
			if (tcp_s_buffsize < GTMSOURCE_MIN_TCP_SEND_BUFSIZE)
			{
				SNPRINTF(error_string, SIZEOF(error_string), "Could not set TCP send buffer size in range [%d, %d],"
						"last known error : %s", GTMSOURCE_MIN_TCP_SEND_BUFSIZE,
						gtmsource_options.send_buffsize, STRERROR(err_status));
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) MAKE_MSG_INFO(ERR_REPLCOMM), 0, ERR_TEXT, 2,
						LEN_AND_STR(error_string));
			}
		}
		if (0 != (err_status = get_recv_sock_buff_size(gtmsource_sock_fd, &recv_buffsize)))
		{
			SNPRINTF(error_string, SIZEOF(error_string), "Error getting socket recv buffsize : %s",
					STRERROR(err_status));
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(10) ERR_REPLCOMM, 0, ERR_TEXT, 2,
					LEN_AND_LIT("Error getting socket recv buffsize"),
					ERR_TEXT, 2, LEN_AND_STR(error_string));
		}
		if (recv_buffsize < gtmsource_options.recv_buffsize)
		{
			if (0 != (err_status = set_recv_sock_buff_size(gtmsource_sock_fd, gtmsource_options.recv_buffsize)))
			{
				if (recv_buffsize < GTMSOURCE_MIN_TCP_RECV_BUFSIZE)
				{
					SNPRINTF(error_string, SIZEOF(error_string), "Could not set TCP recv buffer size to"
							" %d : %s", GTMSOURCE_MIN_TCP_RECV_BUFSIZE, STRERROR(err_status));
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) MAKE_MSG_INFO(ERR_REPLCOMM), 0, ERR_TEXT, 2,
							LEN_AND_STR(error_string));
				}
			}
		}
	}
	return(errcode);
}
