/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <varargs.h>

#include "rtnhdr.h"
#include "stack_frame.h"
#include "lookup_variable.h"
#include "op.h"

GBLREF stack_frame *frame_pointer;

#ifndef __MVS__
void fetch(va_alist)
#else
void gtm_fetch(va_alist)
#endif
va_dcl
{
	va_list		var;
	unsigned int 	cnt, indx;
	stack_frame	*fp;
	mval		**mvpp;

	VAR_START(var);
	cnt = va_arg(var, int4);
	fp = frame_pointer;
	if (0 < cnt)
	{	/* All generated code comes here to verify instantiation
		   of a given set of variables from the local variable table */
		for (; 0 < cnt; --cnt)
		{
			indx = va_arg(var, int4);
			mvpp = &fp->l_symtab[indx];
			if (0 == *mvpp)
				*mvpp = lookup_variable(indx);
		}
	} else
	{	/* GT.M calls come here to verify instantiation of the
		   entire local variable table */
		indx = fp->vartab_len;
		mvpp = &fp->l_symtab[indx];
		for (; indx > 0;)
		{
			--indx;
			--mvpp;
			if (NULL == *mvpp)
				*mvpp = lookup_variable(indx);
		}
	}
}
