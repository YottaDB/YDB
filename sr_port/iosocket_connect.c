/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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
#include <netinet/tcp.h>
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gt_timer.h"
#include "io.h"
#include "iotimer.h"
#include "iotcpdef.h"
#include "iosocketdef.h"
#include "iotcproutine.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "mv_stent.h"
#include "outofband.h"

#define	ESTABLISHED	"ESTABLISHED"

GBLREF	tcp_library_struct	tcp_routines;
GBLREF	volatile int4           outofband;
GBLREF	boolean_t               dollar_zininterrupt;
GBLREF	stack_frame             *frame_pointer;
GBLREF	unsigned char           *stackbase, *stacktop, *msp, *stackwarn;
GBLREF	mv_stent                *mv_chain;
GBLREF	int			socketus_interruptus;

boolean_t iosocket_connect(socket_struct *socketptr, int4 timepar, boolean_t update_bufsiz)
{
	int		temp_1 = 1;
	char		*errptr;
	int4            errlen, msec_timeout, real_errno;
	boolean_t	no_time_left = FALSE;
	short		len;
	io_desc		*iod;
	d_socket_struct *dsocketptr;
        socket_interrupt *sockintr;
	ABS_TIME        cur_time, end_time;
        mv_stent        *mv_zintdev;
	GTM_SOCKLEN_TYPE	sockbuflen;

	error_def(ERR_SOCKINIT);
	error_def(ERR_OPENCONN);
	error_def(ERR_TEXT);
	error_def(ERR_GETSOCKOPTERR);
	error_def(ERR_SETSOCKOPTERR);
	error_def(ERR_ZINTRECURSEIO);
	error_def(ERR_STACKCRIT);
	error_def(ERR_STACKOFLOW);

	SOCKET_DEBUG(PRINTF("socconn: ************* Entering socconn - timepar: %d\n",timepar); DEBUGSOCKFLUSH);
        /* check for validity */
	dsocketptr = socketptr->dev;
        assert(NULL != dsocketptr);
        sockintr = &dsocketptr->sock_save_state;
	iod = dsocketptr->iod;

        dsocketptr->dollar_key[0] = '\0';

#ifdef SOCK_INTR_CONNECT
	/*
	***** Note this code and the code further down is #ifdef'd out because this approach to handling jobinterrupts
              during connect was not feasible because we are always dealing with a new sockintr device on the restart
	      of an OPEN statement. There is no device to continue using so this approach needs to be changed.
	      The code is left here in case it is useful when this is fixed.
	*/
        /* Check for restart */
        if (dsocketptr->mupintr)
        {       /* We have a pending read restart of some sort - check we aren't recursing on this device */
                if (sockwhich_invalid == sockintr->who_saved)
                        GTMASSERT;      /* Interrupt should never have an invalid save state */
                if (dollar_zininterrupt)
                        rts_error(VARLSTCNT(1) ERR_ZINTRECURSEIO);
                if (sockwhich_connect != sockintr->who_saved)
                        GTMASSERT;      /* ZINTRECURSEIO should have caught */
                SOCKET_DEBUG(PRINTF("socconn: *#*#*#*#*#*#*#  Restarted interrupted connect\n"); DEBUGSOCKFLUSH);
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
                        SOCKET_DEBUG(PRINTF("socconn: mv_stent found - endtime: %d/%d\n", end_time.at_sec, end_time.at_usec);
                                     DEBUGSOCKFLUSH);
                } else
                        SOCKET_DEBUG(PRINTF("socconn: no mv_stent found !!\n"); DEBUGSOCKFLUSH);
                dsocketptr->mupintr = FALSE;
                sockintr->who_saved = sockwhich_invalid;
        }
