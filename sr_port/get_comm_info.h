/****************************************************************
 *								*
 * Copyright (c) 2020-2021 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef GET_COMM_INFO_INCLUDED
#define GET_COMM_INFO_INCLUDED
void get_comm_info(void);

#define PROCESS_NAME_LENGTH 32 		/* estimate of max length of text in comm file */

GBLREF char process_name[PROCESS_NAME_LENGTH];

#endif
