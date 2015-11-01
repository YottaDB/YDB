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

#ifndef SEND_MSG_included
#define SEND_MSG_included

void send_msg(UNIX_ONLY(int arg_count) VMS_ONLY(int msg_id_arg), ...);

#endif /* SEND_MSG_included */
