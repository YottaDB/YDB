/****************************************************************
 *								*
 * Copyright (c) 2009-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTM_STRINGSH
#define GTM_STRINGSH

#include <strings.h>

#define STRCASECMP(SOURCE, DEST)		strcasecmp((char *)(SOURCE), (char *)(DEST))
#define STRNCASECMP(SOURCE, DEST, LEN)		strncasecmp(SOURCE, DEST, LEN)
#define	STRNCASECMP_LIT(SOURCE, LITERAL)	strncasecmp(SOURCE, LITERAL, SIZEOF(LITERAL) - 1)		/* BYPASSOK */

#endif
