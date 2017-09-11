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
#include "gtm_unistd.h"
#ifdef USE_POLL
#include <sys/poll.h>
#endif
#ifdef USE_SELECT
#include "gtm_select.h"
#endif
#include "io_params.h"
#include "gt_timer.h"
#include "io.h"
#include "iotimer.h"
#include "iosocketdef.h"
#include "min_max.h"
#include "outofband.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "mv_stent.h"
#include "gtm_netdb.h"
#include "gtm_stdlib.h"
#include "eintr_wrappers.h"
#include "error.h"

#define	CONNECTED	"CONNECT"
#define READ	"READ"

GBLREF volatile int4		outofband;
GBLREF int4			gtm_max_sockets;
GBLREF int			socketus_interruptus;
GBLREF boolean_t		dollar_zininterrupt;
GBLREF stack_frame  	        *frame_pointer;
GBLREF unsigned char            *stackbase, *stacktop, *msp, *stackwarn;
GBLREF mv_stent			*mv_chain;
GBLREF int			dollar_truth;

error_def(ERR_GETNAMEINFO);
error_def(ERR_GETSOCKNAMERR);
error_def(ERR_SOCKACPT);
error_def(ERR_SOCKWAIT);
error_def(ERR_TEXT);
error_def(ERR_SOCKMAX);
error_def(ERR_ZINTRECURSEIO);
error_def(ERR_STACKCRIT);
error_def(ERR_STACKOFLOW);

