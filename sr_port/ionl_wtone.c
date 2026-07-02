/****************************************************************
 *								*
 * Copyright (c) 2001-2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "io.h"

void ionl_wtone(int v)
{
	unmanaged_mstr temp;
	char p;

	p = (char)v;
	temp.len = 1;
	temp.addr = &p;
	ionl_write(&temp);
	return;
}
