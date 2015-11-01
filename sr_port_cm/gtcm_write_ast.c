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
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gtcm_write_ast.h"
#include "gtcm_action_pending.h"
#include "gtcm_int_unpack.h"
#include "gtcm_read_ast.h"

void gtcm_write_ast(struct CLB *c)
{
	if (CMI_CLB_ERROR(c))
	{	/* error */
		*c->mbf = CMMS_E_TERMINATE;
		c->ast = 0;
		gtcm_action_pending(c->usr);
		return;
	}
	if (((connection_struct *)c->usr)->int_cancel.laflag & 1)
	{     /* deal with deferred interrupt cancel message in gtcm_mbxread_ast */
		c->ast = 0;
		gtcm_int_unpack(c->usr);
	} else if (((connection_struct *)c->usr)->state != CM_NOOP)  /* never NOOP I think - smw */
	{
		c->ast = gtcm_read_ast;
		cmi_read(c);
	} else
		c->ast = NULL;
}
