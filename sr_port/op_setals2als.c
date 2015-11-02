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

#include <rtnhdr.h>
#include "stack_frame.h"
#include "op.h"
#include "lv_val.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"

GBLREF stack_frame	*frame_pointer;
GBLREF symval		*curr_symval;
GBLREF uint4		dollar_tlevel;
GBLREF lv_val		*active_lv;

/* Operation - The destination variable becomes a new alias of the source variable:
 * 1) Index into the variable name table to get the variable name for the new alias (destination).
 * 2) Create the hash for the variable and look up in hash table (adding if not yet created).
 * 3) See if hash entry has a value pointer. If yes and same as source var, we are done (NOOP).
 * 4) If var exists, if alias reduce refcnt, else delete the existing var.
 * 5) Reset hashtable to point to new lv_val.
 * 6) Mark the affected symval (and any preceding it) as having had symval activity.
 */
void op_setals2als(lv_val *srclv, int destindx)
{
	ht_ent_mname	*tabent;
	mname_entry	*varname;
	lv_val		*dstlv;
	boolean_t	added;

	assert(srclv);
	assert(LV_IS_BASE_VAR(srclv));	/* Verify base var */
	DEBUG_ONLY(added = FALSE);
	/* Find hash table entry */
	if (NULL == (tabent = (ht_ent_mname *)frame_pointer->l_symtab[destindx]))	/* note tabent assignment */
	{	/* No fast path to hash table entry -- look it up the hard(er) way */
		varname = &(((mname_entry *)frame_pointer->vartab_ptr)[destindx]);
		added = add_hashtab_mname_symval(&curr_symval->h_symtab, varname, NULL, &tabent);
	}
	assert(tabent);
	dstlv = (lv_val *)tabent->value;
	assert(dstlv || added);
	if (dstlv == srclv)
		/* Assignment of self or existing alias, nothing need be done */
		return;
	if ((NULL == dstlv) && curr_symval->tp_save_all)
	{	/* dstlv does not exist yet we need to be able to save a previous "novalue" lvval in case a TPRESTART
		   needs to restore the value. Create a var so its undefined status can be saved.
		*/
		lv_newname(tabent, curr_symval);
		dstlv = (lv_val *)tabent->value;
		assert(dstlv);
	}
	/* Note dstlv could still be NULL if this is the first assignment to the target and the var is not being TPSAVed */
	if (dstlv)
	{
		assert(LV_IS_BASE_VAR(dstlv));	/* Verify base var */
		if (dollar_tlevel && (NULL != dstlv->tp_var) && !dstlv->tp_var->var_cloned)
			TP_VAR_CLONE(dstlv);
		DECR_BASE_REF_NOSYM(dstlv, TRUE);
	}
	frame_pointer->l_symtab[destindx] = tabent;
	DBGRFCT((stderr, "op_setals2als: hte 0x"lvaddr" is reset from 0x"lvaddr" to 0x"lvaddr"\n", tabent, tabent->value, srclv));
	tabent->value = (void *)srclv;
	INCR_TREFCNT(srclv);
	MARK_ALIAS_ACTIVE(LV_SYMVAL(srclv)->symvlvl);		/* This symval has had alias activity */
	active_lv = (lv_val *)NULL;	/* if we get here, subscript set was successful.  clear active_lv to avoid later
					   cleanup problems */
}
