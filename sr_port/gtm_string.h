/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* If this is not the vax, define string.h. This is because the Vax
   has its own built-in instructions for string manipulation.
*/
#ifndef GTM_STRINGH
#define GTM_STRINGH

#ifndef __vax
#  include <string.h>
#endif

#define STRERROR	strerror

#define	STRCPY(SOURCE, DEST)		strcpy((char *)(SOURCE), (char *)(DEST))
#define STRNCPY_LIT(SOURCE, LITERAL)	strncpy((char *)(SOURCE), (char *)(LITERAL), SIZEOF(LITERAL) - 1)	/* BYPASSOK */
#define	STRNCPY_STR(SOURCE, STRING, LEN) strncpy((char *)(SOURCE), (char *)(STRING), LEN)

#define	STRCMP(SOURCE, DEST)		strcmp((char *)(SOURCE), (char *)(DEST))
#define	STRNCMP_LIT(SOURCE, LITERAL)	strncmp(SOURCE, LITERAL, SIZEOF(LITERAL) - 1)		/* BYPASSOK */
#define	STRNCMP_STR(SOURCE, STRING, LEN) strncmp(SOURCE, STRING, LEN)

#endif
