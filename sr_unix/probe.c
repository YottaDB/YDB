/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <sys/types.h>
#include <sys/mman.h>
#include "probe.h"
OS_PAGE_SIZE_DECLARE

boolean_t probe(uint4 len, void *addr, boolean_t write)
{
	int status;
	uint4 temp;

	if (NULL == addr)	/* Special case for null pointer passed */
		return FALSE;
#if defined(__alpha)
	temp = ROUND_DOWN2((uint4)addr, OS_PAGE_SIZE);
	status = mvalid((caddr_t)temp, (size_t)(len + ((uint4)addr - temp)), write ? PROT_WRITE : PROT_READ);
	assert(0 == status);
	return (status == 0 ? TRUE : FALSE);
#else
	return TRUE;
#endif
}

