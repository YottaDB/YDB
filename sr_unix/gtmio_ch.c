/****************************************************************
 *								*
 * Copyright (c) 2015 Fidelity National Information 		*
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
#include "error.h"

GBLREF boolean_t in_prin_gtmio;

CONDITION_HANDLER(gtmio_ch)
{
	START_CH(TRUE);
	in_prin_gtmio = FALSE;
	NEXTCH;
}
