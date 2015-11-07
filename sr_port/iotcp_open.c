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

/* iotcp_open.c - open a TCP/IP connection
 *  Parameters-
 *	dev		- the logical name associated with this socket (ignored by this routine).
 *	pp->str.addr 	- device parameters.  The "stream" parameter is required.
 *	file_des	- unused. (UNIX only)
 *	mspace		- unused.
 *	t		- maximum time to wait for a connection (in ms).
 *
 *  Returns-
 *	non-zero	- socket successfully opened and ready for I/O
 *	zero		- open operation failed or timed out.
 */
#include "mdef.h"

#include <errno.h>
#include "gtm_fcntl.h"
#include "gtm_time.h"
#include "gtm_socket.h"
#include "gtm_netdb.h"	/* gtm_netdb must be in front so that AI_V4MAPPED will be defined */
#include "gtm_ipv6.h"
#include "gtm_inet.h"
#include "gtm_ctype.h"
#include "gtm_string.h"
#include "gtm_stdio.h"

#include "copy.h"
#include "gt_timer.h"
#include "io.h"
#include "iotimer.h"
#include "iotcp_select.h"
#include "iotcpdef.h"
#include "iotcpdefsp.h"
#include "iotcproutine.h"
#include "io_params.h"
#include "stringpool.h"
#include "outofband.h"
#include "wake_alarm.h"
#include "util.h"

GBLREF tcp_library_struct	tcp_routines;
GBLREF bool			out_of_time;
GBLREF volatile int4		outofband;
LITREF unsigned char		io_params_size[];

error_def(ERR_DEVPARMNEG);
error_def(ERR_GETADDRINFO);
error_def(ERR_GETNAMEINFO);
error_def(ERR_INVADDRSPEC);
error_def(ERR_INVPORTSPEC);
error_def(ERR_IPADDRREQ);
error_def(ERR_OPENCONN);
error_def(ERR_SOCKACPT);
error_def(ERR_SOCKINIT);
error_def(ERR_SOCKPARMREQ);
error_def(ERR_SOCKWAIT);
error_def(ERR_TEXT);

