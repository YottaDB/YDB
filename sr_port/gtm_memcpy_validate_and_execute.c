/****************************************************************
 *								*
 * Copyright (c) 2012-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#define BYPASS_MEMCPY_OVERRIDE	/* Want to run original system memcpy() here */
#include "gtm_string.h"

#include "gtmdbglvl.h"
#include "gtm_memcpy_validate_and_execute.h"

#ifdef DEBUG	/* Is only a debugging routine - nothing to see here for a production build - move along */
GBLREF uint4	gtmDebugLevel;

/* Identify memcpy() invocations that should be memmove() instead. If this routine assert fails, the arguments
 * overlap so should be converted to memmove(). One exception to that rule which is currently bypassed is when
 * source and target are equal. There are no known implementation of memcpy() that would break in such a
 * condition so since at least two of these currently exist in GT.M (one in gtmcrypt.h on UNIX and one in
 * mu_cre_file on VMS), this routine does not cause an assert fail in that case.
 */
void *gtm_memcpy_validate_and_execute(void *target, const void *src, size_t len)
{
	/* Unless specifically bypassed, in DEBUG, disallow memcpy() larger than max positive integer (2GB) */
	assert((GDL_AllowLargeMemcpy & gtmDebugLevel) || ((0 <= (signed)len) && (MAXPOSINT4 >= len)));
	if (target == src)	/* Allow special case to go through but avoid actual memcpy() call */
		return target;
	assert(((char *)(target) > (char *)(src))
	       ? ((char *)(target) >= ((char *)(src) + (len)))
	       : ((char *)(src) >= ((char *)(target) + (len))));
	return memcpy(target, src, len);
}
#endif
