/****************************************************************
 *								*
 * Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <rtnhdr.h>

#include "stack_frame.h"
#include "startup.h"
#include "gtm_startup.h"

GBLREF	stack_frame	*frame_pointer;

void gtm_init_env(rhdtyp *base_addr, unsigned char *transfer_addr)
{
	assert(CURRENT_RHEAD_ADR(base_addr) == base_addr);
	/* Note: GT.M V6.3-005 had added an ESTABLISH/REVERT of "mdb_condition_handler" surrounding
	 * the "base_frame" and "new_stack_frame" calls (likely because they can issue STACKCRIT/STACKOFLOW errors).
	 * But since "dm_start" has not yet run, we should NOT be driving an M code error handler at this point
	 * (possible if $gtm_etrap is set) as there is as of yet no save area. Hence the ESTABLISH/REVERT usages
	 * have been commented out.
	 *
	 * ESTABLISH(mdb_condition_handler);
	 */
	base_frame(base_addr);

#	ifdef HAS_LITERAL_SECT
	new_stack_frame(base_addr, (unsigned char *)LINKAGE_ADR(base_addr), transfer_addr);
#	else
	/* Any platform that does not follow pv-based linkage model either
	 *	(1) uses the following calculation to determine the context pointer value, or
	 *	(2) doesn't need a context pointer
	 */
	new_stack_frame(base_addr, PTEXT_ADR(base_addr), transfer_addr);
#	endif
	/* REVERT; */
}