short	iotcp_open(io_log_name *dev, mval *pp, int file_des, mval *mspace, int4 timeout)
{
	boolean_t		no_time_left = FALSE, timed;
	char			addr[SA_MAXLEN + 1], *errptr, sockaddr[SA_MAXLEN + 1],
				temp_addr[SA_MAXLEN + 1], temp_ch;
	char			ipname[SA_MAXLEN];
	unsigned char		ch, len;
	int4			length, width;
	unsigned short		port;
	int4			errlen, msec_timeout;
	GTM_SOCKLEN_TYPE	size;
	int			ii, status,
				on = 1,
				p_offset = 0,
				temp_1 = -2;
	TID			timer_id;
	ABS_TIME		cur_time, end_time, time_for_read, lcl_time_for_read;
	d_tcp_struct		*tcpptr, newtcp;
	io_desc			*ioptr;
	struct sockaddr_storage	peer_sas;		/* socket address + port */
	fd_set			tcp_fd;
	int			lsock;
	short 			retry_num;
	const char		*terrptr;
	int			errcode;
	char			port_buffer[NI_MAXSERV];
	int			port_len;
	struct addrinfo		*ai_ptr = NULL, *remote_ai_ptr = NULL, *tmp_ai_ptr, hints;
	int			host_len; /* addr_len + port_len + delimeters */
	int			af;
	int			test_ipv6_sd;

#ifdef	DEBUG_TCP
	PRINTF("iotcp_open.c >>>   tt = %d\n", t);
#endif
	ioptr = dev->iod;
	assert((params) *(pp->str.addr + p_offset) < (unsigned char)n_iops);
	assert(0 != ioptr);
	assert(ioptr->state >= 0 && ioptr->state < n_io_dev_states);
	assert(tcp == ioptr->type);
	if (dev_never_opened == ioptr->state)
	{
		ioptr->dev_sp = (void *)malloc(SIZEOF(d_tcp_struct));
		memset(ioptr->dev_sp, 0, SIZEOF(d_tcp_struct));
	}
	tcpptr = (d_tcp_struct *)ioptr->dev_sp;
	if (dev_never_opened == ioptr->state)
	{
		ioptr->state	= dev_closed;
		ioptr->width	= TCPDEF_WIDTH;
		ioptr->length	= TCPDEF_LENGTH;
		ioptr->wrap	= TRUE;
		if (-1 == iotcp_fillroutine())
			assert(FALSE);
	}
	ioptr->dollar.zeof = FALSE;
	newtcp = *tcpptr;
	memcpy(ioptr->dollar.device, LITZERO, SIZEOF(LITZERO));
	newtcp.passive = FALSE;
	while (iop_eol != *(pp->str.addr + p_offset))
	{
		switch	(ch = *(pp->str.addr + p_offset++))
		{
		case	iop_width:
			GET_LONG(width, pp->str.addr + p_offset);
			if (0 == width)
				newtcp.width = TCPDEF_WIDTH;
			else if (width > 0)
				newtcp.width = width;
			else
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1)	ERR_DEVPARMNEG);
			break;
		case	iop_length:
			GET_LONG(length, pp->str.addr + p_offset);
			if (0 == length)
				newtcp.length = TCPDEF_LENGTH;
			else if (length > 0)
				newtcp.length = length;
			else
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_DEVPARMNEG);
			break;
		case	iop_listen:
			newtcp.passive = TRUE;
			break;
		case	iop_socket:
			/* test whether ipv6 socket is supported on local system */
			af = ((GTM_IPV6_SUPPORTED && !ipv4_only) ? AF_INET6 : AF_INET);
			if (AF_INET6 == af)
			{
				test_ipv6_sd = tcp_routines.aa_socket(af, SOCK_STREAM, IPPROTO_TCP);
				if (-1 == test_ipv6_sd)
					af = AF_INET;
				else
					tcp_routines.aa_close(test_ipv6_sd);
			}
			len = *(pp->str.addr + p_offset);
			memset(sockaddr, 0, SA_MAXLEN + 1);
			memcpy(sockaddr, pp->str.addr + p_offset + 1, (len <= USR_SA_MAXLITLEN) ? len : USR_SA_MAXLITLEN);
			*temp_addr = '\0';
			*addr = '\0';
			port = 0;
			if (SSCANF(sockaddr, "%[^,], %hu", temp_addr, &port) < 2)
			{
				if (SSCANF(sockaddr, ",%hu", &port) < 1)
				{
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INVPORTSPEC);
					return	FALSE;
				}
				SERVER_HINTS(hints, af);
				port_len = 0;
				I2A(port_buffer, port_len, port);
				port_buffer[port_len]='\0';
				if (0 != (errcode = getaddrinfo(NULL, port_buffer, &hints, &ai_ptr)))
				{
					RTS_ERROR_ADDRINFO(NULL, ERR_GETADDRINFO, errcode);
					return FALSE;
				}
				memcpy(&(newtcp.ai), ai_ptr, SIZEOF(*ai_ptr));
				memcpy(&(newtcp.sas), ai_ptr->ai_addr, ai_ptr->ai_addrlen);
				freeaddrinfo(ai_ptr);
				newtcp.ai.ai_addr = (struct sockaddr*)(&newtcp.sas);

			} else
			{	/* client side (connection side) */
				SPRINTF(addr, "%s", temp_addr);
				SPRINTF(port_buffer, "%hu", port);
				CLIENT_HINTS_AF(hints, af);
				if (0 != (errcode = getaddrinfo(addr, port_buffer, &hints, &remote_ai_ptr)))
				{
					if(AF_INET6 == af)
					{
						af = AF_INET;
						CLIENT_HINTS_AF(hints, af);
						errcode = getaddrinfo(addr, port_buffer, &hints, &remote_ai_ptr);
					}
					if(errcode)
					{
						RTS_ERROR_ADDRINFO(NULL, ERR_GETADDRINFO, errcode);
						return FALSE;
					}
				}
				memcpy(&(newtcp.ai), remote_ai_ptr, SIZEOF(struct addrinfo));
				memcpy(&(newtcp.sas), remote_ai_ptr->ai_addr, remote_ai_ptr->ai_addrlen);
				newtcp.ai.ai_addr = (struct sockaddr*)(&newtcp.sas);
				freeaddrinfo(remote_ai_ptr);
			}
			break;
		case	iop_exception:
			ioptr->error_handler.len = *(pp->str.addr + p_offset);
			ioptr->error_handler.addr = (char *)(pp->str.addr + p_offset + 1);
			s2pool(&ioptr->error_handler);
			break;
		default:
			break;
		}
		p_offset += ((IOP_VAR_SIZE == io_params_size[ch]) ?
			(unsigned char)*(pp->str.addr + p_offset) + 1 : io_params_size[ch]);
	}
	/* the previous check if ((0 == newtcp.sin.sin_port) && (0 == newtcp.sin.sin_addr.s_addr)) is no longer needed as
   	 * getaddrinfo() will return error for the above case
	 */
	/* active connection must have a complete address specification */
	if (dev_closed == ioptr->state)
	{
		if (newtcp.passive)		/* server side */
		{
			/* no listening socket for this addr?  make one. */
			memcpy(ioptr->dev_sp, &newtcp, SIZEOF(d_tcp_struct));
			if (!(lsock = iotcp_getlsock(dev)))
				return	FALSE;	/* could not create listening socket */
			timer_id = (TID)iotcp_open;
			time_for_read.at_sec = ((0 == timeout) ? 0 : 1);
			time_for_read.at_usec = 0;
			while (TRUE)
			{
				out_of_time = FALSE;
				if (NO_M_TIMEOUT == timeout)
				{
					timed = FALSE;
					msec_timeout = NO_M_TIMEOUT;
				} else
				{
					timed = TRUE;
					msec_timeout = timeout2msec(timeout);
					if (msec_timeout > 0)
					{       /* there is time to wait */
						sys_get_curr_time(&cur_time);
						add_int_to_abs_time(&cur_time, msec_timeout, &end_time);
						start_timer(timer_id, msec_timeout, wake_alarm, 0, NULL);
					} else
						out_of_time = TRUE;
				}
				for (status = 0; 0 == status; )
				{
					FD_ZERO(&tcp_fd);
					FD_SET(lsock, &tcp_fd);
					/*
					 * Note: the check for EINTR from the select below should remain, as aa_select is a
					 * function, and not all callers of aa_select behave the same when EINTR is returned.
					 */
					lcl_time_for_read = time_for_read;
					status = tcp_routines.aa_select(lsock + 1, (void *)&tcp_fd, (void *)0, (void *)0,
									&lcl_time_for_read);
					if (0 > status)
					{
						if (EINTR == errno && FALSE == out_of_time)
							/* interrupted by a signal which is not OUR timer */
							status = 0;
						else
							break;
					}
					if (outofband)
						break;
					if (timed)
					{
						if (msec_timeout > 0)
						{
							sys_get_curr_time(&cur_time);
							cur_time = sub_abs_time(&end_time, &cur_time);
							if (cur_time.at_sec <= 0)
							{
								out_of_time = TRUE;
								cancel_timer(timer_id);
								break;
							}
						} else
							break;
					}
				}
				if (timed)
				{
					if (0 != msec_timeout)
					{
						cancel_timer(timer_id);
						if (out_of_time || outofband)
							return FALSE;
						/*if (outofband)
							outofband_action(FALSE);*/
					}
				}
				if (0 > status)
				{
					errptr = (char *)STRERROR(errno);
					errlen = STRLEN(errptr);
					iotcp_rmlsock((io_desc *)dev->iod);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_SOCKWAIT, 0, ERR_TEXT, 2, errlen, errptr);
					return FALSE;
				}
				size = SIZEOF(struct sockaddr_storage);
				status = tcp_routines.aa_accept(lsock, (struct sockaddr*)&peer_sas, &size);
				if (-1 == status)
				{
#					ifdef __hpux
					if (ENOBUFS == errno)
						continue;
#					endif
					errptr = (char *)STRERROR(errno);
					errlen = STRLEN(errptr);
					iotcp_rmlsock((io_desc *)dev->iod);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_SOCKACPT, 0, ERR_TEXT, 2, errlen, errptr);
					return FALSE;
				}
				newtcp.socket = status;
				break;
			}
		} else			/* client side */
		{
			if (NO_M_TIMEOUT != timeout)
			{
				msec_timeout = timeout2msec(timeout);
				sys_get_curr_time(&cur_time);
				add_int_to_abs_time(&cur_time, msec_timeout, &end_time);
			}
			no_time_left = FALSE;
			temp_1 = 1;
			do
			{
				if (1 != temp_1)
					tcp_routines.aa_close(newtcp.socket);
				newtcp.socket = tcp_routines.aa_socket(newtcp.ai.ai_family, newtcp.ai.ai_socktype,
								       newtcp.ai.ai_protocol);
				if (-1 == newtcp.socket)
				{
					errptr = (char *)STRERROR(errno);
					errlen = STRLEN(errptr);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_SOCKINIT, 3, errno, errlen, errptr);
					return FALSE;
				}
				/*	allow multiple connections to the same IP address */
				if	(-1 == tcp_routines.aa_setsockopt(newtcp.socket, SOL_SOCKET, SO_REUSEADDR, &on, SIZEOF(on)))
				{
					(void)tcp_routines.aa_close(newtcp.socket);
					errptr = (char *)STRERROR(errno);
					errlen = STRLEN(errptr);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_SOCKINIT, 3, errno, errlen, errptr);
					return FALSE;
				}
				size=SIZEOF(newtcp.bufsiz);
				if (-1 == tcp_routines.aa_getsockopt(newtcp.socket, SOL_SOCKET, SO_RCVBUF, &newtcp.bufsiz, &size))
				{
					(void)tcp_routines.aa_close(newtcp.socket);
					errptr = (char *)STRERROR(errno);
					errlen = STRLEN(errptr);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_SOCKINIT, 3, errno, errlen, errptr);
					return FALSE;
				}
				/*
				 * Note: the check for EINTR from the connect need not be converted to an EINTR wrapper macro,
				 * since the connect is not retried on EINTR.
				 */
				temp_1 = tcp_routines.aa_connect(newtcp.socket, (struct sockaddr *)&newtcp.sas,
								 newtcp.ai.ai_addrlen);
				if ((temp_1 < 0) && (ECONNREFUSED != errno) && (EINTR != errno))
				{
					(void)tcp_routines.aa_close(newtcp.socket);
					errptr = (char *)STRERROR(errno);
					errlen = STRLEN(errptr);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_OPENCONN, 0, ERR_TEXT, 2, errlen, errptr);
					return FALSE;
				}
				if ((temp_1 < 0) && (EINTR == errno))
				{
					(void)tcp_routines.aa_close(newtcp.socket);
					return FALSE;
				}
				if ((temp_1 < 0) && (NO_M_TIMEOUT != timeout))
				{
					sys_get_curr_time(&cur_time);
					cur_time = sub_abs_time(&end_time, &cur_time);
					if (cur_time.at_sec <= 0)
						no_time_left = TRUE;
				}
				SHORT_SLEEP(100);		/* Sleep for 100 ms */
			}
			while ((TRUE != no_time_left) && (temp_1 < 0));
			if (temp_1 < 0) /* out of time */
			{
				tcp_routines.aa_close(newtcp.socket);
				return FALSE;
			}
		}
		memcpy(ioptr->dev_sp, &newtcp, SIZEOF(d_tcp_struct));
		ioptr->state = dev_open;
	}
#ifdef	DEBUG_TCP
	PRINTF("%s (%d) <<<\n", __FILE__, tcpptr->socket);
#endif
	return TRUE;
}
