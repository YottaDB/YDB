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

#include "gt_timer.h"
#include "op.h"
#include "outofband.h"
#include "rel_quant.h"

GBLREF	int4		outofband;

/*
 * ------------------------------------------
 * Hang the process for a specified time.
 *
 *	Unix version goes to sleep.
 *	Any caught signal will terminate the sleep
 *	following the execution of that signal's cathing routine.
 *
 * Arguments:
 *	a - time to sleep
 *
 * Return:
 *	none
 * ------------------------------------------
 */
void op_hang(int a)
{
	if (a <= 0)
	{
		if (0 == a)
			rel_quant();
	}
	else
	{
		hiber_start(a * 1000);
		if (outofband)
			outofband_action(FALSE);
	}
	return;
}
