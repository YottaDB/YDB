/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Servcies, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <stdarg.h>
#include "msg.h"

#define MAX_ERR_ARGS	64

static int4 msgv[MAX_ERR_ARGS + 1];

void gtm_putmsg(int4 msgid, ...)
{
	va_list		var;
	short int	*sptr;
	int4		argcnt, *lptr;

	VAR_START(var, msgid);
	va_count(argcnt);
	if (MAX_ERR_ARGS < argcnt)
		argcnt = MAX_ERR_ARGS;
	sptr = (short int *)msgv;
	*sptr= argcnt;
	sptr++;
	*sptr= 0;
	assert(0 < argcnt);
	msgv[1] = msgid;
	argcnt--;
	for (lptr = &msgv[2]; argcnt; *lptr++ = va_arg(var, int4), argcnt--)
		;
	va_end(var);
	sys$putmsg(msgv, 0, 0, 0);
	return;
}
