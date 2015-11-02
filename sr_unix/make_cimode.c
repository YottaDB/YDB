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

#include <rtnhdr.h>
#include "gtmci.h"
#include "make_mode.h"

/* The code created and returned by make_cimode() is executed in the frame GTM$CI at level 1 of
 * every nested call-in environment. For every M routine being called-in from C, GTM$CI code
 * will setup argument registers/stack and executes the M routine. When the M routine returns
 * from its final QUIT, GTM$CI returns to gtm_ci(). make_cimode generates machine equivalents
 * for the following operations in that order:
 *
 * 	CALL ci_restart	 :setup register/stack arguments from 'param_list' and transfer control
 * 		to op_extcall/op_extexfun which return only after the M routine finishes and QUITs.
 * 	CALL ci_ret_code :transfer control from the M routine back to C (gtm_ci). Never returns.
 * 	CALL opp_ret	 :an implicit QUIT although it is never executed.
 *
 * Before GTM$CI executes, it is assumed that the global 'param_list' has been populated with
 * argument/return mval *.
 */
rhdtyp *make_cimode(void)
{
	static rhdtyp	*base_address = NULL;

	if (NULL == base_address)
		base_address = make_mode(CI_MODE);
	return base_address;
}
