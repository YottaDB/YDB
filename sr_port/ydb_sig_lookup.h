/****************************************************************
 *								*
 * Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef YDB_SIGFWD_INIT_H
#define YDB_SIGFWD_INIT_H

/* Used for signal table defined in ydb_sigfwd_init.c */
typedef struct
{
	unsigned char	*signame;
	int		signameLen;
	int		sigvalue;
} signame_value;

int signal_lookup(unsigned char *signame, int signameLen);

#endif /* ifndef YDB_SIGFWD_INIT_H */
