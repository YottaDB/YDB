/****************************************************************
 *								*
<<<<<<< HEAD
 * Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 * Copyright (c) 2017 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
=======
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
>>>>>>> 451ab477 (GT.M V7.0-000)
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

error_def(ERR_UNSOLCNTERR);

void gvcmz_neterr_set(struct CLB *c)
{
<<<<<<< HEAD
	error_def(ERR_UNSOLCNTERR);

	ASSERT_IS_LIBGNPCLIENT;
	VMS_ONLY(
		if (c->ios.status == MSG$_CONNECT || c->ios.status == MSG$_INTMSG || c->ios.status == MSG$_CONFIRM)
		        return;
	)

=======
>>>>>>> 451ab477 (GT.M V7.0-000)
	if (((link_info*)(c->usr))->lck_info  & (REMOTE_ZALLOCATES | REMOTE_LOCKS | LREQUEST_SENT | ZAREQUEST_SENT))
	{
		((link_info *)(c->usr))->neterr = TRUE;
		RTS_ERROR_ABT(VARLSTCNT(1) ERR_UNSOLCNTERR);
	}
	return;
}
