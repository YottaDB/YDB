/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 *---------------------------------------------------------------------------------
 * Description:
 *	Given lv_val * of the local variable name, format the local variable name string
 *
 * Input Parameter:
 *	start: Pointer to the local variable name
 * 	buff: Buffer where key will be formatted
 * 	size: Size of buff
 *
 * Return Value:
 *	End address upto which buffer was used to format the key
 *		(Needed for the length calculation in caller)
 *---------------------------------------------------------------------------------
 */

#include "mdef.h"

#include <varargs.h>
#include "gtm_string.h"

#include "rtnhdr.h"
#include "stack_frame.h"
#include "hashtab_mname.h"	/* needed for lv_val.h */
#include "lv_val.h"
#include "sbs_blk.h"
#include "min_max.h"

GBLREF stack_frame	*frame_pointer;

unsigned char	*format_lvname(lv_val *start, unsigned char *buff, int size)
{
	int		i, len;
	mval		**j, *startmv;
	mident		*vent;

	if (!start)
		return buff;
	startmv = (mval *)start;
	if (   startmv >= (mval *) frame_pointer->temps_ptr
	    && startmv <= (mval *) (frame_pointer->temps_ptr + frame_pointer->rvector->temp_size))
	{
		return buff;
	}

	for (i = 0, j = frame_pointer->l_symtab;  i < frame_pointer->vartab_len;  i++, j++)
	{
		if (*j == startmv)
			break;
	}

	if (i >= frame_pointer->vartab_len)
		return buff;

	vent = &(((var_tabent *)frame_pointer->vartab_ptr)[i].var_name);
	assert(vent->len <= MAX_MIDENT_LEN);
	len = MIN(size, vent->len);
	memcpy(buff, vent->addr, len);
	return buff + len;
}
