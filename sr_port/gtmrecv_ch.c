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

error_def(ERR_ASSERT);
error_def(ERR_CTRLC);
error_def(ERR_FORCEDHALT);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_MEMORY);
error_def(ERR_OUTOFSPACE);
error_def(ERR_STACKOFLOW);
error_def(ERR_VMSMEMORY);

CONDITION_HANDLER(gtmrecv_ch)
{
	START_CH;
	if (!(IS_GTM_ERROR(SIGNAL)) || DUMPABLE || SEVERITY == ERROR)
	{
       		NEXTCH;
	}
	/* warning, info, or success */
	CONTINUE;
}
