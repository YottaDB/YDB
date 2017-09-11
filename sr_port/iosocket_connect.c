/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* iosocket_connect.c */
#include "mdef.h"
#include <errno.h>
#include "gtm_time.h"
#include "gtm_socket.h"
#include "gtm_inet.h"
#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "stringpool.h"
#include "gt_timer.h"
#include "io.h"
#include "iotimer.h"
#include "iosocketdef.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "mv_stent.h"
#include "outofband.h"
#include "gtm_netdb.h"
#include "gtm_ipv6.h"
#include "gtm_unistd.h"
#include "gtm_select.h"

#define	ESTABLISHED	"ESTABLISHED"

GBLREF	boolean_t		dollar_zininterrupt;
GBLREF	d_socket_struct		*newdsocket;	/* in case jobinterrupt */
GBLREF	int			socketus_interruptus;
GBLREF	int4			gtm_max_sockets;
GBLREF	mv_stent		*mv_chain;
GBLREF	stack_frame		*frame_pointer;
GBLREF	unsigned char		*stackbase, *stacktop, *msp, *stackwarn;
GBLREF	volatile int4		outofband;

error_def(ERR_GETNAMEINFO);
error_def(ERR_GETSOCKNAMERR);
error_def(ERR_GETSOCKOPTERR);
error_def(ERR_OPENCONN);
error_def(ERR_SETSOCKOPTERR);
error_def(ERR_SOCKINIT);
error_def(ERR_STACKCRIT);
error_def(ERR_STACKOFLOW);
error_def(ERR_TEXT);
error_def(ERR_ZINTRECURSEIO);

