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

/*	gtm_assert - invoked via the "GTMASSERT" macro.
 *
 *	gtm_assert raises the ERR_GTMASSERT error condition which is
 *	intended to be a replacement for the ubiquitous ERR_GTMCHECK.   BYPASSOK
 *	It differs from ERR_GTMCHECK in that it indicates the module    BYPASSOK
 *	name and line number of its invocation so one can determine
 *	exactly which ERR_GTMASSERT caused the termination.
 *
 *	The "GTMASSERT" macro differs from the "assert" macro in that
 *	it is significant regardless of the definition or lack thereof
 *	of the macro "DEBUG" and is therefore valid for PRO images as
 *	well as for DBG and BTA images.
 *
 *	Note that the assertpro() macro is preferred in most instances where a specific condition
 *	is being tested.
 */

#include "mdef.h"
#include "send_msg.h"

LITREF char	gtm_release_name[];
LITREF int4	gtm_release_name_len;

error_def(ERR_GTMASSERT);

void	gtm_assert(int file_name_len, char file_name[], int line_no)
{
	send_msg (VARLSTCNT(7) ERR_GTMASSERT, 5, gtm_release_name_len, gtm_release_name, file_name_len, file_name, line_no);
	rts_error (VARLSTCNT(7) ERR_GTMASSERT, 5, gtm_release_name_len, gtm_release_name, file_name_len, file_name, line_no);
}
