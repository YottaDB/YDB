/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* pointers for tcp/ip routines */
typedef struct
{
        int               (*aa_accept)();
        int               (*aa_bind)();
        int               (*aa_close)();
        int               (*aa_connect)();
        int               (*aa_getsockopt)();
	int               (*aa_getsockname)();
        unsigned short    (*aa_htons)(in_port_t);
/* smw 1999/12/15 STDC is not a good flag to use so why is it here
		  perhaps should define in_addr_t somewhere if needed. */
#if !defined(__STDC__)
	uint4             (*aa_inet_addr)();
#else
	in_addr_t         (*aa_inet_addr)(const char *);
#endif
        char              *(*aa_inet_ntoa)();
        unsigned short    (*aa_ntohs)(in_port_t);
        int               (*aa_listen)();
        int               (*aa_recv)();
        int               (*aa_select)();
        int               (*aa_send)();
        int               (*aa_setsockopt)();
        int               (*aa_shutdown)();
        int               (*aa_socket)();
        bool               using_tcpware;      /* use tcpware(1) or ucx(0) */
}tcp_library_struct;

