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
#include "op.h"
#ifndef EARLY_VARARGS
#include <varargs.h>
#endif
#include "mval2fao.h"

#define FAO_BUFFER_SPACE 2048

void op_zmess(va_alist)
va_dcl
{
	va_list var;
	err_ctl	*ectl;
	err_msg *eptr;
	int fao[MAX_FAO_PARMS];
	char	buff[FAO_BUFFER_SPACE];
	unsigned int	errnum, cnt, j;
	int	faocnt;

	VAR_START(var);
	cnt = va_arg(var, int);
	errnum = va_arg(var, int);
	cnt--;
	if (ectl = err_check(errnum))
	{
		assert((errnum & FACMASK(ectl->facnum)) && (MSGMASK(errnum, ectl->facnum) <= ectl->msg_cnt));
		j = MSGMASK(errnum, ectl->facnum);
		eptr = ectl->fst_msg + j - 1;

		faocnt = eptr->parm_count;
		faocnt = (faocnt > MAX_FAO_PARMS ? MAX_FAO_PARMS : faocnt);
		faocnt = mval2fao(eptr->msg, var, &fao[0], cnt, faocnt, buff, buff + sizeof(buff));
		if (faocnt != -1)
			rts_error(VARLSTCNT(faocnt+2) errnum, faocnt, fao[0], fao[1], fao[2], fao[3], fao[4], fao[5], fao[6],
				fao[7], fao[8], fao[9], fao[10], fao[11]);
		return;
	}
	else
	{	rts_error(VARLSTCNT(1) errnum);
		return;
	}
}


