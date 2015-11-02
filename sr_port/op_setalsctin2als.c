/****************************************************************
 *								*
 *	Copyright 2009 Fidelity Information Services, Inc	*
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

#include "rtnhdr.h"
#include "stack_frame.h"
#include "op.h"
#include "hashtab_mname.h"
#include "hashtab.h"
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
GBLREF short		dollar_tlevel;
GBLREF lv_val		*active_lv;

/* Operation - The destination variable becomes a new alias of the data pointed to by the source container variable:

   1) Index into the variable name table to get the variable name for the new alias (destination).
   2) Create the hash for the variable and look up in hash table (adding if not yet created).
   3) See if hash entry has a value pointer. If yes and same as source var, we are done (NOOP).
   4) Set hash entry to point to new lv_val.
   5) Whichever symval of source container or the alias it pointed to has the largest address, that symval is the
      earliest symval affected by this alias action. Mark all interveening symvals (end points included) as having
      had alias activity via MARK_ALIAS_ACTIVE macro. This is so we can properly handle issues in an exlusive NEW
      situation involving multiple symvals and potential aliasing between them.
*/
void op_setalsctin2als(lv_val *srclv, int destindx)
{
	ht_ent_mname	*tabent;
	mname_entry	*varname;
	lv_val		*srclvc, *dstlv, **free;
	int4		srcsymvlvl;
	boolean_t	added;

	error_def(ERR_ALIASEXPECTED);

	assert(srclv);
	assert(MV_SBS == srclv->ptrs.val_ent.parent.sbs->ident);  	/* Verify subscripted var */
	if (!(srclv->v.mvtype & MV_ALIASCONT))
		rts_error(VARLSTCNT(1) ERR_ALIASEXPECTED);
	srclvc = (lv_val *)srclv->v.str.addr;
	assert(srclvc);
	assert(MV_SYM == srclvc->ptrs.val_ent.parent.sym->ident);	/* Verify base var */
	assert(srclvc->stats.trefcnt >= srclvc->stats.crefcnt);
	assert(1 <= srclvc->stats.crefcnt);				/* Verify we have an existing container reference */
	srcsymvlvl = srclv->ptrs.val_ent.parent.sbs->sym->symvlvl;	/* lv_val may go away below so record symlvl */
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
	/* Note dstlv could still be NULL if this is the first assignment to the target and the var is not being TPSAVed */
	assert(dstlv != srclv);					/* Since source is container, this is an impossible situation */
	INCR_TREFCNT(srclvc);	/* Increment before dstlv processing to prevent removal of last reference to srclvc */
	if (dstlv)
	{
		if (dollar_tlevel && NULL != dstlv->tp_var && !dstlv->tp_var->var_cloned)
			TP_VAR_CLONE(dstlv);
		assert(0 < dstlv->stats.trefcnt);
		assert(0 < (dstlv->stats.trefcnt - dstlv->stats.crefcnt)); /* Make sure there is one non-container reference */
		DECR_TREFCNT(dstlv);
		assert(dstlv->stats.trefcnt >= dstlv->stats.crefcnt);
		if (0 == dstlv->stats.trefcnt)
		{	/* Non alias -- make room for an alias to live here instead */
			lv_kill(dstlv, TRUE);
			free = &dstlv->ptrs.val_ent.parent.sym->lv_flist;
			LV_FLIST_ENQUEUE(free, dstlv);
		} /* Else alias pointer in the hash table is just replaced below */
	}
	frame_pointer->l_symtab[destindx] = tabent;
	DBGRFCT((stderr, "op_setalsct2als: hte 0x"lvaddr" being reset from 0x"lvaddr" to 0x"lvaddr"\n",
		 tabent, tabent->value, srclvc));
	tabent->value = (void *)srclvc;
	/* These symvals have had alias activity */
	MARK_ALIAS_ACTIVE(MIN(srcsymvlvl, srclvc->ptrs.val_ent.parent.sym->symvlvl));
	active_lv = (lv_val *)NULL;	/* if we get here, subscript set was successful.  clear active_lv to avoid later
					   cleanup problems */
}
