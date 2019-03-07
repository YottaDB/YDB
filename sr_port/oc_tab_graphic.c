/****************************************************************
 *								*
 * Copyright 2001 Sanchez Computer Associates, Inc.		*
 *								*
 * Copyright (c) 2018 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "compiler.h"
#include "opcode.h"

LITDEF char *oc_tab_graphic[] =
{
#define OPCODE_DEF(A,B) #A,
#include <opcode_def.h>		/* see HDR_FILE_INCLUDE_SYNTAX comment in mdef.h for why <> syntax is needed */
};
#undef OPCODE_DEF
