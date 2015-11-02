/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef VALID_MNAME_H
#define VALID_MNAME_H

LITREF char 		ctypetab[NUM_CHARS];

#define VALID_MNAME_FCHAR(mchar) ((unsigned char)(mchar) < NUM_ASCII_CHARS && (ctypetab[mchar] == TK_UPPER || \
		ctypetab[mchar] == TK_LOWER || ctypetab[mchar] == TK_PERCENT))
#define VALID_MNAME_NFCHAR(mchar) ((unsigned char)(mchar) < NUM_ASCII_CHARS && (ctypetab[mchar] == TK_UPPER \
		|| ctypetab[mchar] == TK_LOWER || ctypetab[mchar] == TK_DIGIT))

boolean_t valid_mname(mstr *targ);
boolean_t valid_labname(mstr *targ);

#endif
