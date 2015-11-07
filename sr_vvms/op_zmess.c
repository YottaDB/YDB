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
#include "fao_parm.h"
#include <descrip.h>
#include <ssdef.h>
#include <stdarg.h>
#include "mval2fao.h"
#include "op.h"
#include "tp_restart.h"

#define FAO_BUFFER_SPACE 2048
#define MAX_MSG_SIZE 256

void op_zmess(int4 errnum, ...)
{
	va_list		var;
	int4		status, cnt, faocnt;
	unsigned short	m_len;
	unsigned char	faostat[4];
	unsigned char	msgbuff[MAX_MSG_SIZE + 1];
	unsigned char	buff[FAO_BUFFER_SPACE];
	int4		fao[MAX_FAO_PARMS + 1];
	$DESCRIPTOR(d_sp, msgbuff);

	error_def(ERR_TPRETRY);

	VAR_START(var, errnum);
	va_count(cnt);
	cnt--;
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
			/* Currently there are a max of 20 fao parms (MAX_FAO_PARMS) allowed, hence passing upto fao_list[19].
			 * An assert is added to ensure this code is changed whenever the macro MAX_FAO_PARMS is changed.
			 * The # of arguments passed below should change accordingly.
			 */
			assert(MAX_FAO_PARMS == 20);
			if (ERR_TPRETRY == errnum)
			{	/* A TP restart is being signalled. Set t_fail_hist just like a TRESTART command would */
				op_trestart_set_cdb_code();
			}
			rts_error(VARLSTCNT(MAX_FAO_PARMS + 2) errnum, faocnt, fao[0], fao[1], fao[2], fao[3], fao[4], fao[5],
				fao[6], fao[7], fao[8], fao[9], fao[10], fao[11], fao[12], fao[13], fao[14], fao[15], fao[16],
				fao[17], fao[18], fao[19]);
		}
		return;
	} else
	{
		va_end(var);
		rts_error(VARLSTCNT(1) status);
	}
}
