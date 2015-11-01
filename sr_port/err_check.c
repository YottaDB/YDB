/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "error.h"

LITREF err_ctl merrors_ctl;
LITREF err_ctl cmerrors_ctl;
LITREF err_ctl cmierrors_ctl;
LITREF err_ctl gdeerrors_ctl;
#ifdef VMS
LITREF err_ctl laerrors_ctl;	/* Roger thinks that this one is obsolete */
LITREF err_ctl lperrors_ctl;	/* Roger thinks that this one may be obsolete */
#endif

err_ctl *err_check(int errnum)
{
	err_ctl	*all_errors[] = {&merrors_ctl, &gdeerrors_ctl, &cmierrors_ctl, &cmerrors_ctl,
#ifdef VMS
				&laerrors_ctl, &lperrors_ctl,
#endif
 				NULL};
	err_ctl	*fac;
	int	errtype;

        if (0 > errnum)
                return 0;

	for (errtype = 0; all_errors[errtype]; errtype++)
	{
		fac = all_errors[errtype];
		if ((errnum & FACMASK(fac->facnum)) &&
			((MSGMASK(errnum, fac->facnum)) <= fac->msg_cnt))
			return fac;
	}
	return 0;
}
