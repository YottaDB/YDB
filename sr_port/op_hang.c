/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef VMS
#include <ssdef.h>

#include "efn.h"
#include "timedef.h"
#endif
#include "arit.h"
#include "gt_timer.h"
#include "mvalconv.h"
#include "op.h"
#include "outofband.h"
#include "rel_quant.h"

GBLREF	int4		outofband;

/*
 * ------------------------------------------
 * Hang the process for a specified time.
 *
 *	Goes to sleep for a positive value.
 *	Any caught signal will terminate the sleep
 *	following the execution of that signal's catching routine.
 *
 * Arguments:
 *	num - time to sleep
 *
 * Return:
 *	none
 * ------------------------------------------
 */
void op_hang(mval* num)
{
	int 	ms;
#ifdef VMS
	uint4 	time[2];
	int4	efn_mask, status;
	error_def(ERR_SYSCALL);
#endif
	ms = 0;
	MV_FORCE_NUM(num);
	if (num->mvtype & MV_INT)
	{
		if (0 < num->m[1])
		{
			assert(MV_BIAS >= 1000);	/* if formats change overflow may need attention */
			ms = num->m[1] * (1000 / MV_BIAS);
		}
	} else if (0 == num->sgn) /* if sign is not 0 it means num is negative */
		ms = mval2i(num) * 1000;	/* too big to care about fractional amounts */
	if (ms)
	{
		UNIX_ONLY(hiber_start(ms);)
		VMS_ONLY(
			time[0] = -time_low_ms(ms);
			time[1] = -time_high_ms(ms) - 1;
			efn_mask = (1 << efn_outofband | 1 << efn_timer);
			if (SS$_NORMAL != (status = sys$setimr(efn_timer, &time, NULL, &time, 0)))
				rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("$setimr"), CALLFROM, status);
			if (SS$_NORMAL != (status = sys$wflor(efn_outofband, efn_mask)))
				rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("$wflor"), CALLFROM, status);
		)
		if (outofband)
		{
			VMS_ONLY(
				if (SS$_WASCLR == (status = sys$readef(efn_timer, &efn_mask)))
				{
					if (SS$_NORMAL != (status = sys$cantim(&time, 0)))
						rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("$cantim"), CALLFROM,
							status);
				}
				else if (SS$_WASSET != status)
					GTMASSERT;
			)
			outofband_action(FALSE);
		}
	} else
	{
		rel_quant();
		if (outofband)
			outofband_action(FALSE);
	}
	return;
}