#endif
	if (timepar != NO_M_TIMEOUT && !sockintr->end_time_valid)
	{
		msec_timeout = timeout2msec(timepar);
		sys_get_curr_time(&cur_time);
		add_int_to_abs_time(&cur_time, msec_timeout, &end_time);
	}
	sockintr->end_time_valid = FALSE;

	temp_1 = 1;
	do
	{
		if (1 != temp_1)
			tcp_routines.aa_close(socketptr->sd);
	        if (-1 == (socketptr->sd = tcp_routines.aa_socket(AF_INET, SOCK_STREAM, 0)))
        	{
                	errptr = (char *)STRERROR(errno);
			errlen = STRLEN(errptr);
                	rts_error(VARLSTCNT(5) ERR_SOCKINIT, 3, errno, errlen, errptr);
                	return FALSE;
        	}
		temp_1 = 1;
		if (-1 == tcp_routines.aa_setsockopt(socketptr->sd, SOL_SOCKET, SO_REUSEADDR, &temp_1, sizeof(temp_1)))
        	{
                	errptr = (char *)STRERROR(errno);
                	errlen = STRLEN(errptr);
                	rts_error(VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
				  RTS_ERROR_LITERAL("SO_REUSEADDR"), errno, errlen, errptr);
                	return FALSE;
        	}
#ifdef TCP_NODELAY
		temp_1 = socketptr->nodelay ? 1 : 0;
		if (-1 == tcp_routines.aa_setsockopt(socketptr->sd,
						     IPPROTO_TCP, TCP_NODELAY, &temp_1, sizeof(temp_1)))
        	{
                	errptr = (char *)STRERROR(errno);
                	errlen = STRLEN(errptr);
                	rts_error(VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
				  RTS_ERROR_LITERAL("TCP_NODELAY"), errno, errlen, errptr);
                	return FALSE;
        	}
#endif
		if (update_bufsiz)
		{
			if (-1 == tcp_routines.aa_setsockopt(socketptr->sd,
							     SOL_SOCKET, SO_RCVBUF, &socketptr->bufsiz, sizeof(socketptr->bufsiz)))
			{
				errptr = (char *)STRERROR(errno);
         			errlen = STRLEN(errptr);
                		rts_error(VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
					  RTS_ERROR_LITERAL("SO_RCVBUF"), errno, errlen, errptr);
				return FALSE;
			}
		} else
		{
			sockbuflen = sizeof(socketptr->bufsiz);
			if (-1 == tcp_routines.aa_getsockopt(socketptr->sd,
							     SOL_SOCKET, SO_RCVBUF, &socketptr->bufsiz, &sockbuflen))
			{
				errptr = (char *)STRERROR(errno);
         			errlen = STRLEN(errptr);
                		rts_error(VARLSTCNT(7) ERR_GETSOCKOPTERR, 5,
					  RTS_ERROR_LITERAL("SO_RCVBUF"), errno, errlen, errptr);
				return FALSE;
			}
		}
		temp_1 = tcp_routines.aa_connect(socketptr->sd,
						 (struct sockaddr *)&socketptr->remote.sin, sizeof(socketptr->remote.sin));
		if (temp_1 < 0)
		{
			real_errno = errno;
			no_time_left = TRUE;
			switch (real_errno)
			{
				case ETIMEDOUT	:	/* the other side bound but not listening */
				case ECONNREFUSED :
					if (NO_M_TIMEOUT != timepar)
					{
						sys_get_curr_time(&cur_time);
						cur_time = sub_abs_time(&end_time, &cur_time);
						if (cur_time.at_sec > 0)
							no_time_left = FALSE;
					}
					break;
				case EINTR :
					break;
				default:
					errptr = (char *)STRERROR(real_errno);
					errlen = STRLEN(errptr);
					rts_error(VARLSTCNT(6) ERR_OPENCONN, 0, ERR_TEXT, 2, errlen, errptr);
					break;
			}
			if (no_time_left)
				return FALSE;
#ifdef SOCK_INTR_CONNECT
			if (0 != outofband)
                        {
                                SOCKET_DEBUG(PRINTF("socconn: outofband interrupt received (%d) -- "
                                                    "queueing mv_stent for wait intr\n", outofband); DEBUGSOCKFLUSH);
				tcp_routines.aa_close(socketptr->sd);	/* Don't leave a dangling socket around */
                                PUSH_MV_STENT(MVST_ZINTDEV);
                                mv_chain->mv_st_cont.mvs_zintdev.io_ptr = iod;
                                mv_chain->mv_st_cont.mvs_zintdev.buffer_valid = FALSE;
                                sockintr->who_saved = sockwhich_connect;
                                sockintr->end_time = end_time;
                                sockintr->end_time_valid = TRUE;
                                dsocketptr->mupintr = TRUE;
                                socketus_interruptus++;
                                SOCKET_DEBUG(PRINTF("socconn: mv_stent queued - endtime: %d/%d  interrupts: %d\n",
                                                    end_time.at_sec, end_time.at_usec, socketus_interruptus);
                                             DEBUGSOCKFLUSH);
                                outofband_action(FALSE);
                                GTMASSERT;      /* Should *never* return from outofband_action */
                                return FALSE;   /* For the compiler.. */
                        }
#endif
			hiber_start(100);
		}
	} while (temp_1 < 0);

	/* handle the local information later.
	   SPRINTF(socketptr->local.saddr_ip, "%s", tcp_routines.aa_inet_ntoa(socketptr->remote.sin.sin_addr));
	   socketptr->local.port = GTM_NTOHS(socketptr->remote.sin.sin_port);
	*/
	socketptr->state = socket_connected;
	socketptr->first_read = socketptr->first_write = TRUE;
	/* update dollar_key */
        len = sizeof(ESTABLISHED) - 1;
        memcpy(&dsocketptr->dollar_key[0], ESTABLISHED, len);
        dsocketptr->dollar_key[len++] = '|';
        memcpy(&dsocketptr->dollar_key[len], socketptr->handle, socketptr->handle_len);
        len += socketptr->handle_len;
        dsocketptr->dollar_key[len++] = '|';
	strcpy(&dsocketptr->dollar_key[len], socketptr->remote.saddr_ip); /* Also copies in trailing null */

	return TRUE;
}
