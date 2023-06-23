/****************************************************************
 *								*
 * Copyright (c) 2001-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "error.h"

GBLREF err_ctl merrors_ctl;
GBLREF err_ctl cmerrors_ctl;
GBLREF err_ctl cmierrors_ctl;
GBLREF err_ctl gdeerrors_ctl;
GBLREF err_ctl ydberrors_ctl;

STATICDEF const err_ctl *all_errors[] = { &merrors_ctl, &gdeerrors_ctl,
		&cmierrors_ctl, &cmerrors_ctl,
		&ydberrors_ctl,
		NULL };

/* Returns the error control struct corresponding to the errornum if it is valid, otherwise
 * returns NULL
 */
const err_ctl *err_check(int errnum) {
	/* errnum structure:
	 *  ___________________________________________
	 * | 0fff1     FACILITY     1   MSG_IDX     SEV|
	 * |___________________________________________|
	 *  31   27                 15            3   0
	 *
	 * fff - flag bits since GTM-7759
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
		if ((((errnum & 0x0FFFFFFF) >> MSGFAC) == (FACMASK(fac->facnum) >> MSGFAC))
				&& (msg_id <= fac->msg_cnt) && (1 <= msg_id))
			return fac;
	}
	return NULL ;
}
