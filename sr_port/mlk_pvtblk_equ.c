/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "mlkdef.h"
#include "mlk_pvtblk_equ.h"

int mlk_pvtblk_equ(mlk_pvtblk *a, mlk_pvtblk *b)
{
	return (a->ctlptr == b->ctlptr && a->nref_length == b->nref_length
		&& (memcmp(a->value, b->value, a->nref_length) == 0));
}
