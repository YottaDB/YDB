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

#include "mdef.h"
#ifdef EARLY_VARARGS
#include <varargs.h>
#endif
#include "fao_parm.h"
#include "error.h"
#include "msg.h"
#ifndef EARLY_VARARGS
#include <varargs.h>
#endif
#include "util.h"
#include "util_out_print_vaparm.h"
#include "gtmmsg.h"

GBLDEF bool dec_nofac;

#define	NOFLUSH		0
#define FLUSH		1
#define RESET		2
#define OPER		4
#define SPRINT		5

void dec_err(va_alist)
va_dcl
{
	va_list		var;
	uint4		i, j, count, argcnt, err;
	err_ctl 	*ec;
	err_msg		*em;
	char		msgbuff[2048];
	mstr		msgstr;

	util_out_print(0, RESET, 0);	/* reset the buffer */
	VAR_START(var);
	argcnt = va_arg(var, int4);
	assert (argcnt >= 1);
	err = va_arg(var, uint4);
	ec = err_check(err);
	em = NULL;
	if (ec)
	{
		assert((err & FACMASK(ec->facnum)) && (MSGMASK(err, ec->facnum) <= ec->msg_cnt));
		j = MSGMASK(err, ec->facnum);
		em = ec->fst_msg + j - 1;
	}
	msgstr.addr = msgbuff;
	msgstr.len = sizeof(msgbuff);
	gtm_getmsg(err, &msgstr);

	if (!em)
		util_out_print(msgstr.addr, FLUSH, 1, err);
	else
	{
		argcnt--;
		if (argcnt)
		{
			count = va_arg(var, int4);
			assert (count <= argcnt);
		}
		else
			count = 0;
		util_out_print_vaparm(msgstr.addr, FLUSH, var, count);
	}
}
