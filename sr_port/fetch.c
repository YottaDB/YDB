/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>

#include "rtnhdr.h"
#include "stack_frame.h"
#include "lookup_variable.h"
#include "op.h"

GBLREF stack_frame *frame_pointer;

#ifdef __MVS__
/* fetch conflicts with MVS name */
void gtm_fetch(unsigned int cnt_arg, unsigned int indxarg, ...)
#elif defined(UNIX)
void fetch(unsigned int cnt_arg, unsigned int indxarg, ...)
#elif defined(VMS)
void fetch(unsigned int indxarg, ...)
#else
#error unsupported platform
#endif
{
	va_list		var;
	unsigned int 	indx;
	unsigned int 	cnt;
	stack_frame	*fp;
	mval		**mvpp;

	VAR_START(var, indxarg);
	VMS_ONLY(va_count(cnt);)
	UNIX_ONLY(cnt = cnt_arg;)	/* need to preserve stack copy on i386 */
	fp = frame_pointer;
	if (0 < cnt)
	{	/* All generated code comes here to verify instantiation
		   of a given set of variables from the local variable table */
		indx = indxarg;
		for ( ; ; )
		{
			mvpp = &fp->l_symtab[indx];
			if (0 == *mvpp)
				*mvpp = lookup_variable(indx);
			if (0 < --cnt)
				indx = va_arg(var, int4);
			else
				break;
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
	va_end(var);
}
