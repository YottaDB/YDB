/****************************************************************
 *								*
 * Copyright 2001 Sanchez Computer Associates, Inc.		*
 *								*
 * Copyright (c) 2018-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef OPCODE_included
#define OPCODE_included

#define OPCODE_DEF(A,B) A,

enum opcode_enum
{
	OCQ_INVALID = -1,
#include <opcode_def.h>		/* see HDR_FILE_INCLUDE_SYNTAX comment in mdef.h for why <> syntax is needed */
	OPCODE_COUNT
};

#undef OPCODE_DEF

#endif /* OPCODE_included */
