/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

#define VALID_MNAME_FCHAR(MCHAR) (((NUM_ASCII_CHARS > (unsigned char)MCHAR)) && ((TK_UPPER == ctypetab[MCHAR])		\
		|| (TK_LOWER == ctypetab[MCHAR]) || (TK_PERCENT == ctypetab[MCHAR])))
#define VALID_MNAME_NFCHAR(MCHAR) (((NUM_ASCII_CHARS > (unsigned char)MCHAR)) && ((TK_UPPER == ctypetab[MCHAR])	\
		|| (TK_LOWER == ctypetab[MCHAR]) || (TK_DIGIT == ctypetab[MCHAR])))
#define VALID_OBJNAME_FCHAR(MCHAR) (((NUM_ASCII_CHARS > (unsigned char)MCHAR)) && ((TK_UPPER == ctypetab[MCHAR])	\
		|| (TK_LOWER == ctypetab[MCHAR]) || (TK_UNDERSCORE == ctypetab[MCHAR])))

boolean_t valid_mname(mstr *targ);
boolean_t valid_labname(mstr *targ);

#endif
