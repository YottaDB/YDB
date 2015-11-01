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
#include "rtnhdr.h"
#include "stack_frame.h"
#include "startup.h"
#include "gtm_startup.h"

GBLREF	stack_frame	*frame_pointer;

void gtm_init_env(rhdtyp *base_addr, unsigned char *transfer_addr)
{
	assert(base_addr->current_rhead_ptr == 0);
	base_frame(base_addr);

#if	defined(__osf__) || defined (__MVS__) || defined (__s390__)
	new_stack_frame(base_addr, (unsigned char*)base_addr->linkage_ptr, transfer_addr);
	frame_pointer->literal_ptr = base_addr->literal_ptr;	/* new_stack_frame doesn't initialize this field */

#else
	/* Assume everything that is not OSF/1 (Digital Unix) is either:
	 *	(1) AIX/6000 and uses the following calculation to determine the context pointer value, or
	 *	(2) doesn't need a context pointer
	 */
	new_stack_frame(base_addr, (unsigned char*)base_addr + sizeof(rhdtyp), transfer_addr);
#endif
}
