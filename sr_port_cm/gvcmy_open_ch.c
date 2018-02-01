/****************************************************************
 *								*
 * Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2017-2018 YottaDB LLC. and/or its subsidiaries.*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "error.h"
#include "util.h"

GBLREF	gv_key	*gv_currkey, *gv_altkey;
GBLREF	gd_addr	*gd_header;

error_def(ERR_ASSERT);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_OUTOFSPACE);
error_def(ERR_STACKOFLOW);
error_def(ERR_MEMORY);

CONDITION_HANDLER(gvcmy_open_ch)
{
	ASSERT_IS_LIBGNPCLIENT;
	START_CH(TRUE);
	if (DUMPABLE)
	{ /* don't disturb state so that the core reflects the "bad" state */
		NEXTCH;
	}
	if (WARNING == SEVERITY)
	{
		PRN_ERROR;
		CONTINUE;
	}
	assert(NULL != gd_header);
	assert(NULL != gv_currkey);
	assert(NULL != gv_altkey);
	gv_currkey->base[0] = gv_altkey->base[0] = '\0'; /* error opening remote db should reset gv_currkey and gv_altkey so
							  * that we drive gtcm_bind_name if the next global reference is the
							  * same as the current one */
	NEXTCH;
}
