/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
**++
**  FACILITY:
**
**      MLK - MUMPS Locks.
**
**  ABSTRACT:
**
**      [@tbs@]
**
**  AUTHORS:
**
**      [@tbs@]
**
**
**  CREATION DATE:     [@tbs@]
**
**  MODIFICATION HISTORY:
**--
**/
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**      Converts the nref string passed to op_lock, etc. to string form.
**
**  FORMAL PARAMETERS:
**
**      Pointer to the nref string.
**
**  IMPLICIT INPUTS:
**
**      Each of the mval's pointed to by any element of the nref string.
**
**  IMPLICIT OUTPUTS:
**
**      Any mvals which were not not in string form, are converted.
**
**  FUNCTION VALUE:
**
**      none
**
**  SIDE EFFECTS:
**
**      1. stringpool may be garbage collected.
**	2. If any of the mval's are undefined, an UNDEF signal will be
**	   raised before leaving this routine.
**
**--
**/
#include "mdef.h"
#include "stringpool.h"
#include <varargs.h>
#include "mlk_force_str.h"

void mlk_force_str(va_list s)
{
	mval *v;
	int subcnt, i;
	error_def(ERR_MAXLKSTR);

	subcnt = va_arg(s,int);
	assert(subcnt >= 2);
	v = va_arg(s,mval *); subcnt--; /* jmp over extended global reference */
	if (v)
	{	if ((v->mvtype & MV_STR) == 0)
			n2s(v);
		v = va_arg(s,mval *); subcnt--; /* get second argument string */
	}
	for (i = 0 ; i < subcnt ; i++)
	{
		v = va_arg(s,mval *);
		if ((v->mvtype & MV_STR) == 0)
			n2s(v);
		if (v->str.len > 255)
			rts_error(VARLSTCNT(1) ERR_MAXLKSTR);
	}
	return;
}
