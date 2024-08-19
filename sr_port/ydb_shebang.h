/****************************************************************
 *								*
 * Copyright (c) 2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef YDB_SHEBANG_H_INCLUDED
#define YDB_SHEBANG_H_INCLUDED

#define	YDBSH		"ydbsh"	/* the executable name under $ydb_dist which corresponds to a shebang invocation */

#define	SHEBANG_CHAR1	'#'
#define	SHEBANG_CHAR2	'!'

/* Replace shebang with a semicolon (M comment character) to avoid compile errors (YDB#1084) */
#define	REPLACE_IF_SHEBANG_WITH_SEMICOLON(M)					\
{										\
	if (2 <= (M).len)							\
	{									\
		char	*buf;							\
										\
		buf = (M).addr;							\
		if ((SHEBANG_CHAR1 == buf[0]) && (SHEBANG_CHAR2 == buf[1]))	\
			buf[0] = ';';						\
	}									\
}

char *ydb_shebang(char *m_file_name, boolean_t *created_tmpdir);

#endif
