/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* is_proc_alive(pid, imagecnt) - VMS version.
 *
 * Checks to see if a process exists.   Returns TRUE (non-zero) or FALSE (zero)
 * accordingly.
 */
#include "mdef.h"

#include <ssdef.h>
#include "repl_sp.h"
#include "is_proc_alive.h"

bool is_proc_alive(uint4 pid, uint4 imagecnt)
{
	uint4 icount;

	if ((get_proc_info(pid, NULL, &icount) == SS$_NONEXPR) || (imagecnt && (imagecnt != icount)))
		return FALSE;
	return TRUE;
}

