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

/* iotcp_list.c - routines to maintain a list of TCP/IP listening sockets */

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_socket.h"
#include "gtm_inet.h"

#include <errno.h>

#include "io_params.h"
#include "io.h"
#include "iotcproutine.h"
#include "iotcpdef.h"

error_def(ERR_SOCKINIT);
error_def(ERR_TEXT);

/* list of listening sockets */
typedef struct	lsock_rec_s
{
	int			socket;
	struct sockaddr_storage	sas;
	struct addrinfo		ai;
	struct lsock_rec_s	*next;
	io_log_name		*ldev;		/* listening device record */
} lsock_rec;

static lsock_rec		*lsock_list = NULL;

GBLREF tcp_library_struct        tcp_routines;

int	iotcp_newlsock(io_log_name *dev, d_tcp_struct *tcpptr);

/* find the listening socket associated with the address of this
 * new socket, creating a listening socket if there is none.
 */
int	iotcp_getlsock(io_log_name *dev)
{
	d_tcp_struct *tcpptr;
	lsock_rec *ls;

#ifdef DEBUG_TCP
	PRINTF("iotcp_getlsock ---\n");
#endif

	tcpptr = (d_tcp_struct *)dev->iod->dev_sp;

	for (ls = lsock_list;  ls != NULL;  ls = ls->next)
		if (0 == memcmp(&(tcpptr->sas), &(ls->sas), SIZEOF(tcpptr->sas)))
			return ls->socket;

	return iotcp_newlsock(dev, tcpptr);
}


int	iotcp_newlsock(io_log_name *dev, d_tcp_struct *tcpptr)
{
	lsock_rec	*new_lsock;

	io_log_name	*ldev;		/* listening device */
	d_tcp_struct	*lsock_tcp;
	int		lsock;
	char		buf[MAX_TRANS_NAME_LEN];
	static char	ones[] = {1, 1, 1};
	mstr		ldev_name;
	int		on = 1;
	char		*errptr;
	int4		errlen;

#ifdef DEBUG_TCP
	PRINTF("iotcp_newlsock ---\n");
#endif

	lsock = tcp_routines.aa_socket(tcpptr->ai.ai_family, tcpptr->ai.ai_socktype, tcpptr->ai.ai_protocol);
	if (lsock == -1)
	{
		errptr = (char *)STRERROR(errno);
		errlen = STRLEN(errptr);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_SOCKINIT, 3, errno, errlen, errptr);
		return 0;
	}

	/* allow multiple connections to the same IP address */
	if (tcp_routines.aa_setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, &on, SIZEOF(on)) == -1)
	{
		errptr = (char *)STRERROR(errno);
		errlen = STRLEN(errptr);
		(void)tcp_routines.aa_close(lsock);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_SOCKINIT, 3, errno, errlen, errptr);
		return 0;
	}

	if (-1 == tcp_routines.aa_bind(lsock, tcpptr->ai.ai_addr, tcpptr->ai.ai_addrlen))
	{
		errptr = (char *)STRERROR(errno);
		errlen = STRLEN(errptr);
		(void)tcp_routines.aa_close(lsock);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_SOCKINIT, 3, errno, errlen, errptr);
		return 0;
	}

	/* establish a queue of length 5 for incoming connections */
	if (tcp_routines.aa_listen(lsock, 5) == -1)
	{
		errptr = (char *)STRERROR(errno);
		errlen = STRLEN(errptr);
		(void)tcp_routines.aa_close(lsock);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_SOCKINIT, 3, errno, errlen, errptr);
		return 0;
	}

	/* create an extra device for the listening socket (device
	 * name is the user-specified device name with three ^A's
	 * appended).  We need this device to be on the io_log_name
	 * list so that it gets closed automatically when the user's
	 * program exits.
	 */
	ldev_name.addr=buf;
	memcpy(ldev_name.addr, dev->dollar_io, dev->len);
	memcpy(ldev_name.addr+dev->len, ones, 3);
	ldev_name.len = dev->len + 2;
	ldev = get_log_name(&ldev_name, INSERT);

	/* copy all state information from the current device */

	/* io descriptor */
	ldev->iod = (io_desc *)malloc(SIZEOF(io_desc));
	memcpy(ldev->iod, dev->iod, SIZEOF(io_desc));
	ldev->iod->state = dev_open;
	ldev->iod->pair.in  = ldev->iod;
	ldev->iod->pair.out = ldev->iod;

	/* tcp-specific information */
	ldev->iod->dev_sp = (void *)malloc(SIZEOF(d_tcp_struct));
	lsock_tcp=(d_tcp_struct *)ldev->iod->dev_sp;
	memcpy(lsock_tcp, tcpptr, SIZEOF(d_tcp_struct));

	lsock_tcp->socket = lsock;
	ldev->iod->state = dev_open;

	/* add to our list of tcp listening sockets */
	new_lsock = (lsock_rec *)malloc(SIZEOF(lsock_rec));
	new_lsock->socket = lsock;
	new_lsock->ai = tcpptr->ai;
	new_lsock->sas = tcpptr->sas;
	new_lsock->ai.ai_addr = (struct sockaddr *)(&new_lsock->sas);
	new_lsock->next = lsock_list;
	new_lsock->ldev = ldev;
	lsock_list = new_lsock;

	return lsock;
}


void iotcp_rmlsock(io_desc *iod)
{
	lsock_rec	*ls, *prev, *next;
	d_tcp_struct	*tcpptr = (d_tcp_struct *)iod->dev_sp;

	for (prev = NULL, ls = lsock_list;  ls != NULL;)
	{
		next = ls->next;
		/* Actually it's enough to just compare the port number, however extracting port from
		 * sas needs to call getnameinfo(). Same sas can guarantee the same port, so
		 * it's enough to use sas to detect whether the device is what we want to delete
		 */
		if (0 == memcmp(&(tcpptr->sas), &(ls->sas), SIZEOF(tcpptr->sas)))
		{
			if (prev)
				prev->next = ls->next;
			else
				lsock_list = ls->next;
			tcp_routines.aa_close(ls->socket);
			ls->ldev->iod->state = dev_closed;
			free(ls);
		}
		else
			prev = ls;
		ls = next;
	}
}
