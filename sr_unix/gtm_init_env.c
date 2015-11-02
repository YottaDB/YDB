/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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
	base_frame(base_addr);

#ifdef HAS_LITERAL_SECT
	new_stack_frame(base_addr, (unsigned char *)LINKAGE_ADR(base_addr), transfer_addr);
#else
	/* Any platform that does not follow pv-based linkage model either
	 *	(1) uses the following calculation to determine the context pointer value, or
	 *	(2) doesn't need a context pointer
	 */
	new_stack_frame(base_addr, PTEXT_ADR(base_addr), transfer_addr);
#endif
}
