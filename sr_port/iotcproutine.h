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

/* pointers for tcp/ip routines */
typedef struct
{
	int               (*aa_accept)();
	int               (*aa_bind)();
	int               (*aa_close)();
	int               (*aa_connect)();
	int               (*aa_getsockopt)();
	int               (*aa_getsockname)();
	int               (*aa_listen)();
	int               (*aa_recv)();
	int               (*aa_select)();
	int               (*aa_send)();
	int               (*aa_setsockopt)();
	int               (*aa_shutdown)();
	int               (*aa_socket)();
	bool               using_tcpware;      /* use tcpware(1) or ucx(0) */
}tcp_library_struct;

