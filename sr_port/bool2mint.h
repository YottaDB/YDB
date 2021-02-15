/****************************************************************
 *								*
<<<<<<< HEAD:sr_port/bool2mint.h
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
=======
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
>>>>>>> 451ab477 (GT.M V7.0-000):sr_unix/gvcmz_zflush_stub.c
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef BOOL2MINT_INCLUDED
#define BOOL2MINT_INCLUDED

int	bool2mint(int src, int this_bool_depth);

#endif /* BOOL2MINT_INCLUDED */

<<<<<<< HEAD:sr_port/bool2mint.h
=======
error_def(ERR_UNIMPLOP);

void gvcmz_zflush(void)
{
	RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_UNIMPLOP);
}
>>>>>>> 451ab477 (GT.M V7.0-000):sr_unix/gvcmz_zflush_stub.c