boolean_t iosocket_wait(io_desc *iod, int4 msec_timeout)
{
	struct 	timeval  	utimeout, *utimeoutptr;
	ABS_TIME		cur_time, end_time;
#ifdef USE_POLL
	nfds_t			poll_nfds;
	struct pollfd		*poll_fds;
	socket_struct		**poll_socketptr;	/* matching poll_fds */
	size_t			poll_fds_size;
	int			poll_timeout, poll_fd;
#endif
#ifdef USE_SELECT
	int			select_max_fd;
	fd_set			select_fdset;
#endif
	d_socket_struct 	*dsocketptr;
	socket_struct   	*socketptr;
	socket_interrupt	*sockintr;
	char            	*errptr, *charptr;
	int4            	errlen, ii, jj;
	int4			nselect, nlisten, nconnected, rlisten, rconnected;
	int4			oldestconnectedcycle, oldestconnectedindex;
	int4			oldestlistencycle, oldestlistenindex;
	int4			oldesteventcycle, oldesteventindex;
	int			rv, max_fd, len;
	boolean_t		zint_restart, retry_accept = FALSE;
	mv_stent		*mv_zintdev;
	int			errcode;
	boolean_t		ch_set;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* check for validity */
	assert(gtmsocket == iod->type);
	dsocketptr = (d_socket_struct *)iod->dev_sp;
	sockintr = &dsocketptr->sock_save_state;
	ESTABLISH_RET_GTMIO_CH(&iod->pair, FALSE, ch_set);

	/* Check for restart */
	if (!dsocketptr->mupintr)
	{
		/* Simple path, no worries*/
		zint_restart = FALSE;
		dsocketptr->waitcycle++;	/* don't count restarts */
		if (0 == dsocketptr->waitcycle)
		{	/* wrapped so make it non zero */
			dsocketptr->waitcycle++;
		}
	} else
	{       /* We have a pending wait restart of some sort - check we aren't recursing on this device */
		assertpro(sockwhich_invalid != sockintr->who_saved);	/* Interrupt should never have an invalid save state */
		if (dollar_zininterrupt)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZINTRECURSEIO);
		assertpro(sockwhich_wait == sockintr->who_saved);      /* ZINTRECURSEIO should have caught */
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
#ifdef USE_POLL
	poll_fds_size = dsocketptr->n_socket * (SIZEOF(struct pollfd) + SIZEOF(socket_struct *));
	if (NULL == TREF(poll_fds_buffer))
	{
		TREF(poll_fds_buffer) = malloc(poll_fds_size);
		TREF(poll_fds_buffer_size) = poll_fds_size;
	} else if (poll_fds_size > TREF(poll_fds_buffer_size))
	{
		free(TREF(poll_fds_buffer));
		TREF(poll_fds_buffer) = malloc(poll_fds_size);
		TREF(poll_fds_buffer_size) = poll_fds_size;
	}
	poll_fds = (struct pollfd *) TREF(poll_fds_buffer);
	poll_socketptr = (socket_struct **)((char *)poll_fds + (dsocketptr->n_socket * SIZEOF(struct pollfd)));
#endif
	SELECT_ONLY(FD_ZERO(&select_fdset));
	while (TRUE)
	{
		POLL_ONLY(poll_nfds = 0);
		SELECT_ONLY(select_max_fd = 0);
		nselect = nlisten = nconnected = rlisten = rconnected = 0;
		rv = 0;
		for (ii = 0; ii < dsocketptr->n_socket; ii++)
		{
			socketptr = dsocketptr->socket[ii];
			if ((socket_listening == socketptr->state) || (socket_connected == socketptr->state))
			{
				if (socket_connected == socketptr->state)
				{ /* if buffer not empty set flag but not FD_SET */
					nconnected++;
					if (0 < socketptr->buffered_length)
					{	/* something in the buffer so ready now */
						if (!socketptr->pendingevent)
						{
							socketptr->pendingevent = TRUE;
							socketptr->readycycle = dsocketptr->waitcycle;
						}
						rconnected++;
						continue;
					}
				} else
				{
					nlisten++;
					if (socketptr->pendingevent)
					{
						rlisten++;
						continue;	/* ready for ACCEPT now */
					}
				}
#ifdef USE_POLL
				poll_fds[poll_nfds].fd = socketptr->sd;
				poll_fds[poll_nfds].events = POLLIN;
				poll_socketptr[poll_nfds] = socketptr;
				poll_nfds++;
#endif
#ifdef USE_SELECT
				assertpro(FD_SETSIZE > socketptr->sd);
				FD_SET(socketptr->sd, &select_fdset);
				select_max_fd = MAX(select_max_fd, socketptr->sd);
#endif
				nselect++;
			}
		}
		if (nselect)
		{
			if (NO_M_TIMEOUT != msec_timeout)
			{
				utimeout.tv_sec = msec_timeout / MILLISECS_IN_SEC;
				utimeout.tv_usec = (msec_timeout % MILLISECS_IN_SEC) * MICROSECS_IN_MSEC;
				sys_get_curr_time(&cur_time);
				if (!retry_accept && (!zint_restart || !sockintr->end_time_valid))
					add_int_to_abs_time(&cur_time, msec_timeout, &end_time);
				else
				{       /* end_time taken from restart data. Compute what msec_timeout should be so timeout timer
				   	gets set correctly below.  Or retry after failed accept.
					*/
					DBGSOCK((stdout, "socwait: Taking timeout end time from wait restart data\n"));
					cur_time = sub_abs_time(&end_time, &cur_time);
					msec_timeout = (int4)(cur_time.at_sec * MILLISECS_IN_SEC +
						/* Round up in order to prevent premature timeouts */
						DIVIDE_ROUND_UP(cur_time.at_usec, MICROSECS_IN_MSEC));
					if (0 > msec_timeout)
					{
						msec_timeout = -1;
						utimeout.tv_sec = 0;
						utimeout.tv_usec = 0;
					} else
					{
						utimeout.tv_sec = cur_time.at_sec;
						utimeout.tv_usec = (gtm_tv_usec_t)cur_time.at_usec;
					}
				}
			}
			zint_restart = sockintr->end_time_valid = FALSE;
			for ( ; ; )
			{
#ifdef USE_POLL
				if ((0 < rconnected) || (0 <rlisten))
					poll_timeout = 0;
				else if (NO_M_TIMEOUT == msec_timeout)
					poll_timeout = -1;
				else
					poll_timeout = (utimeout.tv_sec * MILLISECS_IN_SEC) +
						DIVIDE_ROUND_UP(utimeout.tv_usec, MICROSECS_IN_MSEC);
				poll_fd = -1;
				rv = poll(poll_fds, poll_nfds, poll_timeout);
#endif
#ifdef USE_SELECT
				utimeoutptr = &utimeout;
				if ((0 < rconnected) || (0 <rlisten))
					utimeout.tv_sec = utimeout.tv_usec = 0;
				else if (NO_M_TIMEOUT == msec_timeout)
					utimeoutptr = (struct timeval *)NULL;
				rv = select(select_max_fd + 1, (void *)&select_fdset, (void *)0, (void *)0, utimeoutptr);
#endif
				if (0 > rv && EINTR == errno)
				{
					if (0 != outofband)
					{
						if (OUTOFBAND_RESTARTABLE(outofband))
						{
							DBGSOCK((stdout, "socwait: outofband interrupt received (%d) -- "
								"queueing mv_stent for wait intr\n", outofband));
							PUSH_MV_STENT(MVST_ZINTDEV);
							mv_chain->mv_st_cont.mvs_zintdev.io_ptr = iod;
							mv_chain->mv_st_cont.mvs_zintdev.buffer_valid = FALSE;
							sockintr->who_saved = sockwhich_wait;
							if (NO_M_TIMEOUT != msec_timeout)
							{
								sockintr->end_time = end_time;
								sockintr->end_time_valid = TRUE;
							} else
								sockintr->end_time_valid = FALSE;
							dsocketptr->mupintr = TRUE;
							socketus_interruptus++;
							DBGSOCK((stdout, "socwait: mv_stent queued - endtime: %d/%d"
								"  interrupts: %d\n", end_time.at_sec, end_time.at_usec,
								socketus_interruptus));
						}
						REVERT_GTMIO_CH(&iod->pair, ch_set);
						outofband_action(FALSE);
						assertpro(FALSE);      /* Should *never* return from outofband_action */
						return FALSE;   /* For the compiler.. */
					}
					if (NO_M_TIMEOUT != msec_timeout)
					{
						sys_get_curr_time(&cur_time);
						cur_time = sub_abs_time(&end_time, &cur_time);
						msec_timeout = (int4)(cur_time.at_sec * MILLISECS_IN_SEC +
							/* Round up in order to prevent premature timeouts */
							DIVIDE_ROUND_UP(cur_time.at_usec, MICROSECS_IN_MSEC));
						if (0 >msec_timeout)
						{
							rv = 0;		/* time out */
							break;
						}
						utimeout.tv_sec = cur_time.at_sec;
						utimeout.tv_usec = (gtm_tv_usec_t)cur_time.at_usec;
					}
				} else
					break;	/* either other error or done */
			}
			if ((rv == 0) && (0 == rconnected) && (0 == rlisten))
			{	/* none selected or prior pending event */
				iod->dollar.key[0] = '\0';
				if (NO_M_TIMEOUT != msec_timeout)
				{
					dollar_truth = FALSE;
					REVERT_GTMIO_CH(&iod->pair, ch_set);
					return FALSE;
				} else
					continue;
			} else  if (rv < 0)
			{
				errptr = (char *)STRERROR(errno);
				errlen = STRLEN(errptr);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_SOCKWAIT, 0, ERR_TEXT, 2, errlen, errptr);
				return FALSE;
			}
		} else if ((0 == rlisten) && (0 == rconnected))
		{	/* nothing to select and no pending events */
			iod->dollar.key[0] = '\0';
			if (NO_M_TIMEOUT != msec_timeout)
			{
				dollar_truth = FALSE;
				REVERT_GTMIO_CH(&iod->pair, ch_set);
				return FALSE;
			} else
				continue;
		}
		/* find out which sockets are ready */
		oldestlistencycle = oldestconnectedcycle = oldesteventcycle = 0;
		oldestlistenindex = oldestconnectedindex = oldesteventindex = -1;
		for (ii = 0; ii < dsocketptr->n_socket; ii++)
		{
			socketptr = dsocketptr->socket[ii];
			if ((socket_listening != socketptr->state) && (socket_connected != socketptr->state))
				continue;	/* not a candidate for /WAIT */
#ifdef USE_POLL
			for (jj = 0; jj < poll_nfds; jj++)
			{
				if (socketptr == poll_socketptr[jj])
					break;
			}
			assertpro((0 == jj) || (jj <= poll_nfds));	/* equal poll_nfds if not polled */
			if (nselect && (jj != poll_nfds) && (socketptr->sd == poll_fds[jj].fd) && poll_fds[jj].revents)
#endif
#ifdef USE_SELECT
			assertpro(FD_SETSIZE > socketptr->sd);
			if (nselect && (0 != FD_ISSET(socketptr->sd, &select_fdset)))
#endif
			{	/* set flag in socketptr and keep going */
				socketptr->pendingevent = TRUE;
				socketptr->readycycle = dsocketptr->waitcycle;
				if (socket_listening == socketptr->state)
					rlisten++;
				else
					rconnected++;
			}
			if (socketptr->pendingevent)	/* may be from prior /WAIT */
			{
				if (socket_listening == socketptr->state)
				{
					if (0 == oldestlistencycle)
					{
						oldestlistencycle = socketptr->readycycle;
						oldestlistenindex = ii;
					} else if (oldestlistencycle > socketptr->readycycle)
					{	/* this socket waiting longer */
						oldestlistencycle = socketptr->readycycle;
						oldestlistenindex = ii;
					}
				} else
				{
					if (0 == oldestconnectedcycle)
					{
						oldestconnectedcycle = socketptr->readycycle;
						oldestconnectedindex = ii;
					} else if (oldestconnectedcycle > socketptr->readycycle)
					{	/* this socket waiting longer */
						oldestconnectedcycle = socketptr->readycycle;
						oldestconnectedindex = ii;
					}
				}
			}
		}
		if (0 < oldestlistencycle)
		{
			oldesteventcycle = oldestlistencycle;
			oldesteventindex = oldestlistenindex;
		} else if (0 < oldestconnectedcycle)
		{
			oldesteventcycle = oldestconnectedcycle;
			oldesteventindex = oldestconnectedindex;
		} else
		{	/* unexpected nothing to do */
			assert((0 < oldestlistencycle) || (0 < oldestconnectedcycle));
			iod->dollar.key[0] = '\0';
			if (NO_M_TIMEOUT != msec_timeout)
			{
				dollar_truth = FALSE;
				REVERT_GTMIO_CH(&iod->pair, ch_set);
				return FALSE;
			} else
				continue;
		}
		socketptr = dsocketptr->socket[oldesteventindex];
		socketptr->pendingevent = FALSE;
		if (socket_listening == socketptr->state)
		{
			rv = iosocket_accept(dsocketptr, socketptr, FALSE);
			if (0 < rv)
			{
				retry_accept = TRUE;
				continue;	/* pending connection gone so redo */
			} else if (-1 == rv)
			{
				REVERT_GTMIO_CH(&iod->pair, ch_set);
				return FALSE;	/* error handled in iosocket_accept */
			}
		} else
		{
			assert(socket_connected == socketptr->state);
			dsocketptr->current_socket = oldesteventindex;
			len = SIZEOF(READ) - 1;
			memcpy(&iod->dollar.key[0], READ, len);
			iod->dollar.key[len++] = '|';
			memcpy(&iod->dollar.key[len], socketptr->handle, socketptr->handle_len);
			len += socketptr->handle_len;
			iod->dollar.key[len++] = '|';
			if (NULL != socketptr->remote.saddr_ip)
			{
				strncpy(&iod->dollar.key[len], socketptr->remote.saddr_ip, DD_BUFLEN - 1 - len);
			} else
			{
				assertpro(socket_local == socketptr->protocol);
				if (NULL != socketptr->local.sa)
					charptr = ((struct sockaddr_un *)(socketptr->local.sa))->sun_path;
				else if (NULL != socketptr->remote.sa)
					charptr = ((struct sockaddr_un *)(socketptr->remote.sa))->sun_path;
				else
					charptr = (char *)"";
				strncpy(&dsocketptr->iod->dollar.key[len], charptr, DD_BUFLEN - len - 1);
			}
			iod->dollar.key[DD_BUFLEN - 1] = '\0';
		}
		break;
	}
	if (NO_M_TIMEOUT != msec_timeout)
		dollar_truth = TRUE;
	REVERT_GTMIO_CH(&iod->pair, ch_set);
	return TRUE;
}

