/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include <sys/time.h>

#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gt_timer.h"
#include "gtcm_init_ast.h"
#include "gtcm_read_ast.h"

GBLREF int gtcm_users;
GBLREF bool cm_shutdown;
GBLREF bool cm_timeout;

void gtcm_init_ast(struct CLB *c)
{
	connection_struct *cs;

	cs = UNIX_ONLY(c->usr;) /* for Unix, allocated already -- setup now */
	     VMS_ONLY((connection_struct *)malloc(SIZEOF(connection_struct));)
	memset(cs, 0, SIZEOF(*cs));
	UNIX_ONLY(gettimeofday(&cs->connect, NULL);)
	VMS_ONLY(
		sys$gettim(&cs->connect[0]);
		c->usr = cs;
		c->mbf = malloc(CM_MSG_BUF_SIZE + CM_BUFFER_OVERHEAD);
		c->mbl = CM_MSG_BUF_SIZE + CM_BUFFER_OVERHEAD;
	)
	cs->lk_cancel = cs->last_cancelled = CM_NOLKCANCEL;
	cs->clb_ptr = c;
	c->ast = gtcm_read_ast;
	cmi_read(c);
	gtcm_users++;
	if (cm_timeout)
		cancel_timer((TID)&cm_shutdown);
}
