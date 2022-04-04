/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MU_GETLST_INCLUDED
#define MU_GETLST_INCLUDED
void mu_getlst(char *name, int4 size);
typedef struct usr_reg_que_struct
{
	struct
	{
		struct 	usr_reg_que_struct	*fl,
						*bl;
	} que;
	char *usr_reg;
	unsigned short usr_reg_len;
} usr_reg_que;
static bool usr_reg_que_checkdup(unsigned char *usr_reg, unsigned short usr_reg_len);
#endif /* MU_GETLST_INCLUDED */
