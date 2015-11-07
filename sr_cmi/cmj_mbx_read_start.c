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

#include <iodef.h>
#include "cmihdr.h"
#include "cmidef.h"
#include "efn.h"

int cmj_mbx_read_start(struct NTD *tsk)
{
        void cmj_mbx_ast();
	int status;

        status = sys$qio(efn_ignore, tsk->mch, IO$_READVBLK, &(tsk->mst),
		&cmj_mbx_ast, tsk, tsk->mbx.dsc$a_pointer, tsk->mbx.dsc$w_length, 0, 0, 0, 0);
	return status;
}
