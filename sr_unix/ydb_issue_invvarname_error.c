/***************************************************************
 *								*
 * Copyright (c) 2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "libyottadb_int.h"
#include "op.h"

/* Helper function to issue a YDB_ERR_INVVARNAME error. Used by various Simple API functions. */
void	ydb_issue_invvarname_error(const ydb_buffer_t *varname)
{
	mval	src, dst;

	src.mvtype = MV_STR;
	src.str.addr = varname->buf_addr;
	src.str.len = varname->len_used;
	op_fnzwrite(FALSE, &src, &dst); /* dst points to stringpool */
	assert(MV_STR == dst.mvtype);
	rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_INVVARNAME, 2, dst.str.len, dst.str.addr);
}

