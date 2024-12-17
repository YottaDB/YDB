/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
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
#include "gtm_poll.h"

#ifdef DEBUG_SOCKWAIT
#include "gtmio.h"
#include "have_crit.h"		/* DBGSOCKWAIT needs for DBGFPF */
#endif

#include "io_params.h"
#include "gt_timer.h"
#include "io.h"
#include "iotimer.h"
#include "iosocketdef.h"
#include "min_max.h"
#include "deferred_events_queue.h"
#include "stack_frame.h"
#include "mv_stent.h"
#include "gtm_netdb.h"
#include "gtm_stdlib.h"
#include "eintr_wrappers.h"
#include "error.h"
#include "gtm_caseconv.h"

#define	CONNECTED	"CONNECT"
#define READ		"READ"
#define WRITE		"WRITE"

#define	WAIT_FOR_READ		1       /* or incoming connection */
#define	WAIT_FOR_WRITE  	2
#define	WAIT_FOR_ISDEFAULT  	4
#define WAIT_FOR_DEFAULT        (WAIT_FOR_READ | WAIT_FOR_WRITE | WAIT_FOR_ISDEFAULT)

GBLREF int			dollar_truth, socketus_interruptus;
GBLREF uint4			ydb_max_sockets;
GBLREF mv_stent			*mv_chain;
GBLREF stack_frame  	        *frame_pointer;
GBLREF unsigned char            *stackbase, *stacktop, *msp, *stackwarn;
GBLREF volatile boolean_t	dollar_zininterrupt;
GBLREF volatile int4		outofband;

