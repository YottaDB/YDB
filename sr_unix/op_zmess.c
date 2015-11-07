/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "gtmmsg.h"
#include "send_msg.h"
#include "wbox_test_init.h"

#define ZMESS_DISALLWD_LIST_SIZE		10
#define FAO_BUFFER_SPACE			2048
#define MAX_ERR_MSG_LEN				256

STATICFNDCL boolean_t is_disallowed(unsigned int errnum);

STATICDEF unsigned int zmess_disallowed_list[ZMESS_DISALLWD_LIST_SIZE] = {0};

error_def(ERR_REPEATERROR);
error_def(ERR_TPRETRY);
error_def(ERR_JOBINTRRQST);
error_def(ERR_JOBINTRRETHROW);
error_def(ERR_UNSOLCNTERR);
error_def(ERR_CTRLY);
error_def(ERR_CTRLC);
error_def(ERR_CTRAP);
error_def(ERR_STACKCRIT);
error_def(ERR_SPCLZMSG);

/* Returns whether an errnum is not allowed to be raised by ZMESSAGE. The errors on this list generally
 * trigger additional processing by the error handler which either assumes certain context to be setup
 * before the processing or does something which should not be triggered manually.
 */
STATICFNDEF boolean_t is_disallowed(unsigned int errnum)
{
	int i;	/* search iterator */

	if (0 == zmess_disallowed_list[0])
	{	/* Lazy initialization of the disallowed array. The ERR_XXXX take value at runtime, hence individual assignments. */
		i = 0;
		zmess_disallowed_list[i++] = ERR_REPEATERROR;
		zmess_disallowed_list[i++] = ERR_TPRETRY;
		zmess_disallowed_list[i++] = ERR_JOBINTRRQST;
		zmess_disallowed_list[i++] = ERR_JOBINTRRETHROW;
		zmess_disallowed_list[i++] = ERR_UNSOLCNTERR;
		zmess_disallowed_list[i++] = ERR_CTRLY;
		zmess_disallowed_list[i++] = ERR_CTRLC;
		zmess_disallowed_list[i++] = ERR_CTRAP;
		zmess_disallowed_list[i++] = ERR_STACKCRIT;
		zmess_disallowed_list[i++] = ERR_SPCLZMSG;
		assert(ZMESS_DISALLWD_LIST_SIZE == i);
	}

	/* Linear search as the list is short. */
	for (i = 0; i < ZMESS_DISALLWD_LIST_SIZE; ++i)
		if ((zmess_disallowed_list[i] >> MSGSEVERITY) == (errnum >> MSGSEVERITY)) /* Message severity doesn't matter. */
			return TRUE;
	return FALSE;
}

void op_zmess(unsigned int cnt, ...)
{
	va_list		var;
	const err_ctl	*ectl;
	const err_msg	*eptr;
	UINTPTR_T	fao[NUM_OF_FAO_SLOTS];
	char		buff[FAO_BUFFER_SPACE];
	unsigned int	errnum, j;
	int		faocnt;
	int4		tmp_severity;

	VAR_START(var, cnt);
	assert(34 == MAX_FAO_PARMS);			/* Defined in fao_parm.h. */
	errnum = va_arg(var, int);
	cnt--;
	if (NULL != (ectl = err_check(errnum)))		/* Note assignment. */
	{
		GET_MSG_INFO(errnum, ectl, eptr);
		tmp_severity = SEVMASK(errnum);
		faocnt = eptr->parm_count;
		faocnt = (faocnt > MAX_FAO_PARMS ? MAX_FAO_PARMS : faocnt);
		faocnt = mval2fao(eptr->msg, var, &fao[0], cnt, faocnt, buff, buff + SIZEOF(buff));
		va_end(var);
		if (0 <= faocnt)
		{
			if (WBTEST_ENABLED(WBTEST_INFO_HUB_SEND_ZMESS))
			{
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(2 + faocnt) errnum, faocnt, fao[0], fao[1], fao[2], fao[3],
					fao[4], fao[5], fao[6], fao[7], fao[8], fao[9], fao[10], fao[11], fao[12], fao[13], fao[14],
					fao[15], fao[16], fao[17], fao[18], fao[19], fao[20], fao[21], fao[22], fao[23], fao[24],
					fao[25], fao[26], fao[27], fao[28], fao[29], fao[30], fao[31], fao[32], fao[33]);
			} else if (is_disallowed(errnum))
			{
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4 + faocnt) ERR_SPCLZMSG, 0, errnum, faocnt, fao[0], fao[1],
					fao[2], fao[3], fao[4], fao[5], fao[6], fao[7], fao[8], fao[9], fao[10], fao[11], fao[12],
					fao[13], fao[14], fao[15], fao[16], fao[17], fao[18], fao[19], fao[20], fao[21], fao[22],
					fao[23], fao[24], fao[25], fao[26], fao[27], fao[28], fao[29], fao[30], fao[31], fao[32],
					fao[33]);
			} else if ((INFO == tmp_severity) || (SUCCESS == tmp_severity))
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(2 + faocnt) errnum, faocnt, fao[0], fao[1], fao[2], fao[3],
					fao[4], fao[5], fao[6], fao[7], fao[8], fao[9], fao[10], fao[11], fao[12], fao[13], fao[14],
					fao[15], fao[16], fao[17], fao[18], fao[19], fao[20], fao[21], fao[22], fao[23], fao[24],
					fao[25], fao[26], fao[27], fao[28], fao[29], fao[30], fao[31], fao[32], fao[33]);
			} else
			{
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(2 + faocnt) errnum, faocnt, fao[0], fao[1], fao[2], fao[3],
					fao[4], fao[5], fao[6], fao[7], fao[8], fao[9], fao[10], fao[11], fao[12], fao[13], fao[14],
					fao[15], fao[16], fao[17], fao[18], fao[19], fao[20], fao[21], fao[22], fao[23], fao[24],
					fao[25], fao[26], fao[27], fao[28], fao[29], fao[30], fao[31], fao[32], fao[33]);
			}

		}
	} else
	{
		va_end(var);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) errnum);
	}
}
