/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "error.h"

VMS_ONLY(LITREF) UNIX_ONLY(GBLREF) err_ctl merrors_ctl;
VMS_ONLY(LITREF) UNIX_ONLY(GBLREF) err_ctl cmerrors_ctl;
VMS_ONLY(LITREF) UNIX_ONLY(GBLREF) err_ctl cmierrors_ctl;
VMS_ONLY(LITREF) UNIX_ONLY(GBLREF) err_ctl gdeerrors_ctl;
#ifdef VMS
LITREF err_ctl laerrors_ctl;	/* Roger thinks that this one is obsolete */
LITREF err_ctl lperrors_ctl;	/* Roger thinks that this one may be obsolete */
#endif

STATICDEF const err_ctl	*all_errors[] = {&merrors_ctl, &gdeerrors_ctl, &cmierrors_ctl, &cmerrors_ctl,
#ifdef VMS
					 &laerrors_ctl, &lperrors_ctl,
#endif
					 NULL};

const err_ctl *err_check(int errnum)
{
	const err_ctl	*fac;
	int		errtype;

        if (0 > errnum)
                return NULL;

	for (errtype = 0; all_errors[errtype]; errtype++)
	{
		fac = all_errors[errtype];
		if ((errnum & FACMASK(fac->facnum))
		    && ((MSGMASK(errnum, fac->facnum)) <= fac->msg_cnt))
			return fac;
	}
	return NULL;
}
