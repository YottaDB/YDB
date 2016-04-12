/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "op.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmidefsp.h"		/* needed for cmmdef.h */
#include "cmmdef.h"
/*
 * -----------------------------------------------
 * This routine is a one-argument front-end for op_zallocate
 *  which has a second argument to deal with secondary ownership,
 *  such as that used by a server on behalf of its clients.
 *
 * Arguments:
 *	timeout	- max. time to wait for locks before giving up
 *
 * Return:
 *	1 - if not timeout specified
 *	if timeout specified:
 *		!= 0 - all the locks int the list obtained, or
 *		0 - blocked
 *	The return result is suited to be placed directly into
 *	the $T variable by the caller if timeout is specified.
 * -----------------------------------------------
 */
int op_zallocate(int timeout)
{

	return op_lock2(timeout, CM_ZALLOCATES);
}
