/****************************************************************
 *								*
 * Copyright (c) 2001-2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

#include "rtnhdr.h"
#include "cmd_qlf.h"
#include "stack_frame.h"
#include "lv_val.h"
#include "min_max.h"

GBLREF stack_frame	*frame_pointer;

unsigned char	*format_lvname(lv_val *startlv, unsigned char *buff, int size)
{
	int		i, len;
	ht_ent_mname	**j;
	mident		*vent, lcl_vent;
	stack_frame	*fp;

	if (!startlv)
		return buff;
	if (frame_pointer->old_frame_pointer && frame_pointer->flags & SFF_PARM_IN_PROG)
		fp = frame_pointer->old_frame_pointer;
	else
		fp = frame_pointer;
	if ((startlv >= (lv_val *)fp->temps_ptr)
			&& (startlv <= (lv_val *)(fp->temps_ptr + fp->rvector->temp_size)))
		return buff;
	for (i = 0, j = fp->l_symtab;  i < fp->vartab_len;  i++, j++)
	{
		if (*j && (lv_val *)((*j)->value) == startlv)
			break;
	}
	if (i >= fp->vartab_len)
		return buff;
	vent = &((fp->vartab_ptr)[i].var_name);
	assert(!DYNAMIC_VARNAMES_ACTIVE(fp) || (INDIR_MARKED != ((fp->vartab_ptr)[i].marked)));
	if ((INDIR_MARKED != ((fp->vartab_ptr)[i].marked)) && DYNAMIC_VARNAMES_ACTIVE(fp))
	{
		lcl_vent = *vent;
		vent = &lcl_vent;
		RELOCATE(vent->addr, char *, fp->rvector->literal_text_adr);
	}
	assert(vent->len <= MAX_MIDENT_LEN);
	len = MIN(size, vent->len);
	memcpy(buff, vent->addr, len);
	return buff + len;
}
