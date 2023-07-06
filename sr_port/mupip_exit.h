/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MUPIP_EXIT_INCLUDED
#define MUPIP_EXIT_INCLUDED

void mupip_exit(int4 stat) __attribute__ ((noreturn));
void mupip_exit_handler(void);

#endif /* MUPIP_EXIT_INCLUDED */