boolean_t iosocket_connect(socket_struct *sockptr, int4 msec_timeout, boolean_t update_bufsiz)
{
	int		temp_1;
	char		*errptr;
	int4		errlen, last_errno, save_errno;
	int		d_socket_struct_len, res, nfds, sockerror;
	fd_set		writefds;
	boolean_t	no_time_left = FALSE;
	boolean_t	need_connect, need_socket, need_select;
	short		len;
	io_desc		*iod;
	d_socket_struct *dsocketptr, *real_dsocketptr;
	socket_interrupt *sockintr, *real_sockintr;
	ABS_TIME	cur_time, end_time;
	struct timeval	*sel_time;
	mv_stent	*mv_zintdev;
	struct addrinfo *remote_ai_ptr, *raw_ai_ptr, *local_ai_ptr;
	int		errcode, real_errno;
	char		ipaddr[SA_MAXLEN + 1];
	char		port_buffer[NI_MAXSERV];
	GTM_SOCKLEN_TYPE	sockbuflen, tmp_addrlen;

	DBGSOCK((stdout, "socconn: ************* Entering socconn - msec_timeout: %d\n",msec_timeout));
	/* check for validity */
	dsocketptr = sockptr->dev;
	assert(NULL != dsocketptr);
	sockintr = &dsocketptr->sock_save_state;
	iod = dsocketptr->iod;
	real_dsocketptr = (d_socket_struct *)iod->dev_sp;	/* not newdsocket which is not saved on error */
	real_sockintr = &real_dsocketptr->sock_save_state;
	iod->dollar.key[0] = '\0';
	need_socket = need_connect = TRUE;
	need_select = FALSE;
	/* Check for restart */
	if (dsocketptr->mupintr)
	{       /* We have a pending read restart of some sort - check we aren't recursing on this device */
		assertpro(sockwhich_invalid != sockintr->who_saved);	/* Interrupt should never have an invalid save state */
		if (dollar_zininterrupt)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZINTRECURSEIO);
		assertpro(sockwhich_connect == sockintr->who_saved);	/* ZINTRECURSEIO should have caught */
		DBGSOCK((stdout, "socconn: *#*#*#*#*#*#*#  Restarted interrupted connect\n"));
		mv_zintdev = io_find_mvstent(iod, FALSE);
		if (mv_zintdev)
		{
			if (sockintr->end_time_valid)
				/* Restore end_time for timeout */
				end_time = sockintr->end_time;
			if ((socket_connect_inprogress == sockptr->state) && (FD_INVALID != sockptr->sd))
			{
				need_select = TRUE;
				need_socket = need_connect = FALSE;	/* sd still good */
			}
			/* Done with this mv_stent. Pop it off if we can, else mark it inactive. */
			if (mv_chain == mv_zintdev)
				POP_MV_STENT();	 /* pop if top of stack */
			else
			{       /* else mark it unused, see iosocket_open for use */
				mv_zintdev->mv_st_cont.mvs_zintdev.buffer_valid = FALSE;
				mv_zintdev->mv_st_cont.mvs_zintdev.io_ptr = NULL;
			}
			DBGSOCK((stdout, "socconn: mv_stent found - endtime: %d/%d\n", end_time.at_sec, end_time.at_usec));
		} else
			DBGSOCK((stdout, "socconn: no mv_stent found !!\n"));
		real_dsocketptr->mupintr = dsocketptr->mupintr = FALSE;
		real_sockintr->who_saved = sockintr->who_saved = sockwhich_invalid;
	} else if (NO_M_TIMEOUT != msec_timeout)
	{
		sys_get_curr_time(&cur_time);
		add_int_to_abs_time(&cur_time, msec_timeout, &end_time);
	}
	real_sockintr->end_time_valid = sockintr->end_time_valid = FALSE;
	last_errno = 0;

	remote_ai_ptr = (struct addrinfo*)(&(sockptr->remote.ai));
	do
	{
		/* If the connect was failed, we may have already changed the remote.
	 	 * So, the remote ai_addr should be restored.
	 	 */
		assertpro((socket_tcpip != sockptr->protocol) || (NULL != sockptr->remote.ai_head));
		if (sockptr->remote.ai_head)
			memcpy(remote_ai_ptr, sockptr->remote.ai_head, SIZEOF(struct addrinfo));
		if (need_socket && (FD_INVALID != sockptr->sd))
		{
			close(sockptr->sd);
			sockptr->sd = FD_INVALID;
		}
		assert(FD_INVALID == -1);
		if (need_socket)
		{
			real_errno = -2;
			for (raw_ai_ptr = remote_ai_ptr; NULL != raw_ai_ptr; raw_ai_ptr = raw_ai_ptr->ai_next)
			{
				if (-1 == (sockptr->sd = socket(raw_ai_ptr->ai_family, raw_ai_ptr->ai_socktype,
										  raw_ai_ptr->ai_protocol)))
				{
					real_errno = errno;
					if (socket_local == sockptr->protocol)
						break;		/* just one attempt */
				} else
				{
					real_errno = 0;
					break;
				}
			}
			if (0 != real_errno)
			{
				if (NULL != sockptr->remote.ai_head)
				{
					freeaddrinfo(sockptr->remote.ai_head);
					sockptr->remote.ai_head = NULL;
				}
				assertpro(-2 != real_errno);
				errptr = (char *)STRERROR(real_errno);
				errlen = STRLEN(errptr);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_SOCKINIT, 3, errno, errlen, errptr);
				return FALSE;
			}
			assertpro(NULL != raw_ai_ptr);	/* there is a local, IPv6, or IPv4 address */
			if (remote_ai_ptr != raw_ai_ptr)
				memcpy(remote_ai_ptr, raw_ai_ptr, SIZEOF(struct addrinfo));
			need_socket = FALSE;
			temp_1 = 1;
			if (socket_local != sockptr->protocol)
			{
				SOCKET_AI_TO_REMOTE_ADDR(sockptr, raw_ai_ptr);
				remote_ai_ptr->ai_addr = SOCKET_REMOTE_ADDR(sockptr);
				remote_ai_ptr->ai_addrlen = raw_ai_ptr->ai_addrlen;
				remote_ai_ptr->ai_next = NULL;
				if (-1 == setsockopt(sockptr->sd, SOL_SOCKET, SO_REUSEADDR,
					&temp_1, SIZEOF(temp_1)))
				{
					save_errno = errno;
					errptr = (char *)STRERROR(save_errno);
					errlen = STRLEN(errptr);
					close(sockptr->sd);	/* Don't leave a dangling socket around */
					sockptr->sd = FD_INVALID;
					if (NULL != sockptr->remote.ai_head)
					{
						freeaddrinfo(sockptr->remote.ai_head);
						sockptr->remote.ai_head = NULL;
					}
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
						  RTS_ERROR_LITERAL("SO_REUSEADDR"), save_errno, errlen, errptr);
					return FALSE;
				}
#				ifdef TCP_NODELAY
				temp_1 = sockptr->nodelay ? 1 : 0;
				if (-1 == setsockopt(sockptr->sd,
					IPPROTO_TCP, TCP_NODELAY, &temp_1, SIZEOF(temp_1)))
				{
					save_errno = errno;
					errptr = (char *)STRERROR(save_errno);
					errlen = STRLEN(errptr);
					close(sockptr->sd);	/* Don't leave a dangling socket around */
					sockptr->sd = FD_INVALID;
					if (NULL != sockptr->remote.ai_head)
					{
						freeaddrinfo(sockptr->remote.ai_head);
						sockptr->remote.ai_head = NULL;
					}
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
						RTS_ERROR_LITERAL("TCP_NODELAY"), save_errno, errlen, errptr);
					return FALSE;
				}
