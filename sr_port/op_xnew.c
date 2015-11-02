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

#include "mdef.h"

#include <stdarg.h>
#include "gtm_string.h"

#include "hashtab_mname.h"	/* needed for lv_val.h */
#include "hashtab.h"
#include "lv_val.h"
#include "op.h"
#include "rtnhdr.h"
#include "stack_frame.h"

GBLREF symval		*curr_symval;
GBLREF unsigned char	*stackbase, *msp;

/* SHOULD BE MODIFIED TO USE AN EXISTING POINTER TO SYMTAB */
void op_xnew(UNIX_ONLY_COMMA(unsigned int argcnt_arg) mval *s_arg, ...)
{
	va_list 	var;
	unsigned int 	argcnt;
	mval 		*s;
	int4 		shift;
	var_tabent	lvent;
	ht_ent_mname	*tabent1, *tabent2;

	VMS_ONLY(va_count(argcnt);)
	UNIX_ONLY(argcnt = argcnt_arg;)		/* need to preserve stack copy on i386 */
	shift = symbinit();
	if (0 >= argcnt)
		return;		/* done if no arguments */
	VAR_START(var, s_arg);
	s = s_arg;
	for ( ; ; )
	{
		if ((unsigned char *)s >= msp && (unsigned char *)s < stackbase)
			s = (mval*)((char*)s - shift);		/* Only if stack resident */
		lvent.var_name.len = s->str.len;
		lvent.var_name.addr = s->str.addr;
		if (MAX_MIDENT_LEN < lvent.var_name.len)
			lvent.var_name.len = MAX_MIDENT_LEN;
		COMPUTE_HASH_MNAME(&lvent);
		assert(curr_symval->last_tab);
		if (add_hashtab_mname(&curr_symval->last_tab->h_symtab, &lvent, NULL, &tabent1))
			lv_newname(tabent1, curr_symval->last_tab);
		add_hashtab_mname(&curr_symval->h_symtab, &lvent, NULL, &tabent2);
		tabent2->value = tabent1->value;
		if (0 < --argcnt)
			s = va_arg(var, mval*);
		else
			break;
	}
	va_end(var);
	return;
}
