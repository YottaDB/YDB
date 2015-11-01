/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "hashdef.h"
#include "lv_val.h"
#include "mv_stent.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "tp_frame.h"
#include "op.h"
#include "gtm_string.h"

GBLREF mv_stent		*mv_chain;
GBLREF unsigned char	*stackbase, *stacktop, *msp, *stackwarn;
GBLREF stack_frame	*frame_pointer;
GBLREF tp_frame		*tp_pointer;
GBLREF symval		*curr_symval;
GBLREF short		dollar_tlevel;

/* Note this module follows the same basic pattern as gtm_newintrisic which handles
   the same function but for intrinsic vars instead of local vars. */
void op_newvar(uint4 arg1)
{
	mv_stent 	*mv_st_ent, *mvst_tmp, *mvst_prev;
	ht_entry	*hte;
	stack_frame	*fp, *fp_prev, *fp_fix;
	unsigned char	*old_sp, *top;
	lv_val		*new;
	mident		*varname;
	mvs_ntab_struct *ptab;
	tp_frame	*tpp;
	int		indx;

	error_def(ERR_STACKOFLOW);
	error_def(ERR_STACKCRIT);

	varname = &(((VAR_TABENT *)frame_pointer->vartab_ptr)[arg1]);
	hte = ht_get(&curr_symval->h_symtab, (mname *)varname);
	assert(hte);	/* variable must be defined and fetched by this point */
	if (frame_pointer->type & SFT_COUNT)
	{	/* Current (youngest) frame is NOT an indirect frame.
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
			varname = &mv_st_ent->mv_st_cont.mvs_nval.name;
			memcpy(varname, &(((VAR_TABENT *)frame_pointer->vartab_ptr)[arg1]), sizeof(*varname));
		}

		/* save symtab that's older than the current frame */
		if (frame_pointer->l_symtab > (mval **)frame_pointer)
			ptab->lst_addr = (lv_val **)&frame_pointer->l_symtab[arg1];
		else
			ptab->lst_addr = NULL;
		frame_pointer->l_symtab[arg1] = (mval *)new;
		assert(0 <= arg1);
	} else
	{	/* Current (youngest) frame IS an indirect frame.
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
		msp -= mvs_size[MVST_NVAL];
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
		mv_st_ent = (mv_stent *)(top - mvs_size[MVST_NVAL]);
		mv_st_ent->mv_st_type = MVST_NVAL;
		frame_pointer = (stack_frame *)((char *)frame_pointer - mvs_size[MVST_NVAL]);
		/* adjust all the pointers in all the stackframes that were moved */
		for (fp_fix = frame_pointer;  fp_fix != fp_prev;  fp_fix = fp_fix->old_frame_pointer)
		{
			if ((unsigned char *)fp_fix->l_symtab < top && (unsigned char *)fp_fix->l_symtab > stacktop)
				fp_fix->l_symtab = (mval **)((char *)fp_fix->l_symtab - mvs_size[MVST_NVAL]);
			if (fp_fix->temps_ptr < top && fp_fix->temps_ptr > stacktop)
				fp_fix->temps_ptr -= mvs_size[MVST_NVAL];
			if (fp_fix->vartab_ptr < (char *)top && fp_fix->vartab_ptr > (char *)stacktop)
				fp_fix->vartab_ptr -= mvs_size[MVST_NVAL];
			if ((unsigned char *)fp_fix->old_frame_pointer < top && (char *)fp_fix->old_frame_pointer
				> (char *)stacktop)
				fp_fix->old_frame_pointer =
					(stack_frame *)((char *)fp_fix->old_frame_pointer - mvs_size[MVST_NVAL]);
		}
		/* Adjust stackframe and mvstent pointers in relevant tp_frame blocks */
		assert((NULL == tp_pointer && 0 == dollar_tlevel) || (NULL != tp_pointer && 0 != dollar_tlevel));
		for (tpp = tp_pointer; (tpp && ((unsigned char *)tpp->fp < top)); tpp = tpp->old_tp_frame)
		{
			if ((unsigned char *)tpp->fp > stacktop)
				tpp->fp = (struct stack_frame_struct *)((char *)tpp->fp - mvs_size[MVST_NVAL]);
			/* Note low check for < top may be superfluous here but without a test case to verify, I
			   feel better leaving it in. SE 8/2001 */
			if ((unsigned char *)tpp->mvc < top && (unsigned char *)tpp->mvc > stacktop)
				tpp->mvc = (struct mv_stent_struct *)((char *)tpp->mvc - mvs_size[MVST_NVAL]);
		}
		/* Put new mvstent entry on (into) the mvstent chain */
		if ((unsigned char *)mv_chain >= top)
		{	/* Just put new entry on end of chain which preceeds our base frame */
			mv_st_ent->mv_st_next = (char *)mv_chain - (char *)mv_st_ent;
			mv_chain = mv_st_ent;
		} else
		{	/* One of the indirect frames has mv_stents associated with it so we have to find
			   the appropriate insertion point for this frame.
			*/
			fp = (stack_frame *)((char *)fp - mvs_size[MVST_NVAL]);
			mv_chain = (mv_stent *)((char *)mv_chain - mvs_size[MVST_NVAL]);
			mvst_tmp = mv_chain;
			mvst_prev = (mv_stent *)((char *)mvst_tmp + mvst_tmp->mv_st_next);
			while (mvst_prev < (mv_stent *)fp)
			{
				mvst_tmp = mvst_prev;
				mvst_prev = (mv_stent *)((char *)mvst_tmp + mvst_tmp->mv_st_next);
			}
			mvst_tmp->mv_st_next = (char *)mv_st_ent - (char *)mvst_tmp;
			mv_st_ent->mv_st_next = (char *)mvst_prev - (char *)mv_st_ent + mvs_size[MVST_NVAL];
		}
		new = mv_st_ent->mv_st_cont.mvs_nval.mvs_val = lv_getslot(curr_symval);
		ptab = &mv_st_ent->mv_st_cont.mvs_nval.mvs_ptab;
		varname = &mv_st_ent->mv_st_cont.mvs_nval.name;
		memcpy(varname, &(((VAR_TABENT *)frame_pointer->vartab_ptr)[arg1]), sizeof(*varname));

		/* For each (indirect) stack frame we have visited, find and set the new value of varname into the
		   stack frame. Note that varname might not exist in all frames.
		*/
		for (fp_fix = frame_pointer; ; fp_fix = fp_fix->old_frame_pointer)
		{
			for (indx = 0; indx < fp_fix->vartab_len; indx++)
			{
				if (0 == memcmp(&((mident *)fp_fix->vartab_ptr)[indx], varname, sizeof(*varname)))
					break;
			}
			if (fp_fix == fp_prev)		/* Have last substantive frame.. Set its value later */
				break;
			if (indx < fp_fix->vartab_len)
				fp_fix->l_symtab[indx] = (mval *)new;
		}
		/* Do frame type specific initialization of restoration structure. Save old value if the value
		   exists in this frame.
		*/
		if (indx < fp_fix->vartab_len)
		{
			ptab->lst_addr = (lv_val **)&fp_fix->l_symtab[indx];			/* save restore symtab entry */
			fp_fix->l_symtab[indx] = (mval *)new;
		} else
			ptab->lst_addr = NULL;
	}

	/* initialize new data cell */
	new->v.mvtype = 0;
	new->tp_var = NULL;
	new->ptrs.val_ent.children = 0;
	new->ptrs.val_ent.parent.sym = curr_symval;
	new->ptrs.free_ent.next_free = 0;

	/* finish initializing restoration structures */
	ptab->save_value = (lv_val *)hte->ptr;
	ptab->nam_addr = varname;
	hte->ptr = (char *)new;
	return;
}