#				endif
				if (update_bufsiz)
				{
					if (-1 == setsockopt(sockptr->sd,
						     SOL_SOCKET, SO_RCVBUF, &sockptr->bufsiz, SIZEOF(sockptr->bufsiz)))
					{
						save_errno = errno;
						errptr = (char *)STRERROR(save_errno);
						errlen = STRLEN(errptr);
						close(sockptr->sd);	/* Don't leave a dangling socket around */
						sockptr->sd = FD_INVALID;
						if (NULL != sockptr->remote.ai_head)
						{
							freeaddrinfo(sockptr->remote.ai_head);
							sockptr->remote.ai_head = NULL;
						}
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
							RTS_ERROR_LITERAL("SO_RCVBUF"), save_errno, errlen, errptr);
						return FALSE;
					}
				} else
				{
					sockbuflen = SIZEOF(sockptr->bufsiz);
					if (-1 == getsockopt(sockptr->sd,
						     SOL_SOCKET, SO_RCVBUF, &sockptr->bufsiz, &sockbuflen))
					{
						save_errno = errno;
						errptr = (char *)STRERROR(save_errno);
						errlen = STRLEN(errptr);
						close(sockptr->sd);	/* Don't leave a dangling socket around */
						sockptr->sd = FD_INVALID;
						if (NULL != sockptr->remote.ai_head)
						{
							freeaddrinfo(sockptr->remote.ai_head);
							sockptr->remote.ai_head = NULL;
						}
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_GETSOCKOPTERR, 5,
							RTS_ERROR_LITERAL("SO_RCVBUF"), save_errno, errlen, errptr);
						return FALSE;
					}
				}
			}
		}
		save_errno = res = 0;
		if (need_connect)
		{
			/* Use plain connect to allow jobinterrupt */
			assert(FD_INVALID != sockptr->sd);
			res = connect(sockptr->sd, SOCKET_REMOTE_ADDR(sockptr), remote_ai_ptr->ai_addrlen);	 /* BYPASSOK */
			if (res < 0)
			{
				save_errno = errno;
				no_time_left = FALSE;
				need_connect = TRUE;
				switch (save_errno)
				{
					case EISCONN:
						save_errno = 0;
						res = 0;	/* since it is connected already, treat as if success */
						need_connect = FALSE;
						break;
					case EINTR:
						if (outofband && 0 != msec_timeout)
						{	/* handle outofband unless zero timeout */
							save_errno = 0;
							need_socket = need_connect = FALSE;
							break;
						} /* else fall through */
					case EINPROGRESS:
					case EALREADY:
#					if (defined(__osf__) && defined(__alpha)) || defined(__sun) || defined(__vms)
					case EWOULDBLOCK:
#					endif
						need_socket = need_connect = FALSE;
						if (0 != msec_timeout)
							need_select = TRUE;
					/* fall through */
					case ETIMEDOUT:	/* the other side bound but not listening */
					case ECONNREFUSED:
#					ifndef VMS
					case ENOENT:	/* LOCAL socket not there */
#					endif
						if (!no_time_left && (0 != msec_timeout) && (NO_M_TIMEOUT != msec_timeout))
						{
							sys_get_curr_time(&cur_time);
							cur_time = sub_abs_time(&end_time, &cur_time);
							msec_timeout = (int4)(cur_time.at_sec * MILLISECS_IN_SEC +
								/* Round up in order to prevent premature timeouts */
								DIVIDE_ROUND_UP(cur_time.at_usec, MICROSECS_IN_MSEC));
							if (0 >= msec_timeout)
								msec_timeout = 0;
						}
						if (0 == msec_timeout)
							no_time_left = TRUE;
						else if (!no_time_left)
						{
							if (ETIMEDOUT == save_errno || ECONNREFUSED == save_errno
								UNIX_ONLY(|| ENOENT == save_errno))
								need_connect = need_socket = TRUE;
							save_errno = 0;
							res = -1;	/* do the outer loop again */
						}
						if (no_time_left)
							save_errno = 0;
						break;
					default:
						break;
				}
			} /* if connect failed */
		}
		if (need_select)
		{
			sockerror = 0;
			do
			{ /* unless outofband loop on select if connection continuing */
				if (NO_M_TIMEOUT == msec_timeout)
					sel_time = NULL;
				else
				{
					sys_get_curr_time(&cur_time);
					cur_time = sub_abs_time(&end_time, &cur_time);
					msec_timeout = (int4)(cur_time.at_sec * MILLISECS_IN_SEC +
						/* Round up in order to prevent premature timeouts */
						DIVIDE_ROUND_UP(cur_time.at_usec, MICROSECS_IN_MSEC));
					if (0 < msec_timeout)
						sel_time = (struct timeval *)&cur_time;
					else
					{	/* timed out so done */
						save_errno = res = 0;
						no_time_left = TRUE;
						break;
					}
				}
				assert(FD_INVALID != sockptr->sd);
				assertpro(FD_SETSIZE > sockptr->sd);
				FD_ZERO(&writefds);
				FD_SET(sockptr->sd, &writefds);
				res = select(sockptr->sd + 1, NULL, &writefds, NULL, sel_time);
				if (0 < res)
				{	/* check for socket error */
					sockbuflen = SIZEOF(sockerror);
					res = getsockopt(sockptr->sd, SOL_SOCKET, SO_ERROR,
							&sockerror, &sockbuflen);
					if (0 == res && 0 == sockerror)
					{	/* got it */
						save_errno = 0;
						break;
					} else if (0 == res && 0 != sockerror)
					{
						if (EINTR == sockerror)
						{ /* loop on select */
							save_errno = 0;
							continue;
						} else
						{	/* return socket error */
							if (ECONNREFUSED == sockerror || ETIMEDOUT == sockerror
								UNIX_ONLY(|| ENOENT == sockerror))
							{	/* try until timeout */
								last_errno = sockerror;
								save_errno = 0;
								need_socket = need_connect = TRUE;
								need_select = FALSE;
								res = -1;
							} else
								save_errno = sockerror;
							break;
						}
					} else
					{
						save_errno = errno; /* error on getsockopt */
						break;
					}
				} else if (0 == res)
				{ /* select timed out */
					save_errno = 0;
					no_time_left = TRUE;
					break;
				} else if (EINTR != errno)
				{
					save_errno = errno;
					break;
				} else if (outofband)
				{
					save_errno = 0;
					break;
				}
			} while (TRUE); /* do select */
		}
		if (save_errno)
		{
			if (FD_INVALID != sockptr->sd)
			{
				close(sockptr->sd);	/* Don't leave a dangling socket around */
				sockptr->sd = FD_INVALID;
			}
			if (NULL != sockptr->remote.ai_head)
			{
				freeaddrinfo(sockptr->remote.ai_head);
				sockptr->remote.ai_head = NULL;
			}
			errptr = (char *)STRERROR(save_errno);
			errlen = STRLEN(errptr);
			if (dev_open == iod->state)
			{
				iod->dollar.za = 9;
				memcpy(iod->dollar.device, ONE_COMMA, SIZEOF(ONE_COMMA));
				memcpy(&iod->dollar.device[SIZEOF(ONE_COMMA) - 1],
						errptr, errlen + 1); /* + 1 for null */
			}
			if (sockptr->ioerror)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_OPENCONN, 0, ERR_TEXT, 2, errlen, errptr);
			errno = save_errno;
			return FALSE;
		}
		if (no_time_left)
		{
			if (NULL != sockptr->remote.ai_head)
			{
				freeaddrinfo(sockptr->remote.ai_head);
				sockptr->remote.ai_head = NULL;
			}
			return FALSE;	/* caller will close socket */
		}
		if (res < 0 && outofband)	/* if connected delay outofband */
		{
			DBGSOCK((stdout, "socconn: outofband interrupt received (%d) -- "
				 "queueing mv_stent for wait intr\n", outofband));
			if (!OUTOFBAND_RESTARTABLE(outofband))
			{	/* the operation would not be resumed, no need to save socket device states */
				if (!need_socket)
				{
					close(sockptr->sd);
					sockptr->sd = FD_INVALID;
				}
				outofband_action(FALSE);
				assertpro(FALSE);
			}
			if (need_connect)
			{	/* no connect in progress */
				close(sockptr->sd);	/* Don't leave a dangling socket around */
				sockptr->sd = FD_INVALID;
				sockptr->state = socket_created;
			} else
				sockptr->state = socket_connect_inprogress;
			real_sockintr->who_saved = sockintr->who_saved = sockwhich_connect;
			if (NO_M_TIMEOUT != msec_timeout)
			{
				real_sockintr->end_time = sockintr->end_time = end_time;
				real_sockintr->end_time_valid  = sockintr->end_time_valid = TRUE;
			} else
				real_sockintr->end_time_valid = sockintr->end_time_valid = FALSE;
			real_sockintr->newdsocket = sockintr->newdsocket = newdsocket;
			real_dsocketptr->mupintr = dsocketptr->mupintr = TRUE;
			d_socket_struct_len = SIZEOF(d_socket_struct) +
						(SIZEOF(socket_struct) * (gtm_max_sockets - 1));
			ENSURE_STP_FREE_SPACE(d_socket_struct_len);
			PUSH_MV_STENT(MVST_ZINTDEV);
			mv_chain->mv_st_cont.mvs_zintdev.buffer_valid = FALSE;
			mv_chain->mv_st_cont.mvs_zintdev.io_ptr = NULL;
			mv_chain->mv_st_cont.mvs_zintdev.socketptr = sockptr;	/* for sd and to free structure */
			mv_chain->mv_st_cont.mvs_zintdev.curr_sp_buffer.len = d_socket_struct_len;
			mv_chain->mv_st_cont.mvs_zintdev.curr_sp_buffer.addr = (char *)stringpool.free;
			memcpy (stringpool.free, (unsigned char *)newdsocket, d_socket_struct_len);
			stringpool.free += d_socket_struct_len;
			mv_chain->mv_st_cont.mvs_zintdev.io_ptr = iod;
			mv_chain->mv_st_cont.mvs_zintdev.buffer_valid = TRUE;
			socketus_interruptus++;
			DBGSOCK((stdout, "socconn: mv_stent queued - endtime: %d/%d  interrupts: %d\n",
				 end_time.at_sec, end_time.at_usec, socketus_interruptus));
			outofband_action(FALSE);
			assertpro(FALSE);      /* Should *never* return from outofband_action */
			return FALSE;   /* For the compiler.. */
		}
		hiber_start(100);
	} while (res < 0);

	sockptr->state = socket_connected;
	sockptr->first_read = sockptr->first_write = TRUE;
	/* update dollar_key */
	len = SIZEOF(ESTABLISHED) - 1;
	memcpy(&iod->dollar.key[0], ESTABLISHED, len);
	iod->dollar.key[len++] = '|';
	memcpy(&iod->dollar.key[len], sockptr->handle, sockptr->handle_len);
	len += sockptr->handle_len;
	iod->dollar.key[len++] = '|';
	/* translate internal address to numeric ip address */
	assert(FALSE == need_socket);
	if (NULL != sockptr->remote.ai_head)
	{
		freeaddrinfo(sockptr->remote.ai_head);
		sockptr->remote.ai_head = NULL;
	}
	local_ai_ptr = &(sockptr->local.ai);
	if (socket_local != sockptr->protocol)
	{
		tmp_addrlen = SIZEOF(struct sockaddr_storage);
		if (-1 == getsockname(sockptr->sd, SOCKET_LOCAL_ADDR(sockptr), &tmp_addrlen))
		{
			save_errno = errno;
			errptr = (char *)STRERROR(save_errno);
			errlen = STRLEN(errptr);
			if (FD_INVALID != sockptr->sd)
			{
				close(sockptr->sd);	/* Don't leave a dangling socket around */
				sockptr->sd = FD_INVALID;
			}
			SOCKET_FREE(sockptr);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_GETSOCKNAMERR, 3, save_errno, errlen, errptr);
			return FALSE;
		}
		local_ai_ptr->ai_addrlen = tmp_addrlen;
		local_ai_ptr->ai_family = SOCKET_LOCAL_ADDR(sockptr)->sa_family;
		GETNAMEINFO(SOCKET_LOCAL_ADDR(sockptr), local_ai_ptr->ai_addrlen, ipaddr, SA_MAXLEN, port_buffer,
			NI_MAXSERV, NI_NUMERICSERV | NI_NUMERICHOST, errcode);
		if (0 != errcode)
		{
			if (FD_INVALID != sockptr->sd)
			{
				close(sockptr->sd);	/* Don't leave a dangling socket around */
				sockptr->sd = FD_INVALID;
			}
			SOCKET_FREE(sockptr);
			RTS_ERROR_ADDRINFO(NULL, ERR_GETNAMEINFO, errcode);
			return FALSE;
		}
		sockptr->local.port = ATOI(port_buffer);
		STRNDUP(ipaddr, SA_MAXLEN, sockptr->local.saddr_ip);
		GETNAMEINFO(SOCKET_REMOTE_ADDR(sockptr), remote_ai_ptr->ai_addrlen, ipaddr, SA_MAXLEN, NULL, 0
			, NI_NUMERICHOST, errcode);
		if (0 != errcode)
		{
			if (FD_INVALID != sockptr->sd)
			{
				close(sockptr->sd);	/* Don't leave a dangling socket around */
				sockptr->sd = FD_INVALID;
			}
			SOCKET_FREE(sockptr);
			RTS_ERROR_ADDRINFO(NULL, ERR_GETNAMEINFO, errcode);
			return FALSE;
		}
		STRNDUP(ipaddr, SA_MAXLEN, sockptr->remote.saddr_ip);
		strncpy(&iod->dollar.key[len], sockptr->remote.saddr_ip, DD_BUFLEN - 1 - len);
	} else
	{	/* getsockname does not return info for AF_UNIX connected socket so copy from remote side */
		local_ai_ptr->ai_socktype = sockptr->remote.ai.ai_socktype;
		local_ai_ptr->ai_addrlen = sockptr->remote.ai.ai_addrlen;
		local_ai_ptr->ai_protocol = sockptr->remote.ai.ai_protocol;
		SOCKET_ADDR_COPY(sockptr->local, sockptr->remote.sa, sockptr->remote.ai.ai_addrlen);
		STRNCPY_STR(&iod->dollar.key[len], ((struct sockaddr_un *)(sockptr->remote.sa))->sun_path, DD_BUFLEN - len - 1);
	}
	iod->dollar.key[DD_BUFLEN - 1] = '\0';			/* In case we fill the buffer */
	return TRUE;
}
