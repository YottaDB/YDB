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
#include "op.h"

GBLREF mval **ind_source_sp, **ind_source_array;

void op_igetsrc(mval *v)
{
	ind_source_sp--;
	assert(ind_source_sp >= ind_source_array);
	*v = **ind_source_sp;
	v->mvtype &= ~MV_ALIASCONT;	/* Make sure alias container property does not pass */
	return;
}
