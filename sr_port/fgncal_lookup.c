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

#include "mdef.h"

#include "toktyp.h"
#include "lv_val.h"	/* needed by "fgncal.h" */
#include "fgncal.h"
#include "valid_mname.h"
#include <rtnhdr.h>

GBLREF	symval			*curr_symval;

mval *fgncal_lookup(mval *x)
{
	mval		*ret_val;
	ht_ent_mname 	*tabent;
	var_tabent	targ_key;
	mident		ident;

	MV_FORCE_DEFINED(x);
	assert(MV_IS_STRING(x));
	ret_val = NULL;
	ident = x->str;
	if (ident.len > MAX_MIDENT_LEN)
		ident.len = MAX_MIDENT_LEN;
	if (valid_mname(&ident))
	{
		targ_key.var_name = ident;
		COMPUTE_HASH_MNAME(&targ_key);
		targ_key.marked = FALSE;
		if (add_hashtab_mname_symval(&curr_symval->h_symtab, &targ_key, NULL, &tabent))
			lv_newname(tabent, curr_symval);
		ret_val = (mval *) tabent->value;
	}
	return ret_val;
}
