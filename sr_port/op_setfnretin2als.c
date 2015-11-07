/****************************************************************
 *								*
 *	Copyright 2010, 2013 Fidelity Information Services, Inc	*
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
#include "min_max.h"

GBLREF stack_frame	*frame_pointer;
GBLREF symval		*curr_symval;
GBLREF uint4		dollar_tlevel;
GBLREF lv_val		*active_lv;
GBLREF mval		*alias_retarg;

/*  Operation - The destination variable becomes a new alias of the data pointed to by the source container variable:
 *
 *  1) Index into the variable name table to get the variable name for the new alias (destination).
 *  2) Create the hash for the variable and look up in hash table (adding if not yet created).
 *  3) See if hash entry has a value pointer. If yes and same as source var, we are done (NOOP).
 *  4) Set hash entry to point to new lv_val.
 *  5) Whichever symval of source container or the alias it pointed to has the largest address, that symval is the
 *     earliest symval affected by this alias action. Mark all interveening symvals (end points included) as having
 *     had alias activity via MARK_ALIAS_ACTIVE macro. This is so we can properly handle issues in an exlusive NEW
 *     situation involving multiple symvals and potential aliasing between them.
 *
 *  Note that this opcode's function is very similar to "op_setalsctin2als" but is necessarily different because the
 *  source container is in a temporary mval passed back through a function rather than the lv_val that "op_setalsctin2als"
 *  deals with. Consequently, the amount of verification we can do is reduced. But this is acceptable due to the checks
 *  done by "unw_retarg" and "op_exfunretals" which pre-processed this value for us. There is also different reference
 *  count maintenance to do than the "op_setalsctin2als" opcode. With substantially more work to reorganize how SET
 *  operates, it would likely be possible to combine these functions but the way things are structured now, all the
 *  set functions plus "op_sto" share the same API so adding a parm to one means adding a useless parm to all 6 of
 *  them which is not acceptable so we end up duplicating portions of code here.
 */
void op_setfnretin2als(mval *srcmv, int destindx)
{
	ht_ent_mname	*tabent;
	mname_entry	*varname;
	lv_val		*srclvc, *dstlv;
	int4		srcsymvlvl;
	boolean_t	added;

	assert(alias_retarg == srcmv);
	assert(srcmv);
	assert(srcmv->mvtype & MV_ALIASCONT);
	assert(MVAL_IN_RANGE(srcmv, frame_pointer->temps_ptr, frame_pointer->temp_mvals));	/* Verify is a temp mval */
	srclvc = (lv_val *)srcmv->str.addr;
	assert(srclvc);
	assert(LV_IS_BASE_VAR(srclvc));	/* Verify base var */
	assert(srclvc->stats.trefcnt >= srclvc->stats.crefcnt);
	assert(1 <= srclvc->stats.crefcnt);		/* Verify we have an existing container reference */
	srcsymvlvl = LV_SYMVAL(srclvc)->symvlvl;	/* lv_val may go away below so record symlvl */
	varname = &(((mname_entry *)frame_pointer->vartab_ptr)[destindx]);
	DEBUG_ONLY(added = FALSE);
	/* Find hash table entry */
	if (NULL == (tabent = (ht_ent_mname *)frame_pointer->l_symtab[destindx]))	/* note tabent assignment */
	{	/* No fast path to hash table entry -- look it up the hard(er) way */
		varname = &(((mname_entry *)frame_pointer->vartab_ptr)[destindx]);
		added = add_hashtab_mname_symval(&curr_symval->h_symtab, varname, NULL, &tabent);
	}
	assert(tabent);
	assert(tabent || added);
	dstlv = (lv_val *)tabent->value;
	if (NULL == dstlv && curr_symval->tp_save_all)
	{	/* dstlv does not exist yet we need to be able to save a previous "novalue" lvval in case a TPRESTART
		   needs to restore the value. Create a var so its undefined status can be saved.
		*/
		lv_newname(tabent, curr_symval);
		dstlv = (lv_val *)tabent->value;
		assert(dstlv);
	}
	/* No need to increment before dstlv processing to prevent removal of last reference to srclvc in this case because
	 * the increment has already been done in "unw_retarg".
	 */
	if (dstlv)
	{
		assert(LV_IS_BASE_VAR(dstlv));	/* Verify base var */
		if (dollar_tlevel && NULL != dstlv->tp_var && !dstlv->tp_var->var_cloned)
			TP_VAR_CLONE(dstlv);
		assert(0 < dstlv->stats.trefcnt);
		assert(0 < (dstlv->stats.trefcnt - dstlv->stats.crefcnt)); /* Make sure there is one non-container reference */
		DECR_TREFCNT(dstlv);
		assert(dstlv->stats.trefcnt >= dstlv->stats.crefcnt);
		if (0 == dstlv->stats.trefcnt)
		{	/* Non alias -- make room for an alias to live here instead */
			lv_kill(dstlv, DOTPSAVE_TRUE, DO_SUBTREE_TRUE);
			LV_FREESLOT(dstlv);
		} /* Else alias pointer in the hash table is just replaced below */
	}
	DECR_CREFCNT(srclvc);	/* In "unw_retarg" we incremented for a container but is now "just" an alias so get rid of
				 * the container count from the temp return parm */
	frame_pointer->l_symtab[destindx] = tabent;
	DBGRFCT((stderr, "op_setfnret2als: hte 0x"lvaddr" being reset from 0x"lvaddr" to 0x"lvaddr"\n",
		 tabent, tabent->value, srclvc));
	tabent->value = (void *)srclvc;
	/* These symvals have had alias activity - Note the possibility of re-marking srcsymvlvl is not necessarily re-doing
	 * the mark done by "unw_retarg" since the source lv_val may have been re-created if it was originally in an xnew'd
	 * symtab which popped during the return.
	 */
	MARK_ALIAS_ACTIVE(MIN(srcsymvlvl, LV_SYMVAL(srclvc)->symvlvl));
	active_lv = (lv_val *)NULL;	/* if we get here, subscript set was successful.  clear active_lv to avoid later
					   cleanup problems */
	alias_retarg = NULL;
}
