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

#include "opcode.h"
#include "compiler.h"
#include "mmemory.h"

oprtype put_cdlt(mstr *x)
{
	triple *ref;
	mstr *str;

	ref = newtriple(OC_CDLIT);
	ref->operand[0].oprclass = CDLT_REF;
	ref->operand[0].oprval.cdlt = str = (mstr *) mcalloc(SIZEOF(mstr));
	str->addr = mcalloc(x->len);
	str->len = x->len;
	memcpy(str->addr, x->addr, x->len);
	return put_tref(ref);
}
