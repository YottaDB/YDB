/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* iosocket_wait.c
 *
 * return a listening socket -- create a new socket for the connection and set it to current
 *				set it to current
 *				set $KEY to "CONNECT"
 * return a connected socket -- set it to current
 *				set $KEY to "READ"
 * timeout		     -- set $Test to 1
 */
#include "mdef.h"
#include <errno.h>
#include "gtm_socket.h"
#include "gtm_inet.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "io_params.h"
#include "gt_timer.h"
#include "io.h"
#include "iotimer.h"
#include "iotcp_select.h"
#include "iotcproutine.h"
#include "iotcpdef.h"
#include "iosocketdef.h"
#include "min_max.h"
#include "outofband.h"

#define	CONNECTED	"CONNECT"
#define READ	"READ"
GBLREF tcp_library_struct	tcp_routines;
GBLREF volatile int4		outofband;
GBLREF int4			gtm_max_sockets;

boolean_t iosocket_wait(io_desc *iod, int4 timepar)
{
	struct 	timeval  	utimeout;
	ABS_TIME		cur_time, end_time;
	struct 	sockaddr_in     peer;           /* socket address + port */
        fd_set    		tcp_fd;
        d_socket_struct 	*dsocketptr;
        socket_struct   	*socketptr, *newsocketptr;
        char            	*errptr;
        int4            	errlen, ii, msec_timeout;
	int			rv, size, max_fd;
	short			len;

        error_def(ERR_SOCKACPT);
        error_def(ERR_SOCKWAIT);
        error_def(ERR_TEXT);
	error_def(ERR_SOCKMAX);

	/* check for validity */
        assert(iod->type == gtmsocket);
        dsocketptr = (d_socket_struct *)iod->dev_sp;
	/* check for events */
	max_fd = 0;
	FD_ZERO(&tcp_fd);
	for (ii = 0; ii < dsocketptr->n_socket; ii++)
	{
		socketptr = dsocketptr->socket[ii];
		if ((socket_listening == socketptr->state) || (socket_connected == socketptr->state))
		{
			FD_SET(socketptr->sd, &tcp_fd);
			max_fd = MAX(max_fd, socketptr->sd);
		}
	}
	utimeout.tv_sec = timepar;
	utimeout.tv_usec = 0;
	msec_timeout = timeout2msec(timepar);
	sys_get_curr_time(&cur_time);
	add_int_to_abs_time(&cur_time, msec_timeout, &end_time);
	for ( ; ; )
	{
		rv = tcp_routines.aa_select(max_fd + 1, (void *)&tcp_fd, (void *)0, (void *)0,
			(timepar == NO_M_TIMEOUT ? (struct timeval *)0 : &utimeout));
		if (0 > rv && EINTR == errno)
		{
			if (OUTOFBANDNOW(outofband))
			{
				rv = 0;		/* treat as time out */
				break;
			}
			sys_get_curr_time(&cur_time);
			cur_time = sub_abs_time(&end_time, &cur_time);
			if (0 > cur_time.at_sec)
			{
				rv = 0;		/* time out */
				break;
			}
			utimeout.tv_sec = cur_time.at_sec;
			utimeout.tv_usec = cur_time.at_usec;
		} else
			break;	/* either other error or done */
	}
	if (rv == 0)
	{
		dsocketptr->dollar_key[0] = '\0';
		return FALSE;
	} else  if (rv < 0)
	{
		errptr = (char *)STRERROR(errno);
		errlen = strlen(errptr);
		rts_error(VARLSTCNT(6) ERR_SOCKWAIT, 0, ERR_TEXT, 2, errlen, errptr);
		return FALSE;
	}
	/* find out which socket is ready */
	for (ii = 0; ii < dsocketptr->n_socket; ii++)
	{
		socketptr = dsocketptr->socket[ii];
		if (0 != FD_ISSET(socketptr->sd, &tcp_fd))
			break;
	}
	assert(ii < dsocketptr->n_socket);
	if (socket_listening == socketptr->state)
	{
	        if (gtm_max_sockets <= dsocketptr->n_socket)
                {
                        rts_error(VARLSTCNT(3) ERR_SOCKMAX, 1, gtm_max_sockets);
                        return FALSE;
                }
		size = sizeof(struct sockaddr_in);
		rv = tcp_routines.aa_accept(socketptr->sd, &peer, &size);
		if (rv == -1)
		{
			errptr = (char *)STRERROR(errno);
			errlen = strlen(errptr);
			rts_error(VARLSTCNT(6) ERR_SOCKACPT, 0, ERR_TEXT, 2, errlen, errptr);
			return FALSE;
		}
		/* got the connection, create a new socket in the device socket list */
		newsocketptr = (socket_struct *)malloc(sizeof(socket_struct));
		*newsocketptr = *socketptr;
		newsocketptr->sd = rv;
		memcpy(&newsocketptr->remote.sin, &peer, sizeof(struct sockaddr_in));
		SPRINTF(newsocketptr->remote.saddr_ip, "%s", tcp_routines.aa_inet_ntoa(peer.sin_addr));
		newsocketptr->remote.port = GTM_NTOHS(peer.sin_port);
		newsocketptr->state = socket_connected;
		newsocketptr->passive = FALSE;
		iosocket_delimiter_copy(socketptr, newsocketptr);
		newsocketptr->buffer = (char *)malloc(socketptr->buffer_size);
		newsocketptr->buffer_size = socketptr->buffer_size;
		newsocketptr->buffered_length = socketptr->buffered_offset = 0;
		newsocketptr->first_read = newsocketptr->first_write = TRUE;
		/* put the new-born socket to the list and create a handle for it */
		iosocket_handle(newsocketptr->handle, &newsocketptr->handle_len, TRUE, dsocketptr);
		dsocketptr->socket[dsocketptr->n_socket++] = newsocketptr;
		dsocketptr->current_socket = dsocketptr->n_socket - 1;
		len = sizeof(CONNECTED) - 1;
		memcpy(&dsocketptr->dollar_key[0], CONNECTED, len);
		dsocketptr->dollar_key[len++] = '|';
		memcpy(&dsocketptr->dollar_key[len], newsocketptr->handle, newsocketptr->handle_len);
		len += newsocketptr->handle_len;
		dsocketptr->dollar_key[len++] = '|';
		MEMCPY_STR(&dsocketptr->dollar_key[len], newsocketptr->remote.saddr_ip);
		len += strlen(newsocketptr->remote.saddr_ip);
		dsocketptr->dollar_key[len] = '\0';
	} else
	{
		assert(socket_connected == socketptr->state);
		dsocketptr->current_socket = ii;
		len = sizeof(READ) - 1;
		memcpy(&dsocketptr->dollar_key[0], READ, len);
		dsocketptr->dollar_key[len++] = '|';
		memcpy(&dsocketptr->dollar_key[len], socketptr->handle, socketptr->handle_len);
                len += socketptr->handle_len;
                dsocketptr->dollar_key[len++] = '|';
                MEMCPY_STR(&dsocketptr->dollar_key[len], socketptr->remote.saddr_ip);
                len += strlen(socketptr->remote.saddr_ip);
                dsocketptr->dollar_key[len] = '\0';
	}
	return TRUE;
}
