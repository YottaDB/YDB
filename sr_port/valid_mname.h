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
#ifndef VALID_MNAME_H
#define VALID_MNAME_H

LITREF char 		ctypetab[NUM_CHARS];

static inline bool VALID_MNAME_FCHAR(unsigned char mchar)
{
	return ((NUM_ASCII_CHARS > mchar) && (TK_UPPER == ctypetab[mchar]))
			|| (TK_LOWER == ctypetab[mchar]) || (TK_PERCENT == ctypetab[mchar]);
}

static inline bool VALID_MNAME_NFCHAR(unsigned char mchar)
{
	return ((NUM_ASCII_CHARS > mchar) && (TK_UPPER == ctypetab[mchar]))
			|| (TK_LOWER == ctypetab[mchar]) || (TK_DIGIT == ctypetab[mchar]);
}
static inline bool VALID_OBJNAME_FCHAR(unsigned char mchar)
{
	return ((NUM_ASCII_CHARS > mchar) && (TK_UPPER == ctypetab[mchar]))
			|| (TK_LOWER == ctypetab[mchar]) || (TK_UNDERSCORE == ctypetab[mchar]);
}

boolean_t valid_mname(mstr *targ);
boolean_t valid_labname(mstr *targ);

#endif
