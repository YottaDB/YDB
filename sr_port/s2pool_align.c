/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "stringpool.h"

GBLREF boolean_t	mstr_native_align;
GBLREF spdesc 		stringpool;

void s2pool_align(mstr *string)
{
	int length, align_padlen;

	if ((length = string->len) == 0)
		return;
	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);
	if (mstr_native_align)
		align_padlen = PADLEN(stringpool.free, NATIVE_WSIZE);
	else
		align_padlen = 0;
	ENSURE_STP_FREE_SPACE(length + align_padlen);
	stringpool.free += align_padlen;
	memcpy(stringpool.free, string->addr, length);
	string->addr = (char *)stringpool.free;
	stringpool.free += length;
	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);
}
