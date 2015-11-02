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

GBLREF mval **ind_result_sp, **ind_result_array;

void op_iretmval(mval *v)
{

	ind_result_sp--;
	assert(ind_result_sp >= ind_result_array);
	MV_FORCE_DEFINED(v);
	**ind_result_sp = *v;
	(*ind_result_sp)->mvtype &= ~MV_ALIASCONT;	/* Make sure alias container property does not pass */
	op_unwind();

}
