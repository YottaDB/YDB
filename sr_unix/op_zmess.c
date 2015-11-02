/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>

#include "fao_parm.h"
#include "error.h"
#include "op.h"
#include "mval2fao.h"
#include "tp_restart.h"

#define FAO_BUFFER_SPACE 2048

void op_zmess(unsigned int cnt, ...)
{
	va_list		var;
	const err_ctl	*ectl;
	const err_msg	*eptr;
	UINTPTR_T	fao[MAX_FAO_PARMS];
	char		buff[FAO_BUFFER_SPACE];
	unsigned int	errnum, j;
	int		faocnt;

	error_def(ERR_TPRETRY);

	VAR_START(var, cnt);
	errnum = va_arg(var, int);
	cnt--;
	if (ectl = err_check(errnum))
	{
		assert((errnum & FACMASK(ectl->facnum)) && (MSGMASK(errnum, ectl->facnum) <= ectl->msg_cnt));
		j = MSGMASK(errnum, ectl->facnum);
		eptr = ectl->fst_msg + j - 1;

		faocnt = eptr->parm_count;
		faocnt = (faocnt > MAX_FAO_PARMS ? MAX_FAO_PARMS : faocnt);
		faocnt = mval2fao(eptr->msg, var, &fao[0], cnt, faocnt, buff, buff + SIZEOF(buff));
		va_end(var);
		if (faocnt != -1)
		{
			if (ERR_TPRETRY == errnum)
			{	/* A TP restart is being signalled. Set t_fail_hist just like a TRESTART command would */
				op_trestart_set_cdb_code();
			}
			rts_error(VARLSTCNT(faocnt+2) errnum, faocnt, fao[0], fao[1], fao[2], fao[3], fao[4], fao[5], fao[6],
				fao[7], fao[8], fao[9], fao[10], fao[11]);
		}
	} else
	{
		va_end(var);
		rts_error(VARLSTCNT(1) errnum);
	}
}

