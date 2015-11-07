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

#include <ssdef.h>

#include "efn.h"
#include "rel_quant.h"

#define TINY_WAIT		(-5000)

static void rel_quant_ast(void)
{ /* Only purpose of this function is to provide a unique identifier for hiber_start timr driven while in an AST */
	return;
}

void rel_quant(void)
{
	int4		pause[2];
	int		status_timr, status_wait, ast_in_prog;
	int4		event_flag;
	gtm_int64_t	reqidt;

	pause[0] = TINY_WAIT;
	pause[1] = -1;
	if (0 != (ast_in_prog = lib$ast_in_prog()))
	{
		reqidt = (gtm_int64_t)rel_quant_ast;
		event_flag = efn_timer_ast;
	} else
	{
		reqidt = (gtm_int64_t)rel_quant;
		event_flag = efn_immed_wait;
	}
	status_timr = sys$setimr(event_flag, &pause, 0, reqidt, 0);
	assert(SS$_NORMAL == status_timr);
	if (SS$_NORMAL == status_timr)
	{
		status_wait = sys$waitfr(event_flag);
		assert(SS$_NORMAL == status_wait);
	}
}
