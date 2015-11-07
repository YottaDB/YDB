/****************************************************************
 *								*
 *	Copyright 2013, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <rtnhdr.h>

#ifdef AUTORELINK_SUPPORTED /* entire file */
/* Routine to provide the content of the thread-local variable "lab_lnr" as a return value to
 * generated code.
 *
 * Parameters: none
 *
 * Return value:
 *   -  Address of line-number table entry corresponding to the entryref's label
 */
void *op_lab_ext(void)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	return TREF(lab_lnr);
}
#endif /* AUTORELINK_SUPPORTED over entire file */
