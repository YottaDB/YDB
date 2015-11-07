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

#ifndef SEND_MSG_included
#define SEND_MSG_included

#if defined(UNIX)
void send_msg(int arg_count, ...);
void send_msg_csa(void *csa, int arg_count, ...);	/* Use CSA_ARG(CSA) for portability */
#elif defined(VMS)
void send_msg(int msg_id_arg, ...);
#define send_msg_csa	send_msg
#endif

#endif /* SEND_MSG_included */
