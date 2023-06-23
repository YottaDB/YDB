/****************************************************************
 *								*
 * Copyright (c) 2022 YottaDB LLC and/or its subsidiaries.	*
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
#include "compiler.h"

/* The purpose of this condition handler is to catch any runtime errors (e.g. PATNOTFOUND etc.)
 * inside "m_for()", do cleanup of the FOR stack AND then bubble the error up to the next condition handler.
 * Not doing this cleanup can cause SIG-11s particularly if in direct mode.
 */
CONDITION_HANDLER(m_for_ch)
{
	START_CH(TRUE);
	FOR_POP(BLOWN_FOR);
	NEXTCH;
}
