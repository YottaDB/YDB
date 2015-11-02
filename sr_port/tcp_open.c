/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 * tcp_open.
 *
 *      parameters:
 *		     host - name like beowulf.sanchez.com or NULL
 *		     port - numeric port number
 *		     timeout - numeric seconds
 *		     passive - boolean 0 is sender and must have a host, 1 may
 *		               have a host and if so, it is checked to see that
 *		               incomming connections are from that host.
 *      return:
 *                  socket descriptor for the connection or
 *                  -1 and output the error.
 *
 * Note that on error return errno may not be at the same value as the error condition.
 *
 */

#include "mdef.h"

#include <errno.h>
#include "gtm_time.h"
#include "gtm_socket.h"
#include "gtm_inet.h"
#include "gtm_string.h"
#include "gtm_ctype.h"
#include "gtm_stdio.h"
#include "gtm_netdb.h"

#include "copy.h"
#include "gt_timer.h"
#include "io.h"
#include "iotimer.h"
#include "iotcp_select.h"
#include "iotcpdef.h"
#include "iotcproutine.h"
#include "io_params.h"
#include "util.h"
#include "gtmmsg.h"

#define	MAX_CONN_PENDING	5
#define	NAP_LENGTH		300

#ifdef __osf__
/* Tru64 does not have the prototype for "hstrerror" even though the function is available in the library.
 * Until we revamp the TCP communications setup stuff to use the new(er) POSIX definitions, we cannot move
 * away from "hstrerror". Declare prototype for this function in Tru64 manually until then.
 */
const char *hstrerror(int err);
#endif

GBLREF tcp_library_struct       tcp_routines;

