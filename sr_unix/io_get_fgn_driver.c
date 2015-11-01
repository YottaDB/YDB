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

/*** STUB FILE ***/

#include "mdef.h"
#include "io.h"

dev_dispatch_struct *io_get_fgn_driver(mstr *s)
{	error_def(ERR_UNIMPLOP);
	rts_error(VARLSTCNT(1) ERR_UNIMPLOP);
	return ((dev_dispatch_struct *)NULL);
}
