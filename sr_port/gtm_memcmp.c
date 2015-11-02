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

/*	gtm_memcmp - GT.M interlude to C library memcmp function.
 *
 *	gtm_memcmp is an interlude to the supplied C library memcmp that
 *	prevents memory addressing errors that can occur when the length
 *	of the items to be compared is zero and the supplied function
 *	does not first validate its input parameters.
 */

#include "mdef.h"

#include "gtm_string.h"

#undef memcmp
int gtm_memcmp (const void *a, const void *b, size_t len)
{
	return (int)(len == 0 ? len : memcmp(a, b, len));
}
