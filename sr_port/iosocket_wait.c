/****************************************************************
 *								*
 *	Copyright 2013 Fidelity Information Services, Inc	*
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
#include <rtnhdr.h>
#include "stack_frame.h"
#include "mv_stent.h"
#include "gtm_netdb.h"
#include "gtm_stdlib.h"

#define	CONNECTED	"CONNECT"
#define READ	"READ"

GBLREF tcp_library_struct	tcp_routines;
GBLREF volatile int4		outofband;
GBLREF int4			gtm_max_sockets;
GBLREF int			socketus_interruptus;
GBLREF boolean_t		dollar_zininterrupt;
GBLREF stack_frame  	        *frame_pointer;
GBLREF unsigned char            *stackbase, *stacktop, *msp, *stackwarn;
GBLREF mv_stent			*mv_chain;

error_def(ERR_GETNAMEINFO);
error_def(ERR_SOCKACPT);
error_def(ERR_SOCKWAIT);
error_def(ERR_TEXT);
error_def(ERR_SOCKMAX);
error_def(ERR_ZINTRECURSEIO);
error_def(ERR_STACKCRIT);
error_def(ERR_STACKOFLOW);

boolean_t iosocket_wait(io_desc *iod, int4 timepar)
{
	struct 	timeval  	utimeout;
	ABS_TIME		cur_time, end_time;
	struct 	sockaddr_storage    	peer;           /* socket address + port */
	fd_set    		tcp_fd;
	d_socket_struct 	*dsocketptr;
	socket_struct   	*socketptr, *newsocketptr;
	socket_interrupt	*sockintr;
	char            	*errptr;
	int4            	errlen, ii, msec_timeout;
	int			rv, max_fd, len;
	GTM_SOCKLEN_TYPE	size;
	boolean_t		zint_restart;
	mv_stent		*mv_zintdev;
	int			retry_num;
	struct sockaddr		*peer_sa_ptr;
	char			port_buffer[NI_MAXSERV], ipaddr[SA_MAXLEN + 1];
	int			errcode;

	/* check for validity */
	assert(iod->type == gtmsocket);
	dsocketptr = (d_socket_struct *)iod->dev_sp;
	sockintr = &dsocketptr->sock_save_state;
	peer_sa_ptr = ((struct sockaddr *)(&peer));

	/* Check for restart */
	if (!dsocketptr->mupintr)
		/* Simple path, no worries*/
		zint_restart = FALSE;
	else
	{       /* We have a pending wait restart of some sort - check we aren't recursing on this device */
		if (sockwhich_invalid == sockintr->who_saved)
			GTMASSERT;	/* Interrupt should never have an invalid save state */
		if (dollar_zininterrupt)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZINTRECURSEIO);
		if (sockwhich_wait != sockintr->who_saved)
			GTMASSERT;      /* ZINTRECURSEIO should have caught */
		DBGSOCK((stdout, "socwait: *#*#*#*#*#*#*#  Restarted interrupted wait\n"));
		mv_zintdev = io_find_mvstent(iod, FALSE);
		if (mv_zintdev)
		{
			if (sockintr->end_time_valid)
				/* Restore end_time for timeout */
				end_time = sockintr->end_time;

			/* Done with this mv_stent. Pop it off if we can, else mark it inactive. */
			if (mv_chain == mv_zintdev)
				POP_MV_STENT();         /* pop if top of stack */
			else
			{       /* else mark it unused */
				mv_zintdev->mv_st_cont.mvs_zintdev.buffer_valid = FALSE;
				mv_zintdev->mv_st_cont.mvs_zintdev.io_ptr = NULL;
			}
			zint_restart = TRUE;
			DBGSOCK((stdout, "socwait: mv_stent found - endtime: %d/%d\n", end_time.at_sec, end_time.at_usec));
		} else
			DBGSOCK((stdout, "socwait: no mv_stent found !!\n"));
		dsocketptr->mupintr = FALSE;
		sockintr->who_saved = sockwhich_invalid;
	}
	/* check for events */
	FD_ZERO(&tcp_fd);
	while (TRUE)
	{
		max_fd = 0;
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
		if (!zint_restart || !sockintr->end_time_valid)
			add_int_to_abs_time(&cur_time, msec_timeout, &end_time);
		else
		{       /* end_time taken from restart data. Compute what msec_timeout should be so timeout timer
					   gets set correctly below.
			*/
			DBGSOCK((stdout, "socwait: Taking timeout end time from wait restart data\n"));
			cur_time = sub_abs_time(&end_time, &cur_time);
			if (0 > cur_time.at_sec)
			{
				msec_timeout = -1;
				utimeout.tv_sec = 0;
				utimeout.tv_usec = 0;
			} else
			{
				msec_timeout = (int4)(cur_time.at_sec * 1000 + cur_time.at_usec / 1000);
				utimeout.tv_sec = cur_time.at_sec;
				utimeout.tv_usec = (gtm_tv_usec_t)cur_time.at_usec;
			}
		}
		sockintr->end_time_valid = FALSE;
		for ( ; ; )
		{
			rv = select(max_fd + 1, (void *)&tcp_fd, (void *)0, (void *)0,
				    (timepar == NO_M_TIMEOUT ? (struct timeval *)0 : &utimeout));
			if (0 > rv && EINTR == errno)
			{
				if (0 != outofband)
				{
					DBGSOCK((stdout, "socwait: outofband interrupt received (%d) -- "
						 "queueing mv_stent for wait intr\n", outofband));
					PUSH_MV_STENT(MVST_ZINTDEV);
					mv_chain->mv_st_cont.mvs_zintdev.io_ptr = iod;
					mv_chain->mv_st_cont.mvs_zintdev.buffer_valid = FALSE;
					sockintr->who_saved = sockwhich_wait;
					sockintr->end_time = end_time;
					sockintr->end_time_valid = TRUE;
					dsocketptr->mupintr = TRUE;
					socketus_interruptus++;
					DBGSOCK((stdout, "socwait: mv_stent queued - endtime: %d/%d  interrupts: %d\n",
						 end_time.at_sec, end_time.at_usec, socketus_interruptus));
					outofband_action(FALSE);
					GTMASSERT;      /* Should *never* return from outofband_action */
					return FALSE;   /* For the compiler.. */
				}
				sys_get_curr_time(&cur_time);
				cur_time = sub_abs_time(&end_time, &cur_time);
				if (0 > cur_time.at_sec)
				{
					rv = 0;		/* time out */
					break;
				}
				utimeout.tv_sec = cur_time.at_sec;
				utimeout.tv_usec = (gtm_tv_usec_t)cur_time.at_usec;
			} else
				break;	/* either other error or done */
		}
		if (rv == 0)
		{
			iod->dollar.key[0] = '\0';
			return FALSE;
		} else  if (rv < 0)
		{
			errptr = (char *)STRERROR(errno);
			errlen = STRLEN(errptr);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_SOCKWAIT, 0, ERR_TEXT, 2, errlen, errptr);
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
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_SOCKMAX, 1, gtm_max_sockets);
				return FALSE;
			}

			size = SIZEOF(struct sockaddr_storage);
			rv = tcp_routines.aa_accept(socketptr->sd, peer_sa_ptr, &size);
			if (-1 == rv)
			{
#				ifdef __hpux
				if (ENOBUFS == errno)
					continue;	/* On HP-UX, ENOBUFS may indicate a transient condition; retry */
#				endif
				errptr = (char *)STRERROR(errno);
				errlen = STRLEN(errptr);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_SOCKACPT, 0, ERR_TEXT, 2, errlen, errptr);
				return FALSE;
			}
			SOCKET_DUP(socketptr, newsocketptr);
			newsocketptr->sd = rv;
			SOCKET_ADDR_COPY(newsocketptr->remote, peer_sa_ptr, size);
			/* translate internal address to numeric ip address */
			GETNAMEINFO(peer_sa_ptr, size, ipaddr, SA_MAXLEN, NULL, 0, NI_NUMERICHOST, errcode);
			if (0 != errcode)
			{
				SOCKET_FREE(newsocketptr);
				RTS_ERROR_ADDRINFO(NULL, ERR_GETNAMEINFO, errcode);
				return FALSE;
			}
			if (NULL != newsocketptr->remote.saddr_ip)
				free(newsocketptr->remote.saddr_ip);
			STRNDUP(ipaddr, SA_MAXLEN, newsocketptr->remote.saddr_ip);
			/* translate internal address to port number*/
			GETNAMEINFO(peer_sa_ptr, size, NULL, 0, port_buffer, NI_MAXSERV, NI_NUMERICSERV, errcode);
			if (0 != errcode)
			{
				SOCKET_FREE(newsocketptr);
				RTS_ERROR_ADDRINFO(NULL, ERR_GETNAMEINFO, errcode);
				return FALSE;
			}
			newsocketptr->remote.port = ATOI(port_buffer);
			newsocketptr->state = socket_connected;
			newsocketptr->passive = FALSE;
			newsocketptr->first_read = newsocketptr->first_write = TRUE;
			/* put the new-born socket to the list and create a handle for it */
			iosocket_handle(newsocketptr->handle, &newsocketptr->handle_len, TRUE, dsocketptr);
			dsocketptr->socket[dsocketptr->n_socket++] = newsocketptr;
			dsocketptr->current_socket = dsocketptr->n_socket - 1;
			len = SIZEOF(CONNECTED) - 1;
			memcpy(&iod->dollar.key[0], CONNECTED, len);
			iod->dollar.key[len++] = '|';
			memcpy(&iod->dollar.key[len], newsocketptr->handle, newsocketptr->handle_len);
			len += newsocketptr->handle_len;
			iod->dollar.key[len++] = '|';
			strncpy(&iod->dollar.key[len], newsocketptr->remote.saddr_ip, DD_BUFLEN - 1 - len);
			iod->dollar.key[DD_BUFLEN-1] = '\0';		/* In case we fill the buffer */
		} else
		{
			assert(socket_connected == socketptr->state);
			dsocketptr->current_socket = ii;
			len = SIZEOF(READ) - 1;
			memcpy(&iod->dollar.key[0], READ, len);
			iod->dollar.key[len++] = '|';
			memcpy(&iod->dollar.key[len], socketptr->handle, socketptr->handle_len);
			len += socketptr->handle_len;
			iod->dollar.key[len++] = '|';
			if (NULL != socketptr->remote.saddr_ip)
			{
				strncpy(&iod->dollar.key[len], socketptr->remote.saddr_ip, DD_BUFLEN - 1 - len);
				iod->dollar.key[DD_BUFLEN-1] = '\0';
			} else
				iod->dollar.key[len] = '\0';
		}
		break;
	}
	return TRUE;
}
