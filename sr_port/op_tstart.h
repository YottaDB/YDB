/****************************************************************
 *								*
 * Copyright (c) 2017-2018 YottaDB LLC. and/or its subsidiaries.*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef OP_TSTART_included
#define OP_TSTART_included

#define	NORESTART -1
#define	ALLLOCAL  -2
#define	LISTLOCAL -3

void	op_tstart(int implicit_flag, ...);

#endif /* OP_TSTART_included */
