/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#define OPCODE_DEF(A,B) A,

enum opcode_enum
{
#include <opcode_def.h>
	OPCODE_COUNT
};

#undef OPCODE_DEF
