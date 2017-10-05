/****************************************************************
 *								*
 * Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	*
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
#include "gdsfhead.h"
#include "release_name.h"
#include "util.h"
#include "dse.h"

LITREF char	ydb_release_name[];
LITREF int4	ydb_release_name_len;

void dse_help(void)
{
	/* This function is a STUB to avoid editting sr_port/dse.h */
}

void dse_version(void)
{
	util_out_print("!AD", TRUE, ydb_release_name_len, ydb_release_name);
	return;
}
