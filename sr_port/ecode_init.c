/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <rtnhdr.h>
#include "stack_frame.h"
#include "error.h"		/* for ERROR_RTN */
#include "error_trap.h"

GBLREF	dollar_ecode_type	dollar_ecode;			/* structure containing $ECODE related information */
GBLREF	dollar_stack_type	dollar_stack;			/* structure containing $STACK related information */

/* NOTE: every malloc'd storage here should be free'd in a nested call-in environment.
 * gtmci_isv_restore (in gtmci_isv.c) needs to be reflected for any future mallocs added here. */
void	ecode_init(void)
{
	dollar_ecode.begin = (char *)malloc(DOLLAR_ECODE_ALLOC);
	dollar_ecode.top = dollar_ecode.begin + DOLLAR_ECODE_ALLOC;
	dollar_ecode.array = (dollar_ecode_struct *)malloc(SIZEOF(dollar_ecode_struct) * DOLLAR_ECODE_MAXINDEX);
	dollar_ecode.error_rtn_addr = NON_IA64_ONLY(CODE_ADDRESS(ERROR_RTN)) IA64_ONLY(CODE_ADDRESS_C(ERROR_RTN));
	dollar_ecode.error_rtn_ctxt = GTM_CONTEXT(ERROR_RTN);
	dollar_ecode.error_return_addr = (error_ret_fnptr)ERROR_RETURN;

	dollar_stack.begin = (char *)malloc(DOLLAR_STACK_ALLOC);
	dollar_stack.top = dollar_stack.begin + DOLLAR_STACK_ALLOC;
	dollar_stack.array = (dollar_stack_struct *)malloc(SIZEOF(dollar_stack_struct) * DOLLAR_STACK_MAXINDEX);

	NULLIFY_DOLLAR_ECODE;	/* this macro resets dollar_ecode.{end,index}, dollar_stack.{begin,index,incomplete} and
				 * first_ecode_error_frame to point as if no error occurred at all */
}
