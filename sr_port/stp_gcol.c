/****************************************************************
 *								*
 *	Copyright 2002 Sanchez Computer Associates, Inc.	*
 *								*
 * Copyright (c) 2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#undef STP_MOVE
#undef STP_GCOL_NOSORT
#undef STP_GCOL_SPSIZE
#include "stp_gcol_src.h"

void stp_gcol(size_t space_asked)
{
	if (stringpool.stp_gcol_nosort && (stringpool.base == rts_stringpool.base))
		stp_gcol_nosort(space_asked);
	else
		stp_gcol_sort(space_asked);
}

