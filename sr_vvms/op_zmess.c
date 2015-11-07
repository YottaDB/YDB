/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "fao_parm.h"
#include <descrip.h>
#include <ssdef.h>
#include <stdarg.h>
#include "mval2fao.h"
#include "op.h"
#include "tp_restart.h"

#define FAO_BUFFER_SPACE	2048
#define MAX_MSG_SIZE		256

error_def(ERR_TPRETRY);

void op_zmess(int4 errnum, ...)
{
	va_list		var;
	int4		status, cnt, faocnt;
	unsigned short	m_len;
	unsigned char	faostat[4];
	unsigned char	msgbuff[MAX_MSG_SIZE + 1];
	unsigned char	buff[FAO_BUFFER_SPACE];
	int4		fao[MAX_FAO_PARMS];
	$DESCRIPTOR(d_sp, msgbuff);

	VAR_START(var, errnum);
	va_count(cnt);
	cnt--;
	assert(34 == MAX_FAO_PARMS);			/* Defined in fao_parm.h. */
	status = sys$getmsg(errnum, &m_len, &d_sp, 0, &faostat[0]);
	if ((status & 1) && m_len)
	{
		buff[m_len] = 0;
		memset(&fao[0], 0, SIZEOF(fao));
		faocnt = (cnt ? faostat[1] : cnt);
		faocnt = (faocnt > MAX_FAO_PARMS ? MAX_FAO_PARMS : faocnt);
		if (faocnt)
			faocnt = mval2fao(msgbuff, var, &fao[0], cnt, faocnt, buff, buff + SIZEOF(buff));
		va_end(var);
		if (faocnt != -1)
		{
			if (ERR_TPRETRY == errnum)
			{	/* A TP restart is being signalled. Set t_fail_hist just like a TRESTART command would */
				op_trestart_set_cdb_code();
			}
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(MAX_FAO_PARMS + 2) errnum, faocnt, fao[0], fao[1], fao[2], fao[3],
				fao[4], fao[5], fao[6], fao[7], fao[8], fao[9], fao[10], fao[11], fao[12], fao[13], fao[14],
				fao[15], fao[16], fao[17], fao[18], fao[19], fao[20], fao[21], fao[22], fao[23], fao[24], fao[25],
				fao[26], fao[27], fao[28], fao[29], fao[30], fao[31], fao[32], fao[33]);
		}
		return;
	} else
	{
		va_end(var);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
	}
}
