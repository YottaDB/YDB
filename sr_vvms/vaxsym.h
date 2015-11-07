/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef VAXSYM_H_INCLUDED
#define VAXSYM_H_INCLUDED

/* Maximum size of an package or external routine reference of the form routine^label */
#define MAX_EXTREF 	(2 * MAX_MIDENT_LEN + STR_LIT_LEN("^"))
#define ZCSYM_PREFIX	"__GTM$ZC"
#define MAX_SYMREF	SIZEOF(ZCSYM_PREFIX) + 2 * MAX_EXTREF	/* __GTM$ZC<package>.<extref> */

#endif /* VAXSYM_H_INCLUDED */
