/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"

#include <limits.h>
#include <errno.h>

#include "cli.h"
#include "util.h"

/* A lot of stuff that can be made portable across unix and vvms cli.c needs to be moved into this module.
 * For a start, cli_str_to_hex() is moved in. At least cli_get_str(), cli_get_int(), cli_get_num() can be moved in later.
 */

/*
 * --------------------------------------------------
 * Convert string to hex.
 *
 * Return:
 *	TRUE	- OK
 *	FALSE	- Could not convert to hex
 * --------------------------------------------------
 */
boolean_t cli_str_to_hex(char *str, int4 *dst)
{
	long		result, minus_uintmax;
	int		save_errno;
	boolean_t	long_bigger_than_int4;

	long_bigger_than_int4 = (sizeof(result) > sizeof(int4));
	if (long_bigger_than_int4)
	{	/* store -UINT_MAX in a variable to compare against result later */
		minus_uintmax = UINT_MAX;	/* need to do this in 2 steps as -UINT_MAX turns out to be 0 otherwise */
		minus_uintmax = -minus_uintmax;
	}
	save_errno = errno;
	errno = 0;
        result = STRTOUL(str, NULL, 16);
	if ((0 != errno)
		|| (long_bigger_than_int4 && ((UINT_MAX < result) || (minus_uintmax > result))))

	{	/* errno is non-zero implies "str" is outside the range of representable values in an unsigned long.
		 * else if result > UINT_MAX, it means that on platforms where long is 8-bytes, "str" is >= 4G
		 */
		*dst = 0;
		errno = save_errno;
		return FALSE;
	} else
	{
		*dst = result;
		errno = save_errno;
		return TRUE;
	}
}
