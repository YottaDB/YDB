/****************************************************************
 *								*
 * Copyright (c) 2016-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
/* inc_rand() is not thread-safe; others are believed to be thread-safe. */

#include "mdef.h"
#include "gtm_stdlib.h"
#include "gtmxc_types.h"
#include "gtmgblstat.h"

/* Functions to support %YGBLSTAT */
gtm_status_t accumulate(int argc, gtm_string_t *acc, gtm_string_t *incr)
{
	unsigned long long *acc1, *acc2, *incr1;
	acc1 = (unsigned long long *)acc->address;
	acc2 = (unsigned long long *)(acc->address + (acc->length > incr->length ? incr->length : acc->length));
	incr1 = (unsigned long long *)incr->address;
	while ( acc1 < acc2 ) *acc1++ += *incr1++ ;
	return 0;
}

gtm_status_t is_big_endian(int argc, gtm_uint_t *endian)
{
#if __BIG_ENDIAN__
	*endian = 1;
#else
	*endian = 0;
#endif
	return 0;
}

gtm_status_t to_ulong(int argc, gtm_ulong_t *value, gtm_string_t *bytestr)
{
	*value = *(gtm_ulong_t *)bytestr->address;
	return 0;
}
