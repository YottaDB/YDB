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

#include "mdef.h"

void fgncal_zlinit (void)
{
	void	fgncal_rtn(), gtm$fgncall();
	bool	dummy;

	dummy = zlput_rname(CODE_ADDRESS(fgncal_rtn));
	assert(TRUE == dummy);
	dummy = zlput_rname(CODE_ADDRESS(gtm$fgncall));
	assert(TRUE == dummy);
}
