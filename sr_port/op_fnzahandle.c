/****************************************************************
 *								*
 *	Copyright 2009, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "lv_val.h"
#include "stringpool.h"

GBLREF	spdesc 		stringpool;

LITREF	mval		literal_null;

#define ALIAS_HANDLE_LENGTH (SIZEOF(INTPTR_T) * 2)	/* Two digits per hex address byte */

/* Gives a unique handle for either the var itself (if a base var aka unsubscripted) or
   if is a container var, then the handle is for the base var it points to. Other subscripted
   vars return NULL. The unique handle is the ASCII-ized version of the object's address.
*/
void op_fnzahandle(lv_val *srclv, mval *dst)
{
	unsigned char		*handle;

	assert(srclv);
	assert(dst);
	if (srclv->v.mvtype & MV_ALIASCONT)
	{	/* lv_val is an alias container, use its pointer addr as the handle */
		assert(!LV_IS_BASE_VAR(srclv));
		handle = (unsigned char *)srclv->v.str.addr;
	} else if (LV_IS_BASE_VAR(srclv))
		/* lv_val is a base variable, use its lv_val addr as the handle */
		handle = (unsigned char *)srclv;
	else
		handle = NULL;
	/* Now convert handle to return value -- return ascii-ized addr if present */
	if (NULL != handle)
	{
		ENSURE_STP_FREE_SPACE(ALIAS_HANDLE_LENGTH);
		dst->str.addr = (char *)stringpool.free;
		/* 32 bit platforms require double conversion of "handle" to avoid spurious compilation warnings */
		dst->str.len = i2hexl_nofill((qw_num)((UINTPTR_T)handle), (unsigned char *)dst->str.addr, ALIAS_HANDLE_LENGTH);
		stringpool.free += dst->str.len;
		dst->mvtype = MV_STR;
	} else
		memcpy(dst, &literal_null, SIZEOF(mval));
}
