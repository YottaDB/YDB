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

#ifdef VMS
#  include <ssdef.h>
#endif

#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gtcm_read_ast.h"
#include "gtcm_action_pending.h"

void gtcm_read_ast(struct CLB *c)
{
	if (VMS_ONLY((c->ios.status & 1) == 0  &&  c->ios.status != SS$_DATAOVERUN)
	    UNIX_ONLY(CMI_CLB_ERROR(c)))
	{
		VMS_ONLY(
			if (c->ios.status == SS$_PROTOCOL)
			{
				cmi_read(c);
				return;
			}
			)
		*c->mbf = CMMS_E_TERMINATE;
	}
	gtcm_action_pending(c->usr);
	((connection_struct *)c->usr)->new_msg = TRUE;
	VMS_ONLY(sys$wake(0,0));
}
