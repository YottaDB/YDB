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
  #include <msgdef.h>
#endif
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gvcmz.h"

void gvcmz_neterr_set(struct CLB *c)
{
	error_def(ERR_UNSOLCNTERR);

	VMS_ONLY(
		if (c->ios.status == MSG$_CONNECT || c->ios.status == MSG$_INTMSG || c->ios.status == MSG$_CONFIRM)
		        return;
	)

	if (((link_info*)(c->usr))->lck_info  & (REMOTE_ZALLOCATES | REMOTE_LOCKS | LREQUEST_SENT | ZAREQUEST_SENT))
	{
		((link_info *)(c->usr))->neterr = TRUE;
		rts_error(VARLSTCNT(1) ERR_UNSOLCNTERR);
	}
	return;
}
