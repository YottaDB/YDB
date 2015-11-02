/****************************************************************
 *      Copyright 2001 Sanchez Computer Associates, Inc.        *
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#include "mdef.h"
#include "mvalconv.h"
#include "op.h"
#include "is_canonic_name.h"

/*
 * -----------------------------------------------
 * op_fnqlength()
 * MUMPS QLength function
 *
 * Arguments:
 *	src	- Pointer to Source Name string mval
 *	dst	- destination buffer to save the piece in
 *
 * Return:
 *	none
 * -----------------------------------------------
 */
void op_fnqlength(mval *src, mval *dst)
{
        boolean_t	instring;
	int		isrc;
        int		subscripts;
        char		letter;
        char		*name;
	error_def(ERR_NOCANONICNAME);

	MV_FORCE_STR(src);
	if (!is_canonic_name(src))
		rts_error(VARLSTCNT(4) ERR_NOCANONICNAME, 2, src->str.len, src->str.addr);
	name    = src->str.addr;
        subscripts = 0;
        instring = FALSE;
        for (isrc = 0; isrc < src->str.len; isrc++)
        {
                letter = *name++;
                if (letter == '"')
                        instring = !instring;
                else
                        if ((!instring) && (('(' == letter) || (',' == letter)))
				subscripts++;
        }
        MV_FORCE_MVAL(dst, subscripts);
	return;
}
