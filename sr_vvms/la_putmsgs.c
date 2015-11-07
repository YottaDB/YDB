/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "msg.h"

/* la_putmsgs.c : outputs the message for the system status code */

 void la_putmsgs(c)
 int c ;
 {
	int k,e ;
	msgtype msgvec;

	msgvec.arg_cnt = 1;
	msgvec.def_opts = 0x000F;
	msgvec.msg_number = c;
	msgvec.fp_cnt = msgvec.new_opts = 0;
	sys$putmsg(&msgvec) ;
 }
