/**************************************************************** *								*
* Copyright (c) 2021 YottaDB LLC and/or its subsidiaries.	*
* All rights reserved.						*
*								*
*	This source code contains the intellectual property	*
*	of its copyright holder(s), and is made available	*
*	under a license.  If you do not know the terms of	*
*	the license, please stop and do not read further.	*
*								*
****************************************************************/

#include "mdef.h"

#include "op.h"
#include "stringpool.h"
#include "mmrhash.h"

/* Since this function is implemented for Octo, this is how Octo defines the
 * length of the hash. I put this here to ensure we can change it easily if
 * we need to.
 */
#define YDB_NAME_PREFIX 9 /* e.g. %ydboctoX */
#define YDB_ZYSUFFIX_LENGTH (MAX_MIDENT_LEN - YDB_NAME_PREFIX)

/* Implemenation of $ZYSU[FFIX]:
 *
 * The M function $zysu[ffix](string) implements the Octo C function int
 * ydb_hash2name_s(ydb_buffer_t *name, uint128_t *hash). This function returns
 * a 22 character alphanumeric (0-9, a-z, A-Z) name that can be concatenated to
 * an application identifier (such as %ydbocto) to generate names for global
 * variables, local variables, and routines that are guaranteed for all
 * practical purposes to be unique.
 */
void op_fnzysuffix(mval *string, mval *ret)
{
	ydb_uint16     mmrhash;
	char           *base62_chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	char           *zysuffix; /* temp place for our data */
	int            i;

	/* This is what $ZYHASH does */
	MV_FORCE_STR(string);
	MurmurHash3_x64_128(string->str.addr, string->str.len, 0, &mmrhash);
	/* Prepare string pool and point our temporary heap buffer to it*/
	ENSURE_STP_FREE_SPACE(YDB_ZYSUFFIX_LENGTH);
	zysuffix = (char *)stringpool.free;
	/* Convert hash to alphabet putting result in zysuffix */
	for (i = 0; i < YDB_ZYSUFFIX_LENGTH;)
	{
		zysuffix[i++] = base62_chars[mmrhash.one % 62];
		mmrhash.one = mmrhash.one / 62;
		zysuffix[i++] = base62_chars[mmrhash.two % 62];
		mmrhash.two = mmrhash.two / 62;
	}
	/* Ensure we have the right length */
	assert(&zysuffix[i] - zysuffix == YDB_ZYSUFFIX_LENGTH);
	/* Update string pool after result is written */
	stringpool.free += YDB_ZYSUFFIX_LENGTH;
	assert(stringpool.free <= stringpool.top);
	/* Prepare return mval */
	ret->str.addr = zysuffix;
	ret->str.len = YDB_ZYSUFFIX_LENGTH;
	ret->mvtype = MV_STR;
}