int tcp_open(char *host, unsigned short port, int4 timeout, boolean_t passive) /* host needs to be NULL terminated */
{
	boolean_t		no_time_left = FALSE, error_given = FALSE;
	char			temp_addr[SA_MAXLEN + 1], addr[SA_MAXLEN + 1];
	char 			*from, *to, *errptr, *temp_ch, *adptr;
	int			match, sock, sendbufsize, ii, on = 1, temp_1 = -2;
	GTM_SOCKLEN_TYPE	size;
	int4                    rv, msec_timeout;
	struct	sockaddr_in	sin;
	in_addr_t		temp_sin_addr;
	char                    msg_buffer[1024];
	mstr                    msg_string;
	ABS_TIME                cur_time, end_time;
	fd_set                  tcp_fd;
	struct sockaddr_in      peer;
	short 			retry_num;
	int 			save_errno, errlen;
	const char		*terrptr;

	error_def(ERR_INVADDRSPEC);
	error_def(ERR_SYSCALL);
	error_def(ERR_IPADDRREQ);
	error_def(ERR_SOCKINIT);
	error_def(ERR_SETSOCKOPTERR);
  	error_def(ERR_SOCKLISTEN);
  	error_def(ERR_SOCKACPT);
  	error_def(ERR_TEXT);

	temp_sin_addr = 0;
	msg_string.len = SIZEOF(msg_buffer);
	msg_string.addr = msg_buffer;
	memset((char *)&sin, 0, SIZEOF(struct sockaddr_in));

	/* ============================= initialize structures ============================== */
	if (NULL != host)
	{
		temp_ch = host;
		while(ISDIGIT_ASCII(*temp_ch) || ('.' == *temp_ch))
			temp_ch++;
		if ('\0' != *temp_ch)
		{
			adptr = iotcp_name2ip(host);
			if (NULL == adptr)
			{
#if !defined(__hpux) && !defined(__MVS__)
				terrptr = HSTRERROR(h_errno);
				rts_error(VARLSTCNT(6) ERR_INVADDRSPEC, 0, ERR_TEXT, 2, LEN_AND_STR(terrptr));
#else
				/* Grumble grumble HPUX and z/OS don't have hstrerror() */
				rts_error(VARLSTCNT(1) ERR_INVADDRSPEC);
#endif
				return -1;
			}
			SPRINTF(addr, "%s", adptr);
		} else
			SPRINTF(addr, "%s", host);

		if ((unsigned int)-1 == (temp_sin_addr = tcp_routines.aa_inet_addr(addr)))
		{
			gtm_putmsg(VARLSTCNT(1) ERR_INVADDRSPEC);
			assert(FALSE);
			return  -1;
		}
	}

	if (passive)
		/* We can only listen on our own system */
		sin.sin_addr.s_addr = INADDR_ANY;
	else
	{
		if (0 == temp_sin_addr)
		{ 	/* If no address was specified */
			gtm_putmsg(VARLSTCNT(1) ERR_IPADDRREQ);
			assert(FALSE);
			return -1;
		}
		/* Set where to send the connection attempt */
		sin.sin_addr.s_addr = temp_sin_addr;
	}
	sin.sin_port = GTM_HTONS(port);
	sin.sin_family = AF_INET;


	/* ============================== do the connection ============================== */

	if (passive)
	{
		struct timeval  utimeout, save_utimeout;
		int 		lsock;

		lsock = tcp_routines.aa_socket(AF_INET, SOCK_STREAM, 0);
		if (-1 == lsock)
		{
			save_errno = errno;
			errptr = (char *)STRERROR(save_errno);
         		errlen = STRLEN(errptr);
			gtm_putmsg(VARLSTCNT(5) ERR_SOCKINIT, 3, save_errno, errlen, errptr);
			assert(FALSE);
			return -1;
		}
		/* allow multiple connections to the same IP address */
		if (-1 == tcp_routines.aa_setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, &on, SIZEOF(on)))
		{
			save_errno = errno;
			(void)tcp_routines.aa_close(lsock);
			errptr = (char *)STRERROR(save_errno);
         		errlen = STRLEN(errptr);
                	gtm_putmsg(VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
					RTS_ERROR_LITERAL("SO_REUSEADDR"), save_errno, errlen, errptr);
			assert(FALSE);
			return -1;
		}
		if (-1 == tcp_routines.aa_bind(lsock, (struct sockaddr *)&sin, SIZEOF(struct sockaddr)))
		{
			save_errno = errno;
			(void)tcp_routines.aa_close(lsock);
			gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5,
				   RTS_ERROR_LITERAL("bind()"), CALLFROM, save_errno);
			return -1;
		}
		/* establish a queue of length MAX_CONN_PENDING for incoming connections */
		if (-1 == tcp_routines.aa_listen(lsock, MAX_CONN_PENDING))
		{
			save_errno = errno;
			(void)tcp_routines.aa_close(lsock);
			errptr = (char *)STRERROR(save_errno);
			errlen = STRLEN(errptr);
			gtm_putmsg(VARLSTCNT(6) ERR_SOCKLISTEN, 0, ERR_TEXT, 2, errlen, errptr);
			assert(FALSE);
			return -1;
		}

		if (NO_M_TIMEOUT != timeout)
		{
			msec_timeout = timeout2msec(timeout);
			sys_get_curr_time(&cur_time);
			add_int_to_abs_time(&cur_time, msec_timeout, &end_time);
			utimeout.tv_sec = timeout;
			utimeout.tv_usec = 0;
		}
		while(1)
		{
			while(1)
			{
				/*
				 * the check for EINTR below is valid and should not be converted to an EINTR
				 * wrapper macro, since it might be a timeout.
				 */
				FD_ZERO(&tcp_fd);
				FD_SET(lsock, &tcp_fd);
                                save_utimeout = utimeout;
				rv = tcp_routines.aa_select(lsock + 1, (void *)&tcp_fd, (void *)0, (void *)0,
					(NO_M_TIMEOUT == timeout) ? (struct timeval *)0 : &utimeout);
				save_errno = errno;
                                utimeout = save_utimeout;
				if ((0 <= rv) || (EINTR != save_errno))
					break;
				if (NO_M_TIMEOUT != timeout)
				{
					sys_get_curr_time(&cur_time);
					cur_time = sub_abs_time(&end_time, &cur_time);
					if (0 >= (utimeout.tv_sec = cur_time.at_sec))
					{
						rv = 0;
						break;
					}
				}
			}
			if (0 == rv)
			{
				(void)tcp_routines.aa_close(lsock);
				util_out_print("Listening timed out.\n", TRUE);
				assert(FALSE);
				return -1;
			} else  if (0 > rv)
			{
				(void)tcp_routines.aa_close(lsock);
				gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5,
					   RTS_ERROR_LITERAL("select()"), CALLFROM, save_errno);
				assert(FALSE);
				return -1;
			}
			size = SIZEOF(struct sockaddr_in);
			sock = tcp_routines.aa_accept(lsock, &peer, &size);
			save_errno = errno;
			if (-1 == sock)
			{
#ifdef __hpux
			/*ENOBUFS in HP-UX is either because of a memory problem or when we have received a RST just
			after a SYN before an accept call. Normally this is not fatal and is just a transient state.
			Hence exiting just after a single error of this kind should not be done. So retry in case
			of HP-UX and ENOBUFS error. */
				if (ENOBUFS == save_errno)
				{
					retry_num = 0;
					while (HPUX_MAX_RETRIES > retry_num)
					{
				/*In case of succeeding with select in first go, accept will still get 5ms time difference*/
						SHORT_SLEEP(5);
						for ( ; HPUX_MAX_RETRIES > retry_num; retry_num++)
						{
							utimeout.tv_sec = 0;
							utimeout.tv_usec = HPUX_SEL_TIMEOUT;
							FD_ZERO(&tcp_fd);
							FD_SET(lsock, &tcp_fd);
							rv = tcp_routines.aa_select(lsock + 1, (void *)&tcp_fd, (void *)0,
							 (void *)0, &utimeout);
							save_errno = errno;
							if (0 < rv)
								break;
							else
								SHORT_SLEEP(5);
						}
						if (0 > rv)
			                        {
                			                (void)tcp_routines.aa_close(lsock);
							gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5,
								   RTS_ERROR_LITERAL("select()"),
								   CALLFROM, save_errno);
							assert(FALSE);
							return -1;
	                		        }
						if (0 == rv)
						{
							(void)tcp_routines.aa_close(lsock);
							util_out_print("Select timed out.\n", TRUE);
							assert(FALSE);
							return -1;
						}
						sock = tcp_routines.aa_accept(lsock, &peer, &size);
						save_errno = errno;
						if ((-1 == sock) && (ENOBUFS  == save_errno))
							retry_num++;
						else
							break;
					}
				}
				if (-1 == sock)
