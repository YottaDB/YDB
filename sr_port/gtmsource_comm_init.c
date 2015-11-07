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

GBLDEF	int			gtmsource_sock_fd = FD_INVALID;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF  FILE			*gtmsource_log_fp;

error_def(ERR_REPLCOMM);
error_def(ERR_GETADDRINFO);
error_def(ERR_TEXT);

int gtmsource_comm_init(void)
{
	/* Initialize communication stuff */
	struct	linger	disable_linger = {0, 0};
	char	error_string[1024];
	int	err_status;
	struct addrinfo *ai_ptr = NULL, *ai_head = NULL, hints;
	gtmsource_local_ptr_t   gtmsource_local;
	char	*host;
	char	port_buffer[NI_MAXSERV];
	int	port_len;
	int	errcode;
	int	tries;

	if (FD_INVALID != gtmsource_sock_fd) /* Initialization done already */
		return(0);
	gtmsource_local = jnlpool.gtmsource_local;
	port_len = 0;
	I2A(port_buffer, port_len,  gtmsource_local->secondary_port);
	port_buffer[port_len] = '\0';
	host = gtmsource_local->secondary_host;
	CLIENT_HINTS(hints);
	for (tries = 0;
	     tries < MAX_GETHOST_TRIES &&
	     EAI_AGAIN == (errcode = getaddrinfo(host, port_buffer, &hints, &ai_head));
	      tries++);
	if (0 != errcode)
		RTS_ERROR_ADDRINFO(NULL, ERR_GETADDRINFO, errcode);
	/* find one address valid for creating socket */
	assert(ai_head);
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
		freeaddrinfo(ai_head); /* prevent mem-leak */
		SNPRINTF(error_string, SIZEOF(error_string), "Error with source server socket create : %s",
			 STRERROR(err_status));
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, RTS_ERROR_STRING(error_string));
	}
	assert(NULL != ai_ptr);
	assert(SIZEOF(gtmsource_local->secondary_inet_addr) >= ai_ptr->ai_addrlen);
	/* only save the addrinfo and address after the socket is successfuly created */
	gtmsource_local->secondary_af = ai_ptr->ai_family;
	gtmsource_local->secondary_addrlen = ai_ptr->ai_addrlen;
	memcpy((struct sockaddr*)(&gtmsource_local->secondary_inet_addr), ai_ptr->ai_addr, ai_ptr->ai_addrlen);
	freeaddrinfo(ai_head); /* prevent mem-leak */
	/* A connection breakage should get rid of the socket */
	if (-1 == setsockopt(gtmsource_sock_fd, SOL_SOCKET, SO_LINGER, (const void *)&disable_linger, SIZEOF(disable_linger)))
	{
		err_status = ERRNO;
		SNPRINTF(error_string, SIZEOF(error_string), "Error with source server socket disable linger : %s",
				STRERROR(err_status));
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, RTS_ERROR_STRING(error_string));
	}
	return(0);
}
