/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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

#ifdef sparc
#include "cachectl.h"
#include "cacheflush.h"
#endif
#include "op.h"

GBLREF mv_stent		*mv_chain;
GBLREF unsigned char	*stackbase, *stacktop, *msp, *stackwarn;
GBLREF stack_frame	*frame_pointer;
GBLREF symval		*curr_symval;

void op_newvar(uint4 arg1)
{
	mv_stent 	*mv_st_ent, *mvst_tmp, *mvst_prev;
	ht_entry	*hte;
	stack_frame	*fp, *fp_prev, *fp_fix;
	unsigned char	*old_sp, *top;
	lv_val		*new;
	mident		*varname;
	mvs_ntab_struct *ptab;
	int		indx;

	error_def(ERR_STACKOFLOW);
	error_def(ERR_STACKCRIT);

	varname = &(((vent *)frame_pointer->vartab_ptr)[arg1]);
	hte = ht_get(&curr_symval->h_symtab, (mname *)varname);
	assert(hte);	/* variable must be defined and fetched by this point */
	if (frame_pointer->type & SFT_COUNT)
	{
		if (varname > (mident *)stacktop && varname < (mident *)frame_pointer)
		{
			PUSH_MV_STENT(MVST_NVAL);
			mv_st_ent = mv_chain;
			new = mv_st_ent->mv_st_cont.mvs_nval.mvs_val = lv_getslot(curr_symval);
			ptab = &mv_st_ent->mv_st_cont.mvs_nval.mvs_ptab;
			varname = &mv_st_ent->mv_st_cont.mvs_nval.name;
			memcpy(varname, &(((vent *)frame_pointer->vartab_ptr)[arg1]), sizeof(*varname));
		} else
		{
			PUSH_MV_STENT(MVST_PVAL);
			mv_st_ent = mv_chain;
			new = mv_st_ent->mv_st_cont.mvs_pval.mvs_val = lv_getslot(curr_symval);
			ptab = &mv_st_ent->mv_st_cont.mvs_pval.mvs_ptab;
		}
		/* save symtab that's older than the current frame */
		if (frame_pointer->l_symtab > (mval **)frame_pointer)
			ptab->lst_addr = (lv_val **)&frame_pointer->l_symtab[arg1];
		else
			ptab->lst_addr = NULL;
		frame_pointer->l_symtab[arg1] = (mval *)new;
		assert(0 <= arg1);
	} else
	{
		fp = frame_pointer;
		fp_prev = fp->old_frame_pointer;
		assert(fp_prev);
		while (!(fp_prev->type & SFT_COUNT))
		{
			fp = fp_prev;
			fp_prev = fp->old_frame_pointer;
			assert(fp_prev);
		}
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
		memcpy(msp, old_sp, top - (unsigned char *)old_sp);
		mv_st_ent = (mv_stent *)(top - mvs_size[MVST_NVAL]);
		mv_st_ent->mv_st_type = MVST_NVAL;
		frame_pointer = (stack_frame *)((char *)frame_pointer - mvs_size[MVST_NVAL]);
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
		if ((unsigned char *)mv_chain >= top)
		{
			mv_st_ent->mv_st_next = (char *)mv_chain - (char *)mv_st_ent;
			mv_chain = mv_st_ent;
		} else
		{
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
		memcpy(varname, &(((vent *)frame_pointer->vartab_ptr)[arg1]), sizeof(*varname));

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
