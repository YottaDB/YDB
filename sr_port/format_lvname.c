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

#include "gtm_string.h"

#include <rtnhdr.h>
#include "stack_frame.h"
#include "lv_val.h"
#include "min_max.h"

GBLREF stack_frame	*frame_pointer;

unsigned char	*format_lvname(lv_val *startlv, unsigned char *buff, int size)
{
	int		i, len;
	ht_ent_mname	**j;
	mident		*vent;

	if (!startlv)
		return buff;
	if ((startlv >= (lv_val *)frame_pointer->temps_ptr)
			&& (startlv <= (lv_val *)(frame_pointer->temps_ptr + frame_pointer->rvector->temp_size)))
		return buff;
	for (i = 0, j = frame_pointer->l_symtab;  i < frame_pointer->vartab_len;  i++, j++)
	{
		if (*j && (lv_val *)((*j)->value) == startlv)
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