boolean_t iosocket_wait(io_desc *iod, uint8 nsec_timeout, mval *whatop, mval *handle)
{
	ABS_TIME		utimeout, *utimeoutptr, cur_time, end_time;
	nfds_t			poll_nfds;
	struct pollfd		*poll_fds;
	socket_struct		**poll_socketptr;	/* matching poll_fds */
	size_t			poll_fds_size;
	int			poll_timeout;
	d_socket_struct 	*dsocketptr;
	socket_struct   	*socketptr, *which_socketptr = NULL, *prev_socketptr;;
	socket_interrupt	*sockintr;
	char            	*errptr, *charptr;
	int4            	errlen, ii, jj, handle_index;
	int4			npoll, rlisten, rconnected, rwrite;
	int4			oldestconnectedcycle, oldestconnectedindex;
	int4			oldestwritecycle, oldestwriteindex;
	int4			oldestlistencycle, oldestlistenindex;
	int4			oldesteventindex;
	int			rv, max_fd, len, len1;
	boolean_t		zint_restart, retry_accept = FALSE;
	mv_stent		*mv_zintdev;
	int			errcode;
	int			wait_for_what = 0;	/* bit mask */
	char			wait_for_string[MAX_DEVCTL_LENGTH + 1];
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
		if (NULL == whatop)
			wait_for_what = WAIT_FOR_DEFAULT;
		else
		{
			MV_FORCE_STR(whatop);
			assert(sizeof(wait_for_string) > whatop->str.len);
			lower_to_upper((uchar_ptr_t)wait_for_string, (uchar_ptr_t)whatop->str.addr,
				MIN((sizeof(wait_for_string) - 1), whatop->str.len));
			wait_for_string[whatop->str.len] = '\0';
			if (strstr(wait_for_string, READ))
				wait_for_what |= WAIT_FOR_READ;
			if (strstr(wait_for_string, WRITE))
				wait_for_what |= WAIT_FOR_WRITE;
			if (0 == wait_for_what)
			{	/* no valid value found */
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_SOCKWAITARG, 4, RTS_ERROR_LITERAL("Second"),
						RTS_ERROR_LITERAL("value is not valid"));
				return FALSE;
			}
		}
		if (NULL != handle)
		{
			MV_FORCE_STR(handle);
			/* WARNING inline assignment below */
			if (0 > (handle_index = iosocket_handle(handle->str.addr, &handle->str.len, FALSE, dsocketptr)))
			{
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SOCKNOTFND, 2, handle->str.len, handle->str.addr);
				return FALSE;		/* for compiler and analyzers */
			}
			which_socketptr = dsocketptr->socket[handle_index];
		}
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
			which_socketptr = (socket_struct *)mv_zintdev->mv_st_cont.mvs_zintdev.socketptr;
			wait_for_what = sockintr->wait_for_what;
			/* Done with this mv_stent. Pop it off if we can, else mark it inactive. */
			if (mv_chain == mv_zintdev)
				POP_MV_STENT();         /* pop if top of stack */
			else
			{       /* else mark it unused */
				mv_zintdev->mv_st_cont.mvs_zintdev.buffer_valid = FALSE;
				mv_zintdev->mv_st_cont.mvs_zintdev.io_ptr = NULL;
			}
			zint_restart = TRUE;
			DBGSOCK((stdout, "socwait: mv_stent found - endtime: %d/%d\n",		\
					end_time.tv_sec, end_time.tv_nsec / NANOSECS_IN_MSEC));
		} else
			DBGSOCK((stdout, "socwait: no mv_stent found !!\n"));
		dsocketptr->mupintr = FALSE;
		sockintr->who_saved = sockwhich_invalid;
	}
	/* check for events */
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
	DBGSOCKWAIT((stdout,"waitcycle= %d\n",dsocketptr->waitcycle));
	while (TRUE)
	{
		DBGSOCKWAIT((stdout,"wait loop:\n"));
		poll_nfds = 0;
		npoll = rlisten = rconnected = rwrite = 0;
		for (ii = 0; ii < dsocketptr->n_socket; ii++)
		{
			if (which_socketptr)
				socketptr = which_socketptr;
			else
				socketptr = dsocketptr->socket[ii];
			socketptr->current_events = 0;
			if ((socket_listening == socketptr->state) || (socket_connected == socketptr->state))
			{
				if (socket_connected == socketptr->state)
				{	/* Connected socket */
					/* if buffer not empty set flag but not FD_SET */
					if ((0 < socketptr->buffered_length) && (WAIT_FOR_READ & wait_for_what))
					{	/* something in the buffer so ready now */
						if (!(SOCKPEND_READ & socketptr->pendingevent))
						{
							socketptr->current_events |= SOCKPEND_BUFFER | SOCKPEND_READ;
							DBGSOCKWAIT((stdout,"socket[%d] buffer, priorreadycycle= %d, pending= %d,"
								" current= %d\n", ii, socketptr->readycycle,
								socketptr->pendingevent, socketptr->current_events));
							socketptr->readycycle = dsocketptr->waitcycle;
						}
						socketptr->readyforwhat |= SOCKREADY_READ;
						rconnected++;
						if (!socketptr->nonblocked_output)
							continue;	/* no need to check if writable */
					}
				} else if (WAIT_FOR_READ & wait_for_what)
				{
					if (SOCKPEND_READ & socketptr->pendingevent)
					{
						rlisten++;
						continue;	/* ready for ACCEPT now */
					}
				}
				poll_fds[poll_nfds].fd = socketptr->sd;
				poll_fds[poll_nfds].events = 0;
				if (WAIT_FOR_READ & wait_for_what)
					poll_fds[poll_nfds].events = POLLIN;
				if ((socket_connected == socketptr->state) && socketptr->nonblocked_output
					&& (WAIT_FOR_WRITE & wait_for_what))
					poll_fds[poll_nfds].events |= POLLOUT;
				poll_socketptr[poll_nfds] = socketptr;
				poll_nfds++;
				npoll++;
			}
			if (which_socketptr)
				break;		/* only check the one socket */
		}
		if (npoll)
		{
			if (NO_M_TIMEOUT != nsec_timeout)
			{
				utimeout.tv_sec = nsec_timeout / NANOSECS_IN_SEC;
				utimeout.tv_nsec = (nsec_timeout % NANOSECS_IN_SEC);
				sys_get_curr_time(&cur_time);
				if (!retry_accept && (!zint_restart || !sockintr->end_time_valid))
					add_uint8_to_abs_time(&cur_time, nsec_timeout, &end_time);
				else
				{       /* end_time taken from restart data. Compute what nsec_timeout should be so timeout timer
				   	gets set correctly below.  Or retry after failed accept.
					*/
					DBGSOCK((stdout, "socwait: Taking timeout end time from wait restart data\n"));
					cur_time = sub_abs_time(&end_time, &cur_time);
					SET_NSEC_TIMEOUT_FROM_DELTA_TIME(cur_time, nsec_timeout);
					if (0 == nsec_timeout)
					{
						utimeout.tv_sec = 0;
						utimeout.tv_nsec = 0;
					} else
					{
						utimeout.tv_sec = cur_time.tv_sec;
						utimeout.tv_nsec = cur_time.tv_nsec;
					}
				}
			}
			zint_restart = sockintr->end_time_valid = FALSE;
			for ( ; ; )
			{
				if ((0 < rconnected) || (0 < rlisten) || (0 < rwrite))
					poll_timeout = 0;
				else if (NO_M_TIMEOUT == nsec_timeout)
					poll_timeout = -1;
				else
					poll_timeout = (utimeout.tv_sec * MILLISECS_IN_SEC) +
						DIVIDE_ROUND_UP(utimeout.tv_nsec, NANOSECS_IN_MSEC);
				rv = poll(poll_fds, poll_nfds, poll_timeout);
				if (0 > rv && EINTR == errno)
				{
					eintr_handling_check();
					if (0 != outofband)
					{
						if (OUTOFBAND_RESTARTABLE(outofband))
						{
							DBGSOCK((stdout, "socwait: outofband interrupt received (%d) -- "
								"queueing mv_stent for wait intr\n", outofband));
							PUSH_MV_STENT(MVST_ZINTDEV);
							mv_chain->mv_st_cont.mvs_zintdev.io_ptr = iod;
							mv_chain->mv_st_cont.mvs_zintdev.buffer_valid = FALSE;
							mv_chain->mv_st_cont.mvs_zintdev.socketptr = which_socketptr;
							sockintr->wait_for_what = wait_for_what;
							sockintr->who_saved = sockwhich_wait;
							if (NO_M_TIMEOUT != nsec_timeout)
							{
								sockintr->end_time = end_time;
								sockintr->end_time_valid = TRUE;
							} else
								sockintr->end_time_valid = FALSE;
							dsocketptr->mupintr = TRUE;
							socketus_interruptus++;
							DBGSOCK((stdout, "socwait: mv_stent queued - endtime: %d/%d"
								"  interrupts: %d\n", end_time.tv_sec,
								end_time.tv_nsec / NANOSECS_IN_USEC, socketus_interruptus));
						}
						REVERT_GTMIO_CH(&iod->pair, ch_set);
						async_action(FALSE);
						assertpro(FALSE);      /* Should *never* return from async_action */
						return FALSE;   /* For the compiler.. */
					}
					if (NO_M_TIMEOUT != nsec_timeout)
					{
						sys_get_curr_time(&cur_time);
						cur_time = sub_abs_time(&end_time, &cur_time);
						SET_NSEC_TIMEOUT_FROM_DELTA_TIME(cur_time, nsec_timeout);
						if (0 == nsec_timeout)
						{
							rv = 0;		/* time out */
							break;
						}
						utimeout.tv_sec = cur_time.tv_sec;
						utimeout.tv_nsec = cur_time.tv_nsec;
					}
				} else
				{
					HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
					break;	/* either other error or done */
				}
			}
			if ((rv == 0) && (0 == rconnected) && (0 == rlisten) && (0 == rwrite))
			{	/* none selected or prior pending event */
				iod->dollar.key[0] = '\0';
				if (NO_M_TIMEOUT != nsec_timeout)
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
		} else if ((0 == rlisten) && (0 == rconnected) && (0 == rwrite))
		{	/* nothing to select and no pending events */
			iod->dollar.key[0] = '\0';
			if (NO_M_TIMEOUT != nsec_timeout)
				dollar_truth = FALSE;
			REVERT_GTMIO_CH(&iod->pair, ch_set);
			return FALSE;
		}
		/* find out which sockets are ready */
		oldestlistencycle = oldestconnectedcycle = oldestwritecycle = 0;
		oldestlistenindex = oldestconnectedindex = oldestwriteindex = -1;
#		ifdef DEBUG
		boolean_t	poll_error = FALSE;	/* set to TRUE if AT LEAST one socket poll() returned POLLERR or POLLHUP */
#		endif
		for (ii = 0; ii < dsocketptr->n_socket; ii++)
		{
			if (which_socketptr && (which_socketptr != dsocketptr->socket[ii]))
				continue;
			socketptr = dsocketptr->socket[ii];
			if ((socket_listening != socketptr->state) && (socket_connected != socketptr->state))
				continue;	/* not a candidate for /WAIT */
			for (jj = 0; jj < poll_nfds; jj++)
			{
				if (socketptr == poll_socketptr[jj])
					break;
			}
			assertpro((0 == jj) || (jj <= poll_nfds));	/* equal poll_nfds if not polled */
			if (npoll && (jj != poll_nfds) && (socketptr->sd == poll_fds[jj].fd) && poll_fds[jj].revents)
			{	/* set flag in socketptr and keep going */
#				ifdef DEBUG
				if ((POLLERR | POLLHUP) & poll_fds[jj].revents)
					poll_error = TRUE;
#				endif
				if (POLLIN & poll_fds[jj].revents)
				{
					socketptr->current_events |= SOCKPEND_READ;
					socketptr->readyforwhat |= SOCKREADY_READ;
					DBGSOCKWAIT((stdout,"socket[%d] pollin, priorreadycycle= %d, pending= %d, current= %d\n",
						ii, socketptr->readycycle, socketptr->pendingevent, socketptr->current_events));
					if (!socketptr->pendingevent && !(SOCKPEND_BUFFER & socketptr->current_events))
						socketptr->readycycle = dsocketptr->waitcycle;
				}
				if (socket_listening == socketptr->state)
				{
					rlisten++;
					socketptr->readycycle = dsocketptr->waitcycle;
				} else
				{
					if (poll_fds[jj].revents & POLLOUT)
					{
						socketptr->current_events |= SOCKPEND_WRITE;
						socketptr->readyforwhat |= SOCKREADY_WRITE;
						if (!socketptr->pendingevent)
						{
							DBGSOCKWAIT((stdout,"socket[%d] pollout, priorreadycycle= %d, "
								"pending= %d, current= %d\n", ii, socketptr->readycycle,
								socketptr->pendingevent, socketptr->current_events));
							socketptr->readycycle = dsocketptr->waitcycle;	/* newly ready */
						}
						rwrite++;
					}
					rconnected++;
				}
			}
			if (socketptr->current_events || (SOCKPEND_READ & socketptr->pendingevent))
			{	/* smallest readycycle is the oldest aka longest unselected */
				if (socket_listening == socketptr->state)
				{
					if (0 == oldestlistencycle)
					{
						oldestlistencycle = socketptr->readycycle;
						oldestlistenindex = ii;
					} else if (oldestlistencycle > socketptr->readycycle)
					{	/* this socket waiting longer */
						oldestlistencycle = socketptr->readycycle;
						assert(0 <= oldestlistenindex);
						prev_socketptr = dsocketptr->socket[oldestlistenindex];
						prev_socketptr->pendingevent |= (SOCKPEND_READ & prev_socketptr->current_events);
						oldestlistenindex = ii;
					} else
					{
						DBGSOCKWAIT((stdout, "socket[%d] LISTEN priorpending= %d, current %d\n",
							ii, socketptr->pendingevent, socketptr->current_events));
						socketptr->pendingevent |= (SOCKPEND_READ & socketptr->current_events);
					}
				} else
				{	/* only select for write if ready this time */
					if (SOCKPEND_WRITE & socketptr->current_events)
					{
						if (0 == oldestwritecycle)
						{
							oldestwritecycle = socketptr->readycycle;
							oldestwriteindex = ii;
							DBGSOCKWAIT((stdout,"socket[%d] oldestwrite, readycycle = %d\n",
								ii, socketptr->readycycle));
						} else if (oldestwritecycle > socketptr->readycycle)
						{	/* this socket waiting longer */
							oldestwritecycle = socketptr->readycycle;
							DBGSOCKWAIT((stdout,"socket[%d] oldestwrite(replace), readycycle = %d\n",
								ii, socketptr->readycycle));
							assert(0 <= oldestwriteindex);
							prev_socketptr = dsocketptr->socket[oldestwriteindex];
							prev_socketptr->pendingevent
								|= (SOCKPEND_WRITE & prev_socketptr->current_events);
							oldestwriteindex = ii;
						} else
						{
							DBGSOCKWAIT((stdout, "socket[%d] WRITE priorpending= %d, current %d\n",
								ii, socketptr->pendingevent, socketptr->current_events));
							socketptr->pendingevent |= (SOCKPEND_WRITE & socketptr->current_events);
						}
					}
					if ((SOCKPEND_READ & socketptr->current_events)
						|| (SOCKPEND_READ & socketptr->pendingevent))
					{
						if (0 == oldestconnectedcycle)
						{
							oldestconnectedcycle = socketptr->readycycle;
							DBGSOCKWAIT((stdout,"socket[%d] oldestread, readycycle = %d\n",
								ii, socketptr->readycycle));
							oldestconnectedindex = ii;
						} else if (oldestconnectedcycle > socketptr->readycycle)
						{	/* this socket waiting longer */
							oldestconnectedcycle = socketptr->readycycle;
							DBGSOCKWAIT((stdout, "socket[%d] oldestread(replace), readycycle = %d\n",
								ii, socketptr->readycycle));
							assert(0 <= oldestconnectedindex);
							prev_socketptr = dsocketptr->socket[oldestconnectedindex];
							prev_socketptr->pendingevent
								|= (SOCKPEND_READ & prev_socketptr->current_events);
							oldestconnectedindex = ii;
						} else
						{
							DBGSOCKWAIT((stdout, "socket[%d] READ priorpending= %d, current %d\n",
								ii, socketptr->pendingevent, socketptr->current_events));
							socketptr->pendingevent |= (SOCKPEND_READ & socketptr->current_events);
						}
					}
				}
			}
		}
		if (0 < oldestlistencycle)
		{
			oldesteventindex = oldestlistenindex;
		} else if (0 < oldestconnectedcycle)
		{	/* something to READ has priority over a WRITE */
			oldesteventindex = oldestconnectedindex;
			DBGSOCKWAIT((stdout,"selected read socket[%d], cycle = %d\n", oldestconnectedindex, oldestconnectedcycle));
		} else if (0 < oldestwritecycle)
		{
			oldesteventindex = oldestwriteindex;
			DBGSOCKWAIT((stdout,"selected write socket[%d], cycle = %d\n", oldestwriteindex, oldestwritecycle));
		} else
		{	/* This is possible if "poll()" returned due to a POLLERR or POLLHUP condition in "poll_fds[jj].revents"
			 * but no POLLIN or POLLOUT condition. That is, the peer side hung up unexpectedly. In that case, there
			 * is nothing we can do.
			 */
			assert(poll_error);
			iod->dollar.key[0] = '\0';
			if (NO_M_TIMEOUT != nsec_timeout)
			{
				dollar_truth = FALSE;
				REVERT_GTMIO_CH(&iod->pair, ch_set);
				return FALSE;
			} else
				continue;
		}
		socketptr = dsocketptr->socket[oldesteventindex];
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
			len = 0;
			/* $KEY can only have one item so READWRITE if both */
			if (socketptr->readyforwhat & SOCKREADY_READ)
			{
				len1 = SIZEOF(READ) - 1;
				memcpy(&iod->dollar.key[len], READ, len1);
				len += len1;
			}
			if (socketptr->readyforwhat & SOCKREADY_WRITE)
			{
				len1 = SIZEOF(WRITE) - 1;
				memcpy(&iod->dollar.key[len], WRITE, len1);
				len += len1;
			}
			assert(0 != len);
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
	if (NO_M_TIMEOUT != nsec_timeout)
		dollar_truth = TRUE;
	REVERT_GTMIO_CH(&iod->pair, ch_set);
	return TRUE;
}

int iosocket_accept(d_socket_struct *dsocketptr, socket_struct *socketptr, boolean_t selectfirst)
{
	char            	*errptr;
	GTM_SOCKLEN_TYPE	size, addrlen, sockoptlen;
	int			rv, len, errcode, keepalive_opt, keepalive_value, save_errno;
	int4            	errlen;
	char			port_buffer[NI_MAXSERV], ipaddr[SA_MAXLEN + 1];
	struct pollfd		poll_fds;
	socket_struct		*newsocketptr;
	struct sockaddr		*peer_sa_ptr;
	struct sockaddr_storage	peer;           /* socket address + port */
	struct timeval		utimeout;
	static readonly char 	action[] = "ACCEPT";
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (ydb_max_sockets <= dsocketptr->n_socket)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_SOCKMAX, 1, ydb_max_sockets);
		return -1;
	}
	peer_sa_ptr = ((struct sockaddr *)(&peer));
	if (selectfirst || (dsocketptr->waitcycle > socketptr->readycycle))
	{	/* if not selected this time do a select first to check if connection still there */
		poll_fds.fd = socketptr->sd;
		poll_fds.events = POLLIN;
		do
		{
			rv = poll(&poll_fds, 1, 0);
			if ((0 <= rv) || (EINTR != (save_errno = errno)))	/* inline assigment */
				break;
			eintr_handling_check();
		} while (TRUE);
		HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
		if (0 > rv)
		{
			errptr = (char *)STRERROR(save_errno);
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
	newsocketptr->pendingevent = 0;
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
#		ifdef DEBUG
		sockoptlen = sizeof(keepalive_value);
		keepalive_value = 0;
		if (-1 == getsockopt(newsocketptr->sd, SOL_SOCKET, SO_KEEPALIVE, &keepalive_value, &sockoptlen))
		{
			save_errno = errno;
			errptr = (char *)STRERROR(save_errno);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_GETSOCKOPTERR, 5, LEN_AND_LIT("SO_KEEPALIVE"),
				save_errno, LEN_AND_STR(errptr));
			return -1;
		} else if ((0 != newsocketptr->options_state.alive)
				&& ((0 == keepalive_value) != (0 == newsocketptr->keepalive)))
			assert(keepalive_value == socketptr->keepalive);	/* AIX returns cnt if on */
#		endif
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