#endif
				{
					(void)tcp_routines.aa_close(lsock);
					errptr = (char *)STRERROR(save_errno);
					errlen = STRLEN(errptr);
					gtm_putmsg(VARLSTCNT(6) ERR_SOCKACPT, 0, ERR_TEXT, 2, errlen, errptr);
					assert(FALSE);
					return -1;
				}
			}
			SPRINTF(&temp_addr[0], "%s", tcp_routines.aa_inet_ntoa(peer.sin_addr));
#ifdef	DEBUG_ONLINE
			PRINTF("Connection is from : %s\n", &temp_addr[0]);
#endif
			/* Check if connection is from whom we want it to be from. Note that this is not a robust check
			   (potential for multiple IP addrs for a resolved name but workarounds for this exist so not a lot
			   of effort has been expended here at this time. Especially since the check is easily spoofed with
			   raw sockets anyway. It is more for the accidental "oops" type check than serious security..
			*/
			if ((0 == temp_sin_addr) || (0 == memcmp(&addr[0], &temp_addr[0], strlen(addr))))
				break;
			else
			{	/* Connection not from expected host */
				(void)tcp_routines.aa_close(sock);
				if (NO_M_TIMEOUT != timeout)
				{
					sys_get_curr_time(&cur_time);
					cur_time = sub_abs_time(&end_time, &cur_time);
					utimeout.tv_sec = ((cur_time.at_sec > 0) ? cur_time.at_sec : 0);
				}
				if (!error_given)
				{
					util_out_print("Connection from !AD rejected and ignored", TRUE,
						       LEN_AND_STR(&temp_addr[0]));
					error_given = TRUE;
				}
			}
		}
		(void)tcp_routines.aa_close(lsock);
	} else
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
				tcp_routines.aa_close(sock);
			sock = tcp_routines.aa_socket(AF_INET, SOCK_STREAM, 0);
			if (-1 == sock)
			{
				save_errno = errno;
				errptr = (char *)STRERROR(save_errno);
				errlen = STRLEN(errptr);
				gtm_putmsg(VARLSTCNT(5) ERR_SOCKINIT, 3, save_errno, errlen, errptr);
				assert(FALSE);
				return -1;
			}
			/*      allow multiple connections to the same IP address */
			if      (-1 == tcp_routines.aa_setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, SIZEOF(on)))
			{
				save_errno = errno;
				(void)tcp_routines.aa_close(sock);
				errptr = (char *)STRERROR(save_errno);
				errlen = STRLEN(errptr);
				gtm_putmsg(VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
					   RTS_ERROR_LITERAL("SO_REUSEADDR"), save_errno, errlen, errptr);
				assert(FALSE);
				return -1;
			}
			temp_1 = tcp_routines.aa_connect(sock, (struct sockaddr *)(&sin), SIZEOF(sin));
			save_errno = errno;
			/*
			 * the check for EINTR below is valid and should not be converted to an EINTR
			 * wrapper macro, because other error conditions are checked, and a retry is not
			 * immediately performed.
			 */
			if ((0 > temp_1) && (ECONNREFUSED != save_errno) && (EINTR != save_errno))
			{
				(void)tcp_routines.aa_close(sock);
				gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5,
					   RTS_ERROR_LITERAL("connect()"), CALLFROM, save_errno);
				assert(FALSE);
				return -1;
			}
			if ((0 > temp_1) && (EINTR == save_errno))
			{
				(void)tcp_routines.aa_close(sock);
				util_out_print("Interrupted.", TRUE);
				assert(FALSE);
				return -1;
			}
			if ((temp_1 < 0) && (NO_M_TIMEOUT != timeout))
			{
				sys_get_curr_time(&cur_time);
				cur_time = sub_abs_time(&end_time, &cur_time);
				if (cur_time.at_sec <= 0)
					no_time_left = TRUE;
			}
			SHORT_SLEEP(NAP_LENGTH);               /* Sleep for NAP_LENGTH ms */
		} while ((FALSE == no_time_left) && (0 > temp_1));

		if (0 > temp_1) /* out of time */
		{
			tcp_routines.aa_close(sock);
			util_out_print("Connection timed out.", TRUE);
			assert(FALSE);
			return -1;
		}
	}

	return sock;
}
