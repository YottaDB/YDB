/****************************************************************
 *								*
 *	Copyright 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* gtm_assert2 - invoked via the "assertpro" macro.
 *
 * gtm_assert2 is driven by the assertpro() macro which is intended to be a replacement for
 * most instances of the GTMASSERT macro which gave no indication of the cause of the GTMASSERT
 * error. The GTMASSERT2 message output from this routine also contains the failing assertpro()
 * text very similar to what the ASSERT error does.
 */

#include "mdef.h"
#include "send_msg.h"

LITREF char	gtm_release_name[];
LITREF int4	gtm_release_name_len;

error_def(ERR_GTMASSERT2);

int gtm_assert2(int condlen, char *condtext, int file_name_len, char file_name[], int line_no)
{
	send_msg(VARLSTCNT(9) ERR_GTMASSERT2, 7, gtm_release_name_len, gtm_release_name, file_name_len, file_name, line_no,
		 condlen, condtext);
	rts_error(VARLSTCNT(9) ERR_GTMASSERT2, 7, gtm_release_name_len, gtm_release_name, file_name_len, file_name, line_no,
		  condlen, condtext);
	return 0;	/* Required for assertpro() macro which assumes (syntactically) this rtn returns a result */
}
