/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
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
#include "have_crit.h"
#include "deferred_events_queue.h"
#include "gtm_netdb.h"
#include "gtm_ipv6.h"
#include "gtm_unistd.h"
#include "gtm_select.h"
#include "min_max.h"

#define	ESTABLISHED	"ESTABLISHED"

GBLREF	d_socket_struct		*newdsocket;	/* in case jobinterrupt */
GBLREF	int			socketus_interruptus;
GBLREF	int4			gtm_max_sockets;
GBLREF	mv_stent		*mv_chain;
GBLREF	stack_frame		*frame_pointer;
GBLREF	unsigned char		*stackbase, *stacktop, *msp, *stackwarn;
GBLREF	volatile int4		outofband;
GBLREF	volatile boolean_t	dollar_zininterrupt;

error_def(ERR_GETNAMEINFO);
error_def(ERR_GETSOCKNAMERR);
error_def(ERR_GETSOCKOPTERR);
error_def(ERR_OPENCONN);
error_def(ERR_SETSOCKOPTERR);
error_def(ERR_SOCKINIT);
error_def(ERR_TEXT);
error_def(ERR_ZINTRECURSEIO);

boolean_t iosocket_connect(socket_struct *sockptr, int4 msec_timeout, boolean_t update_bufsiz)
{
	int		temp_1, keepalive_opt;
	char		*errptr;
	int4		errlen, last_errno, save_errno;
	int		d_socket_struct_len, res, nfds, sockerror;
	fd_set		writefds;
	boolean_t	no_time_left = FALSE, first_time = TRUE, timer_started = FALSE, oneshot;
	boolean_t	timer_done;	/* for TIMEOUT_INIT */
	boolean_t	need_connect, need_socket, need_select;
	short		len;
	io_desc		*iod;
	d_socket_struct *dsocketptr, *real_dsocketptr;
	socket_interrupt *sockintr, *real_sockintr;
	ABS_TIME	cur_time, end_time;
	struct timeval	*sel_time;
	mv_stent	*mv_zintdev;
	struct addrinfo *remote_ai_ptr, *raw_ai_ptr, *local_ai_ptr;
	intrpt_state_t	prev_intrpt_state;
	int		errcode, real_errno;
	char		ipaddr[SA_MAXLEN + 1];
	char		port_buffer[NI_MAXSERV];
	static readonly char 	action[] = "CONNECT";
	GTM_SOCKLEN_TYPE	sockbuflen, tmp_addrlen;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
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
	oneshot = (0 == msec_timeout);
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
					DEFER_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
					freeaddrinfo(sockptr->remote.ai_head);
					ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
					sockptr->remote.ai_head = NULL;
				}
				SOCKET_FREE(sockptr);
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
			first_time = TRUE;	/* for initial connect */
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
						DEFER_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
						freeaddrinfo(sockptr->remote.ai_head);
						ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
						sockptr->remote.ai_head = NULL;
					}
					SOCKET_FREE(sockptr);
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
						DEFER_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
						freeaddrinfo(sockptr->remote.ai_head);
						ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
						sockptr->remote.ai_head = NULL;
					}
					SOCKET_FREE(sockptr);
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
							DEFER_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
							freeaddrinfo(sockptr->remote.ai_head);
							ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
							sockptr->remote.ai_head = NULL;
						}
						SOCKET_FREE(sockptr);
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
							RTS_ERROR_LITERAL("SO_RCVBUF"), save_errno, errlen, errptr);
						return FALSE;
					}
					sockptr->options_state.rcvbuf |= SOCKOPTIONS_USER;
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
							DEFER_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
							freeaddrinfo(sockptr->remote.ai_head);
							ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
							sockptr->remote.ai_head = NULL;
						}
						SOCKET_FREE(sockptr);
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_GETSOCKOPTERR, 5,
							RTS_ERROR_LITERAL("SO_RCVBUF"), save_errno, errlen, errptr);
						return FALSE;
					}
					sockptr->options_state.rcvbuf |= SOCKOPTIONS_SYSTEM;
				}
				if (0 != (SOCKOPTIONS_PENDING & sockptr->options_state.sndbuf))
				{
					if (-1 == iosocket_setsockopt(sockptr, "SO_SNDBUF", SO_SNDBUF, SOL_SOCKET,
						&sockptr->iobfsize, sizeof(sockptr->iobfsize), TRUE))
						return FALSE;
					sockptr->options_state.sndbuf |= SOCKOPTIONS_USER;
					sockptr->options_state.sndbuf &= ~SOCKOPTIONS_PENDING;
				}
			}
		}
		save_errno = res = 0;
		if (need_connect)
		{
			/* Use plain connect to allow jobinterrupt */
			assert(FD_INVALID != sockptr->sd);
			if (first_time && (NO_M_TIMEOUT != msec_timeout))
			{	/* start timer if not end_time, if timeout is 0 just a short wait */
				DBGSOCK((stdout, "socconn: if first_time: msec_timeout = %d\n", msec_timeout));
				sys_get_curr_time(&cur_time);
				cur_time = sub_abs_time(&end_time, &cur_time);	/* calculate remaining time */
				if (0 != msec_timeout)
					msec_timeout = (int4)((cur_time.at_sec * MILLISECS_IN_SEC) +
					/* Round up in order to prevent premature timeouts */
						DIVIDE_ROUND_UP(cur_time.at_usec, MICROSECS_IN_MSEC));
				if (0 >= msec_timeout)
				{
					no_time_left = TRUE;
					msec_timeout = 100 /* enough time to connect if no problems */;
					DBGSOCK((stdout, "socconn: first_time msec_timeout was 0, now = %d\n", msec_timeout));
				}
				first_time = FALSE;
				timer_started = TRUE;
				TIMEOUT_INIT(timer_done, msec_timeout);
			 }
			res = connect(sockptr->sd, SOCKET_REMOTE_ADDR(sockptr), remote_ai_ptr->ai_addrlen);	 /* BYPASSOK */
			if (res < 0)
			{
				save_errno = errno;
				need_connect = TRUE;
				switch (save_errno)
				{
					case EISCONN:
						save_errno = 0;
						res = 0;	/* since it is connected already, treat as if success */
						need_connect = FALSE;
						break;
					case EINTR:
						DBGSOCK((stdout, "socconn: EINTR: outofband = %d, oneshot = %d\n",
							outofband, oneshot));
						if (outofband && !oneshot)
						{	/* handle outofband unless zero timeout */
							save_errno = 0;
							need_socket = need_connect = FALSE;
							break;
						} /* else fall through */
					case EINPROGRESS:
					case EALREADY:
						need_socket = need_connect = FALSE;
						if (!oneshot)
						{
							DBGSOCK((stdout, "socconn: EINPROGESS: errno = %d\n", save_errno));
							need_select = TRUE;
						}
					/* fall through */
					case ETIMEDOUT:	/* the other side bound but not listening */
					case ECONNREFUSED:
					case ENOENT:    /* LOCAL socket not there */
						if (!no_time_left && (!oneshot) && (NO_M_TIMEOUT != msec_timeout))
						{
							sys_get_curr_time(&cur_time);
							cur_time = sub_abs_time(&end_time, &cur_time);
							DBGSOCK((stdout, "socconn: ENOENT: errno = %d, msec_timeout= %d\n",
								save_errno, msec_timeout));
							msec_timeout = (int4)(cur_time.at_sec * MILLISECS_IN_SEC +
								/* Round up in order to prevent premature timeouts */
								DIVIDE_ROUND_UP(cur_time.at_usec, MICROSECS_IN_MSEC));
							if (0 >= msec_timeout)
								msec_timeout = 0;
						}
						if (oneshot || (0 == msec_timeout))
							no_time_left = TRUE;
						else if (!no_time_left)
						{
							if (ETIMEDOUT == save_errno || ECONNREFUSED == save_errno
								|| ENOENT == save_errno)
								need_connect = need_socket = TRUE;
							else if (socket_local == sockptr->protocol)
							{	/* AF_UNIX sockets do not continue the connect in background */
								need_connect = TRUE;
								need_socket = need_select = FALSE;
							}
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
		if (timer_started)
		{
			timer_started = FALSE;
			TIMEOUT_DONE(timer_done);
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
								|| ENOENT == sockerror)
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
				DEFER_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
				freeaddrinfo(sockptr->remote.ai_head);
				ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
				sockptr->remote.ai_head = NULL;
			}
			errptr = (char *)STRERROR(save_errno);
			if (dev_open == iod->state)
			{
				iod->dollar.za = ZA_IO_ERR;
				SET_DOLLARDEVICE_ONECOMMA_ERRSTR(dsocketptr->iod, errptr, errlen);
			}
			if (sockptr->ioerror)
			{
				SOCKET_FREE(sockptr);
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_OPENCONN, 0, ERR_TEXT, 2, errlen, errptr);
			}
			errno = save_errno;
			return FALSE;
		}
		if (no_time_left)
		{
			if (NULL != sockptr->remote.ai_head)
			{
				DEFER_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
				freeaddrinfo(sockptr->remote.ai_head);
				ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
				sockptr->remote.ai_head = NULL;
			}
			if (!need_socket)
			{
				close(sockptr->sd);
				sockptr->sd = FD_INVALID;
			}
			SOCKET_FREE(sockptr);
			return FALSE;
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
				if (NULL != sockptr->remote.ai_head)
				{
					DEFER_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
					freeaddrinfo(sockptr->remote.ai_head);
					ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
					sockptr->remote.ai_head = NULL;
				}
				SOCKET_FREE(sockptr);
				async_action(FALSE);
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
			async_action(FALSE);
			assertpro(FALSE);      /* Should *never* return from async_action */
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
		DEFER_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
		freeaddrinfo(sockptr->remote.ai_head);
		ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
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
	if ((SOCKOPTIONS_PENDING & sockptr->options_state.alive)
		|| (SOCKOPTIONS_PENDING & sockptr->options_state.cnt)
		|| (SOCKOPTIONS_PENDING & sockptr->options_state.intvl))
		keepalive_opt = SOCKOPTIONS_FROM_STRUCT;
	else
		keepalive_opt = TREF(gtm_socket_keepalive_idle);	/* deviceparameter takes precedence */
	if (keepalive_opt && !iosocket_tcp_keepalive(sockptr, keepalive_opt, action, TRUE))
		return FALSE;		/* iosocket_tcp_keepalive issues rts_error rather than return */
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
