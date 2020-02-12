/****************************************************************
 *								*
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef BXRELOP_OPERATOR_INCLUDED
#define BXRELOP_OPERATOR_INCLUDED

int	bxrelop_operator(mval *lhs, mval *rhs, opctype relopcode, int this_bool_depth, uint4 combined_opcode);

#endif /* BXRELOP_OPERATOR_INCLUDED */

