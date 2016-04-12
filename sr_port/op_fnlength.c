/****************************************************************
 *								*
 *	Copyright 2006, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "op.h"
#include "arit.h"
#include "mvalconv.h"

#ifdef UNICODE_SUPPORTED
#include "gtm_utf8.h"

void op_fnlength(mval *src, mval *dest)
{
	MV_FORCE_STR(src);
	MV_FORCE_LEN(src);
	MV_FORCE_MVAL(dest, (int)src->str.char_len);
}
#endif

void op_fnzlength(mval *src, mval *dest)
{
	MV_FORCE_STR(src);
	MV_FORCE_MVAL(dest, src->str.len);
}
