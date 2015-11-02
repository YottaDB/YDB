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

#include "gtm_stdio.h"
#include "gtm_string.h"

#include "lv_val.h"
#include <rtnhdr.h>
#include "mv_stent.h"
#include "stack_frame.h"
#include "tp_frame.h"
#include "op.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"

GBLREF mv_stent		*mv_chain;
GBLREF unsigned char	*stackbase, *stacktop, *msp, *stackwarn;
GBLREF stack_frame	*frame_pointer;
GBLREF tp_frame		*tp_pointer;
GBLREF symval		*curr_symval;
GBLREF uint4		dollar_tlevel;

/* Note this module follows the same basic pattern as gtm_newintrisic which handles
   the same function but for intrinsic vars instead of local vars. */
void op_newvar(uint4 arg1)
{
	mv_stent 	*mv_st_ent, *mvst_tmp, *mvst_prev;
	ht_ent_mname	*tabent;
	stack_frame	*fp, *fp_prev, *fp_fix;
	unsigned char	*old_sp, *top;
	lv_val		*new;
	var_tabent	*varname;
	mvs_ntab_struct *ptab;
	tp_frame	*tpp;
	int		indx;
	int4		shift_size;
	DBGRFCT_ONLY(mident_fixed vname;)

	error_def(ERR_STACKOFLOW);
	error_def(ERR_STACKCRIT);

	varname = &(((var_tabent *)frame_pointer->vartab_ptr)[arg1]);
	tabent = lookup_hashtab_mname(&curr_symval->h_symtab, varname);
	assert(tabent);	/* variable must be defined and fetched by this point */
	if (frame_pointer->type & SFT_COUNT)
	{	/* Current (youngest) frame IS a counted frame.
		   If the var being new'd exists in an earlier frame, we need to save
		   that value so it can be restored when we exit this frame. Since this
		   is a counted frame, just create a stack entry to save the old value.
		   If there was no previous entry, we will destroy the entry when we pop
		   off this frame (make it undefined again).
		*/
		if (!(frame_pointer->flags & SFF_INDCE))
		{	/* This is a normal counted frame with a stable variable name pointer */
			PUSH_MV_STENT(MVST_PVAL);
			mv_st_ent = mv_chain;
			new = mv_st_ent->mv_st_cont.mvs_pval.mvs_val = lv_getslot(curr_symval);
			ptab = &mv_st_ent->mv_st_cont.mvs_pval.mvs_ptab;
		} else
		{	/* This is actually an indirect (likely XECUTE or ZINTERRUPT) so the varname
			   pointer could be gone by the time we unroll this frame if an error occurs
			   while this frame is executing and error processing marks this frame reusable
			   so carry the name along with us to avoid this situation.
			*/
			PUSH_MV_STENT(MVST_NVAL);
			mv_st_ent = mv_chain;
			new = mv_st_ent->mv_st_cont.mvs_nval.mvs_val = lv_getslot(curr_symval);
			ptab = &mv_st_ent->mv_st_cont.mvs_nval.mvs_ptab;
			DEBUG_ONLY(mv_st_ent->mv_st_cont.mvs_nval.name = ((var_tabent *)frame_pointer->vartab_ptr)[arg1]);
			DEBUG_ONLY(varname = &mv_st_ent->mv_st_cont.mvs_nval.name);
		}
		assert((int)arg1 >= 0);
	} else
	{	/* Current (youngest) frame IS NOT a counted frame.
		   The situation is more complex because this is not a true stackframe.
		   It has full access to the base "counted" frame's vars and any new
		   done here must behave as if it were done in the base/counted frame.
		   To accomplish this, we actually find the base frame we are executing
		   in, then shift all frames younger than that up by the size of the mvstent
		   entry we need to save/restore the value being new'd and then go into
		   each frame modified and fixup all the addresses.
		 */
		fp = frame_pointer;
		fp_prev = fp->old_frame_pointer;
		assert(fp_prev);
		/* Find relevant base (counted) frame */
		while (!(fp_prev->type & SFT_COUNT))
		{
			fp = fp_prev;
			fp_prev = fp->old_frame_pointer;
			assert(fp_prev);
		}
		/* top is beginning of earliest indirect stackframe before counted base frame.
		   It is the point where we will shift to make room to insert an mv_stent into
		   the base frame.
		*/
		top = (unsigned char *)(fp + 1);
		old_sp = msp;
		shift_size = mvs_size[MVST_NVAL];
		msp -= shift_size;
	   	if (msp <= stackwarn)
	   	{
			if (msp <= stacktop)
			{
				msp = old_sp;
	   			rts_error(VARLSTCNT(1) ERR_STACKOFLOW);
			}
	   		else
	   			rts_error(VARLSTCNT(1) ERR_STACKCRIT);
	   	}
		/* Ready, set, shift the younger indirect frames to make room for mv_stent */
		memmove(msp, old_sp, top - (unsigned char *)old_sp);
		mv_st_ent = (mv_stent *)(top - shift_size);
		mv_st_ent->mv_st_type = MVST_NVAL;
		ADJUST_FRAME_POINTER(frame_pointer, shift_size);
		/* adjust all the pointers in all the stackframes that were moved */
		for (fp_fix = frame_pointer;  fp_fix != fp_prev;  fp_fix = fp_fix->old_frame_pointer)
		{
			if ((unsigned char *)fp_fix->l_symtab < top && (unsigned char *)fp_fix->l_symtab > stacktop)
				fp_fix->l_symtab = (ht_ent_mname **)((char *)fp_fix->l_symtab - shift_size);
			if (fp_fix->temps_ptr < top && fp_fix->temps_ptr > stacktop)
				fp_fix->temps_ptr -= shift_size;
			if (fp_fix->vartab_ptr < (char *)top && fp_fix->vartab_ptr > (char *)stacktop)
				fp_fix->vartab_ptr -= shift_size;
			if ((unsigned char *)fp_fix->old_frame_pointer < top && (char *)fp_fix->old_frame_pointer
				> (char *)stacktop)
			{
				ADJUST_FRAME_POINTER(fp_fix->old_frame_pointer, shift_size);
			}
		}
		/* Adjust stackframe and mvstent pointers in relevant tp_frame blocks */
		assert(((NULL == tp_pointer) && !dollar_tlevel) || ((NULL != tp_pointer) && dollar_tlevel));
		for (tpp = tp_pointer; (tpp && ((unsigned char *)tpp->fp < top)); tpp = tpp->old_tp_frame)
		{
			if ((unsigned char *)tpp->fp > stacktop)
				tpp->fp = (struct stack_frame_struct *)((char *)tpp->fp - shift_size);
			/* Note low check for < top may be superfluous here but without a test case to verify, I
			   feel better leaving it in. SE 8/2001 */
			if ((unsigned char *)tpp->mvc < top && (unsigned char *)tpp->mvc > stacktop)
				tpp->mvc = (struct mv_stent_struct *)((char *)tpp->mvc - shift_size);
		}
		/* Put new mvstent entry on (into) the mvstent chain */
		if ((unsigned char *)mv_chain >= top)
		{	/* Just put new entry on end of chain which preceeds our base frame */
			mv_st_ent->mv_st_next = (unsigned int)((char *)mv_chain - (char *)mv_st_ent);
			mv_chain = mv_st_ent;
		} else
		{	/* One of the indirect frames has mv_stents associated with it so we have to find
			   the appropriate insertion point for this frame.
			*/
			fp = (stack_frame *)((char *)fp - shift_size);
			mv_chain = (mv_stent *)((char *)mv_chain - shift_size);
			mvst_tmp = mv_chain;
			mvst_prev = (mv_stent *)((char *)mvst_tmp + mvst_tmp->mv_st_next);
			while (mvst_prev < (mv_stent *)fp)
			{
				mvst_tmp = mvst_prev;
				mvst_prev = (mv_stent *)((char *)mvst_tmp + mvst_tmp->mv_st_next);
			}
			mvst_tmp->mv_st_next = (unsigned int)((char *)mv_st_ent - (char *)mvst_tmp);
			mv_st_ent->mv_st_next = (unsigned int)((char *)mvst_prev - (char *)mv_st_ent + shift_size);
		}
		new = mv_st_ent->mv_st_cont.mvs_nval.mvs_val = lv_getslot(curr_symval);
		ptab = &mv_st_ent->mv_st_cont.mvs_nval.mvs_ptab;
		DEBUG_ONLY(mv_st_ent->mv_st_cont.mvs_nval.name = ((var_tabent *)frame_pointer->vartab_ptr)[arg1]);
		DEBUG_ONLY(varname = &mv_st_ent->mv_st_cont.mvs_nval.name);
	}

	/* initialize new data cell */
	LVVAL_INIT(new, curr_symval);

	/* finish initializing restoration structures */
	ptab->save_value = (lv_val *)tabent->value;
	ptab->hte_addr = tabent;
	DEBUG_ONLY(ptab->nam_addr = varname);
	DBGRFCT_ONLY(
		memcpy(vname.c, tabent->key.var_name.addr, tabent->key.var_name.len);
		vname.c[tabent->key.var_name.len] = '\0';
	);
	DBGRFCT((stderr, "op_newvar: Var '%s' hte 0x"lvaddr" being reset from 0x"lvaddr" to 0x"lvaddr"\n",
		 &vname.c, tabent, tabent->value, new));
	tabent->value = (char *)new;
}
