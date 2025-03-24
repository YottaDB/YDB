/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_socket.h"
#include "gtm_netdb.h" /* for NI_MAXSERV and AI_V4MAPPED */
#include "gtm_ipv6.h"
#include "gtm_inet.h"
#include "gtm_fcntl.h"

#include <sys/time.h>
#include <errno.h>
#include "gtm_unistd.h"
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
#include "repl_sp.h"
#include "gtmio.h"
#include "util.h"
#include "repl_log.h"
#include "repl_comm.h"
#include "dogetaddrinfo.h"

GBLDEF	int                gtmrecv_listen_sock_fd = FD_INVALID;
GBLREF	gtmrecv_options_t  gtmrecv_options;

/* Initialize communication stuff */
int gtmrecv_comm_init(in_port_t port)
{
	struct addrinfo		*ai_ptr = NULL, hints;
	const	int		enable_reuseaddr = 1;
	struct  linger  	disable_linger = {0, 0};
	int			rc, send_buffsize, recv_buffsize, tcp_r_buffsize;
	int			errcode;
	char			port_buffer[NI_MAXSERV];
	int			port_buffer_len;
	int			temp_sock_fd;
	int			af;
	char			err_buffer[512];
	struct sockaddr_in 	local;
	struct sockaddr         *local_sa_ptr;
	char			local_ip[SA_MAXLEN];
	char                    local_port_buffer[NI_MAXSERV];
	unsigned int		save_errno;
	GTM_SOCKLEN_TYPE        len;

	if (FD_INVALID != gtmrecv_listen_sock_fd) /* Initialization done already */
		return (0);
	/* Create the socket used for communicating with primary */
	af = ((GTM_IPV6_SUPPORTED && !ipv4_only) ? AF_INET6 : AF_INET);
	if (FD_INVALID == (temp_sock_fd = socket(af, SOCK_STREAM, IPPROTO_TCP)))
	{
		af = AF_INET;
		if (FD_INVALID == (temp_sock_fd = socket(af, SOCK_STREAM, IPPROTO_TCP)))
		{
			save_errno = ERRNO;
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
					RTS_ERROR_LITERAL("Error with receiver server socket create"), save_errno);
			return (-1);
		}
	}
	/* Make it known to the world that you are ready for a Source Server */
	SERVER_HINTS(hints, af);
	SNPRINTF(port_buffer, NI_MAXSERV, "%hu", port);
	if (0 != (errcode = dogetaddrinfo(NULL, port_buffer, &hints, &ai_ptr)))
	{
		CLOSEFILE(temp_sock_fd, rc);
		RTS_ERROR_ADDRINFO_CTX(NULL, ERR_GETADDRINFO, errcode, "FAILED in obtaining IP address on receiver server.");
		return -1;
	}
<<<<<<< HEAD
	/* Note: Since the "dogetaddrinfo" call succeeded, "ai_ptr" would have been allocated.
	 * So need to free it up (using "FREEADDRINFO") before returning normally or with an error.
	 */
