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

#ifndef __IOTCPDEF_H__
#define __IOTCPDEF_H__
#include "gtm_inet.h"
/* iotcpdef.h UNIX - TCP header file */

#define TCPDEF_WIDTH	255
#define TCPDEF_LENGTH	66

#define TCP_NOOP		0
#define TCP_WRITE		1
#define TCP_READ		2

#define SA_MAXLEN		32   /* SIZEOF(123.567.901.345,78901) */
#define SA_MAXLITLEN		128  /* maximun size of beowulf.sanchez.com */
#define DD_BUFLEN		80

#ifdef VMS
#define VMS_MAX_TCP_IO_SIZE	(64 * 1024 - 512) /* Hard limit for TCP send or recv size. On some implementations, the limit is
						   * 64K - 1, on others it is 64K - 512. We take the conservative approach and
						   * choose the lower limit
						   */
#endif

/*Definitions in case of ENOBUFs error in HPUX*/
#ifdef __hpux
#define HPUX_MAX_RETRIES 8
#define HPUX_SEL_TIMEOUT (20 * 1000) /*20 milliseconds(reperesented in micro secs)*/
#endif


#define DOTCPSEND(SDESC, SBUFF, SBUFF_LEN, SFLAGS, RC) \
{ \
	ssize_t		gtmioStatus; \
	size_t		gtmioBuffLen; \
	size_t		gtmioChunk; \
	sm_uc_ptr_t	gtmioBuff; \
	gtmioBuffLen = SBUFF_LEN; \
	gtmioBuff = (sm_uc_ptr_t)(SBUFF); \
	for (;;) \
        { \
		gtmioChunk = gtmioBuffLen VMS_ONLY(> VMS_MAX_TCP_IO_SIZE ? VMS_MAX_TCP_IO_SIZE : gtmioBuffLen); \
		if ((ssize_t)-1 != (gtmioStatus = tcp_routines.aa_send(SDESC, gtmioBuff, gtmioChunk, SFLAGS))) \
	        { \
			gtmioBuffLen -= gtmioStatus; \
			if (0 == gtmioBuffLen) \
				break; \
			gtmioBuff += gtmioStatus; \
	        } \
		else if (EINTR != errno) \
		  break; \
        } \
	if ((ssize_t)-1 == gtmioStatus)    	/* Had legitimate error - return it */ \
		RC = errno; \
	else if (0 == gtmioBuffLen) \
	        RC = 0; \
	else \
		RC = -1;		/* Something kept us from sending what we wanted */ \
}

/* ***************************************************** */
/* *********** structures for TCP driver *************** */
/* ***************************************************** */

typedef struct
{
	char	        saddr[SA_MAXLEN];	/* socket address */
	char		dollar_device[DD_BUFLEN];
	struct sockaddr_in	sin;		/* socket address + port */
	unsigned char	lastop;
	int		bufsiz;			/* OS internal buffer size */
	int		socket;			/* socket descriptor */
	int4		width;
	int4		length;
	bool		passive;		/* passive connection */
	bool		urgent;			/* urgent data mode */
}d_tcp_struct;	/*  tcp		*/

/* if ntohs/htons are macros, use them, otherwise, use the tcp_routines */

#define MAX_DELIM_BUFF  64
#ifdef  ntohs
# define GTM_NTOHS      ntohs
#else
# define GTM_NTOHS      tcp_routines.aa_ntohs
#endif
#ifdef  htons
# define GTM_HTONS      htons
#else
# define GTM_HTONS      tcp_routines.aa_htons
#endif

#endif
