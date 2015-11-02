/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "error.h"

VMS_ONLY(LITREF)UNIX_ONLY(GBLREF) err_ctl merrors_ctl;
VMS_ONLY(LITREF)UNIX_ONLY(GBLREF) err_ctl cmerrors_ctl;
VMS_ONLY(LITREF)UNIX_ONLY(GBLREF) err_ctl cmierrors_ctl;
VMS_ONLY(LITREF)UNIX_ONLY(GBLREF) err_ctl gdeerrors_ctl;
#ifdef VMS
LITREF err_ctl laerrors_ctl; /* Roger thinks that this one is obsolete */
LITREF err_ctl lperrors_ctl; /* Roger thinks that this one may be obsolete */
#endif

STATICDEF const err_ctl *all_errors[] = { &merrors_ctl, &gdeerrors_ctl,
		&cmierrors_ctl, &cmerrors_ctl,
#ifdef VMS
		&laerrors_ctl, &lperrors_ctl,
#endif
		NULL };

/* Returns the error control struct corresponding to the errornum if it is valid, otherwise
 * returns NULL
 */
const err_ctl *err_check(int errnum) {
	/* errnum structure:
	 *  ___________________________________________
	 * |     1     FACILITY     1   MSG_IDX     SEV|
	 * |___________________________________________|
	 *  31   27                 15            3   0
	 *
	 */
	const err_ctl *fac;
	int errtype;
	int msg_id; /* Error message number once facility and severity are stripped */

	if (0 > errnum)
		return NULL ;

	for (errtype = 0; all_errors[errtype]; errtype++) {
		fac = all_errors[errtype];
		msg_id = MSGMASK(errnum, fac->facnum);

		/* These conditions ensure: The facility bits are identical, the message index
		 * doesn't exceed the array size and is larger than zero */
		if (((errnum >> MSGFAC) == (FACMASK(fac->facnum) >> MSGFAC))
				&& (msg_id <= fac->msg_cnt) && (1 <= msg_id))
			return fac;
	}
	return NULL ;
}
