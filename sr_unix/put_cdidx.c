/****************************************************************
 *								*
 * Copyright (c) 2014-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

/* Creates an operator reference to a given routine or label ultimately used to refer to an index into the linkage
 * table holding the address of the needed component.
 *
 * Parameter:
 *   - x - mstr address containing name of routine or label.
 *
 * Return:
 *   - operator descriptor
 */
oprtype put_cdidx(mstr *x)
{
	triple	*ref;
#	ifdef AUTORELINK_SUPPORTED
	mstr	*str;

	ref = newtriple(OC_CDIDX);
	ref->operand[0].oprclass = CDIDX_REF;
	ref->operand[0].oprval.cdidx = str = (mstr *)mcalloc(SIZEOF(mstr));
	str->addr = mcalloc(x->len);
	str->len = x->len;
	memcpy(str->addr, x->addr, x->len);
#	else
	assertpro(FALSE); 	/* Routine should never be used in non-autorelink platform */
	ref = NULL;		/* Avoid unintialized warnings. Need return in this path to avoid compiler error */
#	endif
	return put_tref(ref);
}
