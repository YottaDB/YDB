/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc *
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* gtcm_bgn_net.c ---
 *	This routine should initialize the network layer and set up a network visible connect point for the server.
 */

#include "mdef.h"

#include "gtm_stdlib.h"
#include "gtm_unistd.h"		/* for close() used by CLOSEFILE_RESET */
#include "gtm_time.h"		/* for ctime() and time() */

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

#include <errno.h>
#include "gtm_ctype.h"

#include "gtm_stdio.h"

#include "gtcm.h"
#include "gtmio.h"

GBLREF char	*omi_service;
GBLREF int	rc_server_id;
GBLREF int	one_conn_per_inaddr;
GBLREF int	authenticate;
GBLREF int	psock;
GBLREF int	ping_keepalive;
GBLREF int	omi_pid;

int gtcm_bgn_net(omi_conn_ll *cll)
{
	extern int4	omi_nxact, omi_nerrs, omi_brecv, omi_bsent;
	omi_fd		fd;
	int		i;
	int 		save_errno;
	int		rc;
#ifdef NET_TCP
	struct servent		*se;
	unsigned short		port;
#endif /* defined(NET_TCP) */
#ifdef BSD_TCP
	struct sockaddr_in	sin;
	int			on = 1;
#else /* defined(BSD_TCP) */
#ifdef SYSV_TCP
	struct t_bind		*bind;
#endif /* defined(SYSV_TCP) */
#endif /* !defined(BSD_TCP) */

	/*  The linked list of connections */
	cll->head = cll->tail = (omi_conn *)0;
	/*  The statistics */
	cll->stats.conn = cll->stats.clos = cll->stats.disc = 0;
	cll->st_cn.bytes_recv = 0;
	cll->st_cn.bytes_send = 0;
	cll->st_cn.start      = 0;
	for (i = 0; i < OMI_OP_MAX; i++)
		cll->st_cn.xact[i] = 0;
	for (i = 0; i < OMI_ER_MAX; i++)
		cll->st_cn.errs[i] = 0;
	omi_nxact = omi_nerrs = omi_brecv = omi_bsent = 0;
	/*  Fall back on a compile time constant */
	if (!omi_service)
		omi_service = SRVC_NAME;
#ifdef NET_TCP
/*  If not specified, we ask the system for any port */
	if (!omi_service)
		port = htons(0);
/*  Ask for a specific port */
	else
	{
		if (ISDIGIT_ASCII(*omi_service))
			port = atoi(omi_service);
		else
		{
			se = getservbyname(omi_service, "tcp");
			endservent();
			if (!se)
			{
				OMI_DBG((omi_debug, "%s:  Service \"%s\" not found in /etc/services.\n", SRVR_NAME, omi_service));
				return -1;
			}
			port = htons(se->s_port);
		}
	}
#endif /* defined(NET_TCP) */
#ifdef BSD_TCP
	/*  Create a socket */
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		save_errno = errno;
		return save_errno;
	}
	/*  Reuse a specified address */
	if (port && setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *)&on, SIZEOF(on)) < 0)
	{
		save_errno = errno;
		CLOSEFILE_RESET(fd, rc);	/* resets "fd" to FD_INVALID */
		return save_errno;
	}
	/* the system should periodically check to see if the connections are live */
	if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&on, SIZEOF(on)) < 0)
	{
		save_errno = errno;
		perror("setsockopt:");
		CLOSEFILE_RESET(fd, rc);	/* resets "fd" to FD_INVALID */
		return save_errno;
	}
	/*  Bind an address to the socket */
	sin.sin_family      = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port        = htons(port);
	if (bind(fd, (struct sockaddr *)&sin, SIZEOF(sin)) < 0)
	{
		save_errno = errno;
		CLOSEFILE_RESET(fd, rc);	/* resets "fd" to FD_INVALID */
		return save_errno;
	}
	/*  Initialize the listen queue */
	if (listen(fd, 5) < 0)
	{
		save_errno = errno;
		CLOSEFILE_RESET(fd, rc);	/* resets "fd" to FD_INVALID */
		return save_errno;
	}
	/* set up raw socket for use with pinging option */
	if (ping_keepalive)
		psock = init_ping();
	/*  Store the file descriptor away for use later */
	cll->nve = fd;
	OMI_DBG_STMP;
	OMI_DBG((omi_debug, "%s: socket registered at port %d\n", SRVR_NAME, (int)port));
#ifdef GTCM_RC
	OMI_DBG((omi_debug, "RC server ID %d, Process ID %d\n", rc_server_id, omi_pid));
#endif
	if (authenticate)
		OMI_DBG((omi_debug, "Password verification on OMI connections enabled.\n"));
	if (!one_conn_per_inaddr)
		OMI_DBG((omi_debug, "Multiple connections from the same internet address allowed.\n"));
	if (psock > 0)
		OMI_DBG((omi_debug, "Keepalive option (-ping) enabled.\n"));
	return 0;
#else /* defined(BSD_TCP) */
#ifdef SYSV_TCP
	if ((fd = t_open(SYSV_TCP, O_RDWR, NULL)) < 0)
	{
		save_errno = errno;
		return save_errno;
	}
	if (!(bind = (struct t_bind *)t_alloc(fd, T_BIND, T_ALL)))
	{
		save_errno = errno;
		(void) t_close(fd);
		return save_errno;
	}
	bind->qlen      = 5;
	bind->addr.len  = 0;
	bind->addr.buf  = 0;
	if (t_bind(fd, bind, bind) < 0)
	{
		save_errno = errno;
		(void) t_free(bind, T_BIND);
		(void) t_close(fd);
		return save_errno;
	}
	/*  Store the file descriptor away for use later */
	cll->nve = fd;
	OMI_DBG_STMP;
	OMI_DBG((omi_debug, "%s: socket registered at port %d\n", SRVR_NAME, (int)port));
#ifdef GTCM_RC
	OMI_DBG((omi_debug, "RC server ID %d\n", rc_server_id));
#endif
	return 0;
#else /* defined(SYSV_TCP) */
	cll->nve = FD_INVALID;
	return -1;
#endif /* !defined(SYSV_TCP) */
#endif /* !defined(BSD_TCP) */
}
