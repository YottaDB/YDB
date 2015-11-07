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

/* gtcm_ping.c - routines providing the capability of pinging remote
 * 		 machines.
 */
#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_stdlib.h"		/* for exit() */
#include "gtm_unistd.h"		/* for getpid() */

#include <errno.h>
#if defined(sun) || defined(mips)
#include <sys/time.h>
#else
#include "gtm_time.h"
#endif
#include <sys/types.h>
#include "gtm_socket.h"
#include "gtm_netdb.h"
#include "gtm_ipv6.h"

#include "gtm_inet.h"
#ifndef __MVS__
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#endif
#include "omi.h"
#include "gtmio.h"
#include "have_crit.h"

#if (defined(__CYGWIN__) || defined(__MVS__)) && !defined(ICMP_ECHO)
/* Note that we may actually need to use Windows sockets for icmp for now */

struct icmp
{
   u_char icmp_type;
   u_char icmp_code;
   u_char icmp_cksum;
   u_char icmp_id;
   u_char icmp_seq;
   u_char icmp_data[1];
};

#ifndef ICMP_ECHOREPLY
#define ICMP_ECHOREPLY 0
#endif

#ifndef ICMP_ECHO
#define ICMP_ECHO 8
#endif

#ifndef ICMP_MINLEN
#define ICMP_MINLEN 8
#endif

#ifndef IP_MAXPACKET
#define IP_MAXPACKET 65535
#endif
#endif /* __CYGWIN__ || __MVS */

#ifdef __MVS__

error_def(ERR_GETNAMEINFO);

struct ip
  {
    unsigned int ip_v:4;		/* version */
    unsigned int ip_hl:4;		/* header length */
    unsigned char ip_tos;		/* type of service */
    u_short ip_len;			/* total length */
    u_short ip_id;			/* identification */
    u_short ip_off;			/* fragment offset field */
    unsigned char ip_ttl;		/* time to live */
    unsigned char ip_p;			/* protocol */
    u_short ip_sum;			/* checksum */
    struct in_addr ip_src, ip_dst;	/* source and dest address */
  };
#endif

static int pingsock = -1, ident;
static char pingrcv[IP_MAXPACKET], pingsend[256];

/* init_ping
 * Set up a raw socket to ping machines
 * Returns the socket id of the "ping" socket.
 */
int init_ping(void)
{
    struct protoent *proto;

    ident = getpid() & 0xFFFF;
    if (!(proto = getprotobyname("icmp")))
    {
	    FPRINTF(stderr, "ping: unknown protocol icmp.\n");
	    pingsock = -1;
	    return pingsock;
    }
    if ((pingsock = socket(AF_INET, SOCK_RAW, proto->p_proto)) < 0) {
	    perror("ping: socket");
	    if (errno == EACCES)
		    OMI_DBG((omi_debug,"You must run this program as root in order to use the -ping option.\n"));
	    pingsock = -1;
    }
    return pingsock;
}

/* icmp_ping --
 * 	Compose and transmit an ICMP ECHO REQUEST packet.  The IP packet
 * will be added on by the kernel.  The ID field is our UNIX process ID,
 * and the sequence number is an ascending integer.  The first 8 bytes
 * of the data portion are used to hold a UNIX "timeval" struct in VAX
 * byte-order, to compute the round-trip time.
 */
