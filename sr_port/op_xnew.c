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

#include <stdarg.h>

#include "gtm_string.h"
#include "gtm_stdio.h"

#include "lv_val.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "op.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "alias.h"

GBLREF lv_xnew_var	*xnewvar_anchor;
GBLREF lv_xnew_ref	*xnewref_anchor;
GBLREF uint4		lvtaskcycle;
GBLREF symval		*curr_symval;
GBLREF unsigned char	*stackbase, *msp;

/* Exclusive new - Operation:

   1) Push a new symtab onto the stack via mv_stent entry
   2) For each var name listed that is not to be NEW'd:
      a) lookup the var in the old symbol table (create if does not exist)
      b) add the symbol to the new symbol table.
      c) copy the lv_val address from the old symbol table to the new symbol
         table.
      d) save the var name in a chain off of the symtab so we know which vars
         need special alias consideration processing when the symtab pops
	 in umw_mv_stent().
*/
void op_xnew(UNIX_ONLY_COMMA(unsigned int argcnt_arg) mval *s_arg, ...)
{
	va_list 	var;
	unsigned int 	argcnt;
	mval 		*s;
	int4 		shift;
	var_tabent	lvent;
	ht_ent_mname	*tabent1, *tabent2;
	hash_table_mname *htold, *htnew;
	lv_xnew_var	*xnewvar;
	lv_val		*lvp, *lvtab1;
	boolean_t	added;

	VMS_ONLY(va_count(argcnt));
	UNIX_ONLY(argcnt = argcnt_arg);		/* need to preserve stack copy on i386 */
	htold = &curr_symval->h_symtab;
	shift = symbinit();
	DBGRFCT((stderr, "\n\n****op_xnew: **** New symbol table (0x"lvaddr") replaced previous table (0x"lvaddr")\n\n",
		 curr_symval, curr_symval->last_tab));
	if (0 >= argcnt)
		return;		/* done if no arguments */
	INCR_LVTASKCYCLE;	/* So we don't process referenced base vars more than once */
	htnew = &curr_symval->h_symtab;
	assert(curr_symval->last_tab);
	assert(htold == &curr_symval->last_tab->h_symtab);
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
		lvent.marked = FALSE;
		if (add_hashtab_mname_symval(htold, &lvent, NULL, &tabent1))
			lv_newname(tabent1, curr_symval->last_tab);
		lvtab1 = (lv_val *)tabent1->value;
		assert(lvtab1);
		assert(LV_IS_BASE_VAR(lvtab1));
		added = add_hashtab_mname_symval(htnew, &lvent, NULL, &tabent2);
		if (added)
		{	/* This var has NOT been specified twice */
			tabent2->value = tabent1->value;
			lvp = (lv_val *)tabent2->value;
			if (lvp && (IS_ALIASLV(lvp) || lvp->has_aliascont))
				curr_symval->alias_activity = TRUE; 		/* Loading an alias into new symtab counts as
										 * activity! */
			if (NULL != xnewvar_anchor)
			{	/* Reuse entry from list */
				xnewvar = xnewvar_anchor;
				xnewvar_anchor = xnewvar->next;
			} else
				xnewvar = (lv_xnew_var *)malloc(SIZEOF(lv_xnew_var));
			xnewvar->key = tabent1->key;	/* Note "value" in this key is not used since it is not sync'd */
			xnewvar->lvval = lvtab1;
			xnewvar->next = curr_symval->xnew_var_list;
			curr_symval->xnew_var_list = xnewvar;
			INCR_CREFCNT(lvtab1);
			INCR_TREFCNT(lvtab1);
			/* Note that there is no attempt to prevent double processing an lvval that has been indicated
			   through more than one (aliased) var being passed through. This is because these vars could be
			   independent by the time they come back through when the symtab pops. So we will handle them the
			   same way then.
			*/
			lvtab1->stats.lvtaskcycle = lvtaskcycle;
		} else
			/* Apparently var was specified twice -- better have value set as we expect it */
			assert(tabent1->value == tabent2->value);
		if (0 < --argcnt)
			s = va_arg(var, mval*);
		else
			break;
	}
	va_end(var);
	/* Now that all the passed-thru vars have been identified and recorded, go through them again to process their arrays
	   and search for containers pointing to other arrays that we need to record */
	if (curr_symval->alias_activity)
	{
		for (xnewvar = curr_symval->xnew_var_list; xnewvar; xnewvar = xnewvar->next)
		{
			lvtab1 = xnewvar->lvval;
			if (LV_HAS_CHILD(lvtab1))
				XNEWREF_CNTNRS_IN_TREE(lvtab1);
		}
	}
	return;
}
