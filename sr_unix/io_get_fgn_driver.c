/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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

error_def(ERR_INVMNEMCSPC);

dev_dispatch_struct *io_get_fgn_driver(mstr *s)
{
	rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_INVMNEMCSPC, 2, s->len, s->addr);
	return ((dev_dispatch_struct *)NULL);
}
