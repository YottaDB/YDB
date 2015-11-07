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

#ifndef __IOTCPDEF_H__
#define __IOTCPDEF_H__
#include "gtm_inet.h"
#include "gtm_socket.h" /* for NI_MAXHOST */
#include "gtm_netdb.h"
/* iotcpdef.h UNIX - TCP header file */

#define TCPDEF_WIDTH	255
#define TCPDEF_LENGTH	66

#define TCP_NOOP		0
#define TCP_WRITE		1
#define TCP_READ		2

#define SA_MAXLEN	NI_MAXHOST	/* NI_MAXHOST is 1025, large enough to hold any IPV6 address format
					 	 * e.g.(123:567:901:345:215:0:0:0)
						 */
#define SA_MAXLITLEN	NI_MAXHOST	/* large enough to hold any host name, e.g.
						 * host name google: dfw06s16-in-x12.1e100.net
						 */
#define USR_SA_MAXLITLEN	128  /* maximum size of host GTM user can specify
					      * the reason why the number is so small is because the host name size
					      * is stored as one byte in socket parameter list (refer to iosocket_use)
					      */


#ifdef VMS
#define VMS_MAX_TCP_IO_SIZE	(64 * 1024 - 512) /* Hard limit for TCP send or recv size. On some implementations, the limit is
						   * 64K - 1, on others it is 64K - 512. We take the conservative approach and
						   * choose the lower limit
						   */
#endif

#define DOTCPSEND(SDESC, SBUFF, SBUFF_LEN, SFLAGS, RC) 								\
{ 														\
	ssize_t		gtmioStatus; 										\
	size_t		gtmioBuffLen; 										\
	size_t		gtmioChunk; 										\
	sm_uc_ptr_t	gtmioBuff; 										\
														\
	gtmioBuffLen = SBUFF_LEN; 										\
	gtmioBuff = (sm_uc_ptr_t)(SBUFF); 									\
	for (;;) 												\
        { 													\
		gtmioChunk = gtmioBuffLen VMS_ONLY(> VMS_MAX_TCP_IO_SIZE ? VMS_MAX_TCP_IO_SIZE : gtmioBuffLen); \
		if ((ssize_t)-1 != (gtmioStatus = tcp_routines.aa_send(SDESC, gtmioBuff, gtmioChunk, SFLAGS))) 	\
	        { 												\
			gtmioBuffLen -= gtmioStatus; 								\
			if (0 == gtmioBuffLen) 									\
				break; 										\
			gtmioBuff += gtmioStatus; 								\
	        } 												\
		else if (EINTR != errno) 									\
		  break; 											\
        } 													\
	if ((ssize_t)-1 == gtmioStatus)    	/* Had legitimate error - return it */ 				\
		RC = errno; 											\
	else if (0 == gtmioBuffLen) 										\
	        RC = 0; 											\
	else 													\
		RC = -1;		/* Something kept us from sending what we wanted */ 			\
}

/* ***************************************************** */
/* *********** structures for TCP driver *************** */
/* ***************************************************** */

typedef struct
{
	char	        saddr[SA_MAXLEN];	/* socket address */
	struct sockaddr_storage	sas;		/* socket address + port */
	struct addrinfo		ai;
	unsigned char	lastop;
	int		bufsiz;			/* OS internal buffer size */
	int		socket;			/* socket descriptor */
	int4		width;
	int4		length;
	bool		passive;		/* passive connection */
	bool		urgent;			/* urgent data mode */
} d_tcp_struct;	/*  tcp		*/

#endif
