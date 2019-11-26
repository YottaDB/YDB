/****************************************************************
 *								*
* Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
* All rights reserved.						*
*								*
*	This source code contains the intellectual property	*
*	of its copyright holder(s), and is made available	*
*	under a license.  If you do not know the terms of	*
*	the license, please stop and do not read further.	*
*								*
****************************************************************/

#include "mdef.h"

#include "mmrhash.h"
#include "op.h"
#include "stringpool.h"

#define HASHED_STRING_LEN	(128 / 4) /* We need 1 byte for every hex digit in the 128 bit value. */

/* This routine ($ZHASH) exposes the MurmurHash3 routine to M code. It takes 2 parameters from the user:
 * string: the string to hash
 * salt: an optional salt for the hash that defaults to 0 if not present
 *
 * There is also an additional parameter ret which is used to return an mval containing
 * the MurmurHash3 hash of string. This mval is then returned to the M user.
 */
void op_fnzhash(mval *string, int salt, mval *ret)
{
	ydb_uint16	hash_out;
	unsigned char	*str_out;

	ret->mvtype = 0; /* we initialize this string to keep the garbage collector from worrying about an incomplete string  */
	MV_FORCE_STR(string);
	MurmurHash3_x64_128(string->str.addr, string->str.len, salt, &hash_out);
	ENSURE_STP_FREE_SPACE(HASHED_STRING_LEN);
	str_out = (unsigned char *)stringpool.free;
	ydb_mmrhash_128_hex(&hash_out, str_out);
	stringpool.free += HASHED_STRING_LEN;
	assert(stringpool.free <= stringpool.top);
	ret->str.addr = (char *)str_out;
	ret->str.len = HASHED_STRING_LEN;
	ret->mvtype = MV_STR;
}
