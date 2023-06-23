/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2021 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.                                     *
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MEM_ACCESS_H_INCLUDED
#define  MEM_ACCESS_H_INCLUDED

void set_noaccess(unsigned char *na_page[2], unsigned char *prvprt);
void reset_access(unsigned char *na_page[2], unsigned char oldprt);

#endif