int icmp_ping(int conn)
{
	fd_set			fdmask;
	struct sockaddr_storage	paddr;
	GTM_SOCKLEN_TYPE	paddr_len = SIZEOF(struct sockaddr_storage);
	struct icmp		*icp;
	struct ip		*ip;
	struct timeval		timeout;
	int			cc;

	if (pingsock < 0)
	{
		FPRINTF(stderr,"icmp_ping:  no ping socket.\n");
		exit(1);
	}
	if (getpeername(conn, (struct sockaddr *)&paddr, (GTM_SOCKLEN_TYPE *)&paddr_len) < 0)
	{
		perror("getpeername");
		return -1;	/* to denote error return */
	}
	icp = (struct icmp *)pingsend;
	icp->icmp_type = ICMP_ECHO;
	icp->icmp_code = 0;
	icp->icmp_cksum = 0;
	icp->icmp_seq = conn;
	icp->icmp_id = ident;				/* ID */
	{
		time_t time_val;
		time(&time_val);
		*((int *)(&pingsend[ICMP_MINLEN])) = (int) time_val;	/* time stamp */
	}
	/* compute ICMP checksum here */
	icp->icmp_cksum = in_cksum((u_short *)icp, ICMP_MINLEN + SIZEOF(int));
	while (cc = sendto(pingsock, (char *)pingsend, ICMP_MINLEN + SIZEOF(int), 0, (struct sockaddr *)&paddr, paddr_len) < 0)
	{
		if (errno == EINTR)
			continue;
		perror("ping: sendto");
		continue;
	}
#ifdef DEBUG_PING
	{
		char host[SA_MAXLEN];
		struct hostent *he;
		if (0 != (errcode = getnameinfo((struct sockaddr *)&paddr, paddr_len, host, SA_MAXLEN, NULL, 0 0)))
		{
			assert(FALSE);
			RTS_ERROR_ADDRINFO(NULL, ERR_GETNAMEINFO, errcode);
			return FALSE;
		}
		OMI_DBG((omi_debug, "ping: send to %s\n",host));
	}
#endif
	return 1;	/* No Error. Added to make compiler happy */
}

/* get_ping_rsp
 * Read the ping socket and determine the packet type.
 *
 * Returns:  The sequence field in the incoming ECHO_REPLY packet, or -1
 *           if the packet is not a response to one of our pings.
 */
int get_ping_rsp(void)
{
	struct sockaddr_storage from;
	register int cc;
	GTM_SOCKLEN_TYPE fromlen;
	struct icmp *icp;
	struct ip *ip;

	if (pingsock < 0)
	{
		FPRINTF(stderr,"icmp_ping:  no ping socket.\n");
		exit(1);
	}
	/* SIZEOF() does not provide correct fromlen.
	 * fromlen in fact decided by recvfrom() below, so no need getaddrinfo() is needed to obtain the correct fromlen
	 */
	fromlen = SIZEOF(from);
	while ((cc = (int)(recvfrom(pingsock, (char *)pingrcv, IP_MAXPACKET, 0, (struct sockaddr *)&from,
					(GTM_SOCKLEN_TYPE *)&fromlen))) < 0)
	{
		if (errno == EINTR)
			continue;
		perror("ping: recvfrom");
		continue;
	}
	ip = (struct ip *) pingrcv;
	icp = (struct icmp *)(pingrcv + ((ip->ip_hl) << 2));
	/* xxxxxxx icp = (struct icmp *)(pingrcv + (ip->ip_hl << 2)); */
#ifdef DEBUG_PING
	{
		char host[SA_MAXLEN];
		struct hostent *he;
		if (0 != (errcode = getnameinfo((struct sockaddr *)&from, fromlen, host, SA_MAXLEN, NULL, 0 0)))
		{
			assert(FALSE);
			RTS_ERROR_ADDRINFO(NULL, ERR_GETNAMEINFO, errcode);
			return FALSE;
		}
		OMI_DBG((omi_debug, "ping: response from %s\n",host));
	}
#endif
	/* check to see if it is a reply and if it belongs to us */
	if (icp->icmp_type == ICMP_ECHOREPLY && icp->icmp_id == ident)
		return icp->icmp_seq;
	else
		return -1;
}

/* in_cksum --
 *	Checksum routine for Internet Protocol family headers (C Version)
 */
int in_cksum(u_short *addr, int len)
{
	register int nleft = len;
	register u_short *w = addr;
	register int sum = 0;
	u_short answer = 0;

	/* Our algorithm is simple, using a 32 bit accumulator (sum), we add
	 * sequential 16 bit words to it, and at the end, fold back all the
	 * carry bits from the top 16 bits into the lower 16 bits.
	 */
	while (nleft > 1)  {
		sum += *w++;
		nleft -= 2;
	}
	/* mop up an odd byte, if necessary */
	if (nleft == 1) {
		*(u_char *)(&answer) = *(u_char *)w ;
		sum += answer;
	}
	/* add back carry outs from top 16 bits to low 16 bits */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;				/* truncate to 16 bits */
	return(answer);
}
