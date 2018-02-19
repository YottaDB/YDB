/****************************************************************
 *								*
 * Copyright 2001 Sanchez Computer Associates, Inc.		*
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
#include "cmidef.h"

GBLREF struct NTD *ntd_root;

struct NTD *cmu_ntdroot(void)
{
	ASSERT_IS_LIBCMISOCKETTCP;
	return ntd_root;
}
