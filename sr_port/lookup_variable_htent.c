/****************************************************************
 *								*
 *	Copyright 2009, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_string.h"

#include "lv_val.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "lookup_variable_htent.h"
#include "alias.h"

GBLREF symval		*curr_symval;
GBLREF stack_frame	*frame_pointer;

ht_ent_mname *lookup_variable_htent(unsigned int x)
{
	ht_ent_mname	*tabent;
	mident_fixed	varname;
	boolean_t	added;

	assert(x < frame_pointer->vartab_len);
	added = add_hashtab_mname_symval(&curr_symval->h_symtab, ((var_tabent *)frame_pointer->vartab_ptr + x), NULL, &tabent);
	assert(tabent);
	if (NULL == tabent->value)
	{
		assert(added);		/* Should never be a valid name without an lv */
#ifdef DEBUG_REFCNT
		memset(varname.c, '\0', SIZEOF(varname));
		memcpy(varname.c, tabent->key.var_name.addr, tabent->key.var_name.len);
		DBGRFCT((stderr, "lookup_variable_htent: Allocating lv_val for variable '%s'\n", varname.c));
#endif
		lv_newname(tabent, curr_symval);
	}
	assert(NULL != LV_GET_SYMVAL((lv_val *)tabent->value));
	return tabent;
}
