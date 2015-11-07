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

#include "mdef.h"
#include "msg.h"
#include <stdarg.h>

GBLREF bool dec_nofac;

void dec_err(int4 msgnum, ...)
{
	va_list		var;
	uint4		i, count, argcnt;
	msgtype 	msg;

	VAR_START(var, msgnum);
	va_count(argcnt);
	msg.msg_number = msgnum;
	argcnt--;
	if (argcnt)
	{	count = va_arg(var, int4);
		assert (count <= argcnt);
	} else
		count = 0;

	count = (count > DEF_MSG_ARGS ? DEF_MSG_ARGS : count);
	msg.new_opts = msg.def_opts = dec_nofac;
	msg.arg_cnt = 2 + count; /* count + overhead */
	msg.fp_cnt = count;
	for (i = 0; i < count; i++)
		msg.fp[i].n = va_arg(var, int4);
	sys$putmsg(&msg,0,0,0);
	va_end(var);
}