int iosocket_accept(d_socket_struct *dsocketptr, socket_struct *socketptr, boolean_t selectfirst)
{
	char            	*errptr;
	int4            	errlen;
	int			rv, len, errcode, save_errno;
	char			port_buffer[NI_MAXSERV], ipaddr[SA_MAXLEN + 1];
#ifdef USE_POLL
	struct pollfd		poll_fds;
	int			poll_fd;
#endif
#ifdef USE_SELECT
	fd_set			select_fdset;
#endif
	struct timeval		utimeout;
	GTM_SOCKLEN_TYPE	size, addrlen;
	socket_struct		*newsocketptr;
	struct sockaddr_storage	peer;           /* socket address + port */
	struct sockaddr		*peer_sa_ptr;

	if (gtm_max_sockets <= dsocketptr->n_socket)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_SOCKMAX, 1, gtm_max_sockets);
		return -1;
	}
	peer_sa_ptr = ((struct sockaddr *)(&peer));
	if (selectfirst || (dsocketptr->waitcycle > socketptr->readycycle))
	{	/* if not selected this time do a select first to check if connection still there */
#ifdef USE_POLL
		poll_fds.fd = socketptr->sd;
		poll_fds.events = POLLIN;
		rv = poll(&poll_fds, 1, 0);
#endif
#ifdef USE_SELECT
		FD_ZERO(&select_fdset);
		FD_SET(socketptr->sd, &select_fdset);
		utimeout.tv_sec = utimeout.tv_usec = 0;
		rv = select(socketptr->sd + 1, (void *)&select_fdset, (void *)0, (void *)0, &utimeout);
#endif
		if (0 > rv)
		{
			errptr = (char *)STRERROR(errno);
			errlen = STRLEN(errptr);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_SOCKWAIT, 0, ERR_TEXT, 2, errlen, errptr);
			return -1;
		} else if (0 == rv)
			return EWOULDBLOCK;	/* signal to find another ready socket */
	}
	size = SIZEOF(struct sockaddr_storage);
	ACCEPT_SOCKET(socketptr->sd, peer_sa_ptr, &size, rv);
	if (-1 == rv)
	{
		switch (errno)
		{
			case ENOBUFS:
			case ECONNABORTED:
			case ETIMEDOUT:
			case ECONNRESET:
			case ENOTCONN:
			case ENOSR:
				return errno;	/* pending connection gone so retry */
			default:
				errptr = (char *)STRERROR(errno);
				errlen = STRLEN(errptr);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_SOCKACPT, 0, ERR_TEXT, 2, errlen, errptr);
				return -1;
		}
	}
	SOCKET_DUP(socketptr, newsocketptr);
	newsocketptr->remote.ai.ai_socktype = socketptr->local.ai.ai_socktype;
	newsocketptr->remote.ai.ai_protocol = socketptr->local.ai.ai_protocol;
	newsocketptr->lastaction = newsocketptr->readycycle = 0;
	newsocketptr->pendingevent = FALSE;
	newsocketptr->sd = rv;
	if (socket_local != newsocketptr->protocol)
	{	/* translate internal address to numeric ip address */
		SOCKET_ADDR_COPY(newsocketptr->remote, peer_sa_ptr, size);	/* info not set for socket_local */
		GETNAMEINFO(peer_sa_ptr, size, ipaddr, SA_MAXLEN, NULL, 0, NI_NUMERICHOST, errcode);
		if (0 != errcode)
		{
			close(newsocketptr->sd);
			SOCKET_FREE(newsocketptr);
			RTS_ERROR_ADDRINFO(NULL, ERR_GETNAMEINFO, errcode);
			return -1;
		}
		if (NULL != newsocketptr->remote.saddr_ip)
			free(newsocketptr->remote.saddr_ip);
		STRNDUP(ipaddr, SA_MAXLEN, newsocketptr->remote.saddr_ip);
		/* translate internal address to port number*/
		GETNAMEINFO(peer_sa_ptr, size, NULL, 0, port_buffer, NI_MAXSERV, NI_NUMERICSERV, errcode);
		if (0 != errcode)
		{
			close(newsocketptr->sd);
			SOCKET_FREE(newsocketptr);
			RTS_ERROR_ADDRINFO(NULL, ERR_GETNAMEINFO, errcode);
			return -1;
		}
		newsocketptr->remote.port = ATOI(port_buffer);
		newsocketptr->remote.ai.ai_addrlen = size;
		addrlen = SIZEOF(struct sockaddr_storage);
		if (-1 == getsockname(newsocketptr->sd, SOCKET_LOCAL_ADDR(newsocketptr), &addrlen))
		{
			save_errno = errno;
			errptr = (char *)STRERROR(save_errno);
			errlen = STRLEN(errptr);
			close(newsocketptr->sd);
			SOCKET_FREE(newsocketptr);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_GETSOCKNAMERR, 3, save_errno, errlen, errptr);
			return -1;
		}
		newsocketptr->local.ai.ai_addrlen = addrlen;
		newsocketptr->local.ai.ai_family = SOCKET_LOCAL_ADDR(newsocketptr)->sa_family;
		GETNAMEINFO(SOCKET_LOCAL_ADDR(newsocketptr), newsocketptr->local.ai.ai_addrlen, ipaddr,
			SA_MAXLEN, NULL, 0, NI_NUMERICHOST, errcode);
		if (0 != errcode)
		{
			close(newsocketptr->sd);
			SOCKET_FREE(newsocketptr);
			RTS_ERROR_ADDRINFO(NULL, ERR_GETNAMEINFO, errcode);
			return -1;
		}
		if (NULL != newsocketptr->local.saddr_ip)
			free(newsocketptr->local.saddr_ip);
		STRNDUP(ipaddr, SA_MAXLEN, newsocketptr->local.saddr_ip);
	}
	newsocketptr->state = socket_connected;
	newsocketptr->passive = FALSE;
	newsocketptr->howcreated = creator_accept;
	newsocketptr->first_read = newsocketptr->first_write = TRUE;
	/* put the new-born socket to the list and create a handle for it */
	iosocket_handle(newsocketptr->handle, &newsocketptr->handle_len, TRUE, dsocketptr);
	STRNDUP(socketptr->handle, socketptr->handle_len, newsocketptr->parenthandle);
	socketptr->lastaction = dsocketptr->waitcycle;	/* record cycle for last connect */
	dsocketptr->socket[dsocketptr->n_socket++] = newsocketptr;
	dsocketptr->current_socket = dsocketptr->n_socket - 1;
	len = SIZEOF(CONNECTED) - 1;
	memcpy(&dsocketptr->iod->dollar.key[0], CONNECTED, len);
	dsocketptr->iod->dollar.key[len++] = '|';
	memcpy(&dsocketptr->iod->dollar.key[len], newsocketptr->handle, newsocketptr->handle_len);
	len += newsocketptr->handle_len;
	dsocketptr->iod->dollar.key[len++] = '|';
	if (socket_local != newsocketptr->protocol)
		strncpy(&dsocketptr->iod->dollar.key[len], newsocketptr->remote.saddr_ip, DD_BUFLEN - 1 - len);
	else
	{ /* get path from listening socket local side */
		assert(NULL != socketptr->local.sa);
		STRNCPY_STR(&dsocketptr->iod->dollar.key[len], ((struct sockaddr_un *)(socketptr->local.sa))->sun_path,
			DD_BUFLEN - len);
		SOCKET_ADDR_COPY(newsocketptr->remote, socketptr->local.sa, SIZEOF(struct sockaddr_un));
		newsocketptr->remote.ai.ai_addrlen = socketptr->local.ai.ai_addrlen;
	}
	dsocketptr->iod->dollar.key[DD_BUFLEN - 1] = '\0';		/* In case we fill the buffer */
	newsocketptr->remote.ai.ai_family = SOCKET_REMOTE_ADDR(newsocketptr)->sa_family;
	return 0;
}
