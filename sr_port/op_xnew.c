/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
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

#include "gtmio.h"
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
#include "error.h"

GBLREF lv_xnew_ref	*xnewref_anchor;
GBLREF lv_xnew_var	*xnewvar_anchor;
GBLREF stack_frame	*frame_pointer;
GBLREF symval		*curr_symval;
GBLREF uint4		lvtaskcycle;
GBLREF unsigned char	*stackbase, *msp;

STATICDEF	stack_frame	*fp_save;

STATICFNDCL	void guard_against_zbzst(void);

CONDITION_HANDLER(zbreak_zstep_xnew_ch);

/* Exclusive new - Operation:
 *
 * 1) Push a new symtab onto the stack via mv_stent entry
 * 2) For each var name listed that is not to be NEW'd:
 *    a) lookup the var in the old symbol table (create if does not exist)
 *    b) add the symbol to the new symbol table.
 *    c) copy the lv_val address from the old symbol table to the new symbol
 *       table.
 *    d) save the var name in a chain off of the symtab so we know which vars
 *       need special alias consideration processing when the symtab pops
 *	 in umw_mv_stent().
 */

void op_xnew(UNIX_ONLY_COMMA(unsigned int argcnt_arg) mval *s_arg, ...)
{
	boolean_t		added;
	ht_ent_mname		*tabent1, *tabent2;
	hash_table_mname	*htold, *htnew;
	int			argcnt;
	int4			shift;
	lv_val			*lvp, *lvtab1;
	lv_xnew_var		*xnewvar;
	mval			*s;
	va_list			var;
	var_tabent		lvent;

	VMS_ONLY(va_count(argcnt));
	UNIX_ONLY(argcnt = argcnt_arg);		/* need to preserve stack copy on i386 */
	htold = &curr_symval->h_symtab;
	shift = symbinit();
	DBGRFCT((stderr, "\n\n****op_xnew: **** New symbol table (0x"lvaddr") replaced previous table (0x"lvaddr")\n\n",
		 curr_symval, curr_symval->last_tab));
	if (0 >= argcnt)
	{
		if (shift)
			guard_against_zbzst();
		return;
	}
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
			lvp = (lv_val *)tabent2->value;				/* Same lv_val pointed to by both hashtabs */
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
			 * through more than one (aliased) var being passed through. This is because these vars could be
			 * independent by the time they come back through when the symtab pops. So we will handle them the
			 * same way then.
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
	 * and search for containers pointing to other arrays that we need to record.
	 */
	if (curr_symval->alias_activity)
	{
		for (xnewvar = curr_symval->xnew_var_list; xnewvar; xnewvar = xnewvar->next)
		{
			lvtab1 = xnewvar->lvval;
			XNEWREF_CNTNRS_IN_TREE(lvtab1);				/* note macro has LV_GET_CHILD exists check */
		}
	}
	if (shift)
		guard_against_zbzst();
	return;
}

CONDITION_HANDLER(zbreak_zstep_xnew_ch)
{
	START_CH(TRUE);
	frame_pointer = fp_save;
	NEXTCH;
}

STATICFNDEF void guard_against_zbzst(void)
{	/* if the new symbol table slid into the stack, then we guard against ZBREAK and ZSTEP having taken us to the xnew
	 * this could be done sligthly more efficiently and with stronger asserts using custom code, but this is an edge case,
	 * so we use, with an empty argument list, gtm_fetch et al to do the job
	 */
	boolean_t	fetch;
	stack_frame	*fp, *fp_prev;

	fp = frame_pointer;
	assert(!(fp->type & SFT_COUNT));
	fp_prev = fp->old_frame_pointer;
	assert(fp_prev);
	fetch = (fp->type & (SFT_ZBRK_ACT | SFT_ZSTEP_ACT));
	while (!(fp_prev->type & SFT_COUNT))
	{	/* find where we put the new symbol table like we did in symbinit but with additional checking on frame types */
		fp = fp_prev;
		fetch |= (fp->type & (SFT_ZBRK_ACT | SFT_ZSTEP_ACT));
		fp_prev = fp->old_frame_pointer;
		assert(fp_prev);
	}
	if (fetch)
	{	/* only need to fetch if there is a zbreak or zstep frame in between our current position and the adjusted frame
		 * all other non SFT_COUNT frames either require a quit (SFT_ZINTR & SFT_TRIGR) which unstacks the symbol table
		 * protected by the NEW, or return to the beginning of the line (SFT_REP_OP & SFT_DEV_ACT & SFT_ZTRAP) which cause
		 * a refetch, or vector to a new virtual line (SFT_ZTRAP, but for $ZTRAP)
		 */
		ESTABLISH(zbreak_zstep_xnew_ch);
		fp_save = frame_pointer;
		frame_pointer = fp_prev;				/* pretend we're in the original frame */
		UNIX_ONLY(gtm_fetch(0, 0));				/* refetch everything in the old table into the new one */
		VMS_ONLY(fetch_all());					/* call an assembly interlude so VMS gets to gtm_fetch */
		frame_pointer = fp_save;				/* restore the proper frame_pointer */
		REVERT;
	}
}
