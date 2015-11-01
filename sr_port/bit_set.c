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
#include "bit_set.h"

uint4 bit_set (uint4 bit, sm_uc_ptr_t base)
{
	int4		retval;
	sm_uc_ptr_t	ptr;

	ptr = base + bit / 8;
	retval = (1 << (bit & 7)) & *ptr;
	*ptr |= 1 << (bit & 7);
	return retval != 0;
}
