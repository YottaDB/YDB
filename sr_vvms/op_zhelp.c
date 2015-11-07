/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_limits.h"
#include <descrip.h>
#include "io.h"

#define	HLP$M_PROMPT	1
#define	DEFAULT_LIBRARY	"GTM$HELP:MUMPS.HLB"

void op_zhelp(mval *text,mval *lib)
{
	struct dsc$descriptor	helptext, library;
	int			status, flags;
	error_def(ERR_INVSTRLEN);

	MV_FORCE_STR(text);
	MV_FORCE_STR(lib);
	if (SHRT_MAX < text->str.len)
		rts_error(VARLSTCNT(4) ERR_INVSTRLEN, 2, text->str.len, SHRT_MAX);
	if (SHRT_MAX < lib->str.len)
		rts_error(VARLSTCNT(4) ERR_INVSTRLEN, 2, lib->str.len, SHRT_MAX);
	flush_pio();
	helptext.dsc$w_length	= text->str.len;
	helptext.dsc$b_dtype	= DSC$K_DTYPE_T;
	helptext.dsc$b_class	= DSC$K_CLASS_D;
	helptext.dsc$a_pointer	= text->str.addr;
	library.dsc$b_dtype	= DSC$K_DTYPE_T;
	library.dsc$b_class	= DSC$K_CLASS_D;
	if (!lib->str.len)
	{
		library.dsc$w_length	= sizeof DEFAULT_LIBRARY - 1;
		library.dsc$a_pointer	= DEFAULT_LIBRARY;
	}
	else
	{
		library.dsc$w_length	= lib->str.len;
		library.dsc$a_pointer	= lib->str.addr;
	}
	flags = HLP$M_PROMPT;
	status = lbr$output_help(lib$put_output, 0, &helptext, &library, &flags, lib$get_input);
	if (!(status & 1)) rts_error(VARLSTCNT(1) status);
	return;
}