=======
>>>>>>> fdfdea1e (GT.M V7.1-002)
	gtmrecv_listen_sock_fd = temp_sock_fd;
	if (0 > setsockopt(gtmrecv_listen_sock_fd, SOL_SOCKET, SO_LINGER, (const void *)&disable_linger, SIZEOF(disable_linger)))
	{
		save_errno = ERRNO;
		FREEADDRINFO(ai_ptr);
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Error with receiver server listen socket disable linger"), save_errno);
	}
	if (0 > setsockopt(gtmrecv_listen_sock_fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&enable_reuseaddr,
			SIZEOF(enable_reuseaddr)))
	{
		save_errno = ERRNO;
		FREEADDRINFO(ai_ptr);
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Error with receiver server listen socket enable reuseaddr"), save_errno);
	}
	if (0 != (errcode = get_send_sock_buff_size(gtmrecv_listen_sock_fd, &send_buffsize)))
	{
		SNPRINTF(err_buffer, SIZEOF(err_buffer), "Error getting socket send buffsize : %s", STRERROR(errcode));
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(err_buffer));
	}
	if (send_buffsize < gtmrecv_options.send_buffsize)
	{
		if (0 != (errcode = set_send_sock_buff_size(gtmrecv_listen_sock_fd, gtmrecv_options.send_buffsize)))
		{
			if (send_buffsize < GTMRECV_MIN_TCP_SEND_BUFSIZE)
			{
				SNPRINTF(err_buffer, SIZEOF(err_buffer), "Could not set TCP send buffer size to %d : %s",
						GTMRECV_MIN_TCP_SEND_BUFSIZE, STRERROR(errcode));
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) MAKE_MSG_INFO(ERR_REPLCOMM), 0,
						ERR_TEXT, 2, LEN_AND_STR(err_buffer));
			}
		}
	}
	if (0 != (errcode = get_recv_sock_buff_size(gtmrecv_listen_sock_fd, &recv_buffsize)))
	{
		SNPRINTF(err_buffer, SIZEOF(err_buffer), "Error getting socket recv buffsize : %s", STRERROR(errcode));
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(err_buffer));
	}
	if (recv_buffsize < gtmrecv_options.recv_buffsize)
	{
		for (tcp_r_buffsize = gtmrecv_options.recv_buffsize;
				tcp_r_buffsize >= MAX(recv_buffsize, GTMRECV_MIN_TCP_RECV_BUFSIZE)
				&&  0 != (errcode = set_recv_sock_buff_size(gtmrecv_listen_sock_fd, tcp_r_buffsize));
				tcp_r_buffsize -= GTMRECV_TCP_RECV_BUFSIZE_INCR)
			;
		if (tcp_r_buffsize < GTMRECV_MIN_TCP_RECV_BUFSIZE)
		{
			SNPRINTF(err_buffer, SIZEOF(err_buffer), "Could not set TCP receive buffer size in range [%d, %d], last "
					"known error : %s", GTMRECV_MIN_TCP_RECV_BUFSIZE, gtmrecv_options.recv_buffsize,
					STRERROR(errcode));
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) MAKE_MSG_INFO(ERR_REPLCOMM), 0,
					ERR_TEXT, 2, LEN_AND_STR(err_buffer));
		}
	}
	if ((0 > BIND(gtmrecv_listen_sock_fd, ai_ptr->ai_addr, ai_ptr->ai_addrlen)) || (WBTEST_ENABLED(WBTEST_REPL_INIT_ERR)) )
	{
		save_errno = ERRNO;
		GTM_WHITE_BOX_TEST(WBTEST_REPL_INIT_ERR, errno, 98);
		SNPRINTF(err_buffer, 512, "Could not bind local address. Local Port : %hu", port);
		SEND_SYSMSG_REPLCOMM(LEN_AND_STR(err_buffer));
		FREEADDRINFO(ai_ptr);
		CLOSEFILE_RESET(gtmrecv_listen_sock_fd, rc);	/* resets "gtmrecv_listen_sock_fd" to FD_INVALID */
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
				 RTS_ERROR_STRING(err_buffer), save_errno);
		return (-1);
	}
	if ((0 > listen(gtmrecv_listen_sock_fd, 5)) || (WBTEST_ENABLED(WBTEST_REPL_INIT_ERR2)))
	{
		save_errno = ERRNO;
		len = SIZEOF(local);
		if (0 == getsockname(gtmrecv_listen_sock_fd, (struct sockaddr *)&local, (GTM_SOCKLEN_TYPE *)&len))
			SNPRINTF(err_buffer, 512, "Could not listen. Local port : %hu", ntohs(local.sin_port));
		else
			SNPRINTF(err_buffer, 512, "Could not listen. Local port : *UNKNOWN* : %s\n", strerror(errno));
		GTM_WHITE_BOX_TEST(WBTEST_REPL_INIT_ERR2, save_errno, 98);
		SEND_SYSMSG_REPLCOMM(LEN_AND_STR(err_buffer));
		FREEADDRINFO(ai_ptr);
		CLOSEFILE_RESET(gtmrecv_listen_sock_fd, rc);	/* resets "gtmrecv_listen_sock_fd" to FD_INVALID */
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
				 RTS_ERROR_STRING(err_buffer), save_errno);
		return (-1);
	}
	FREEADDRINFO(ai_ptr);
	return (0);
}
