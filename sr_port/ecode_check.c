/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdlib.h"		/* for exit() */

#include "dollar_zlevel.h"
#include "error_trap.h"
#include "error.h"

GBLREF int4             error_last_ecode;
GBLREF mval             dollar_etrap;
GBLREF ecode_list	*dollar_ecode_list;

boolean_t ecode_check(void)
{
	int		curlev;
	ecode_list	*ecode_entry;

	curlev = dollar_zlevel();
	if (curlev < 1)
	{
		EXIT(error_last_ecode);
	}
	if (dollar_etrap.str.len > 0)
	{
		/* If there are errors in $ECODE at a deeper level
		 * when a QUIT command is executed
		 * trap the same error again at the next lower level.
		 *
		 * If there are errors at a lower level, we're in an
		 * error trap routine for that error, and shouldn't
		 * re-issue that error.
		 */
		for (ecode_entry = dollar_ecode_list;
			ecode_entry->previous;
			ecode_entry = ecode_entry->previous)
		{
			if (ecode_entry->level >= curlev)
				return TRUE;
		}
	}
	return FALSE;
}
