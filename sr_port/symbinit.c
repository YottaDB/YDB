/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include <rtnhdr.h>
#include "mv_stent.h"		/* this includes lv_val.h which also includes hashtab_mname.h and hashtab.h */
#include "stack_frame.h"
#include "mdq.h"

#define MVST_STAB_SIZE (SIZEOF(*mv_chain) - SIZEOF(mv_chain->mv_st_cont) + SIZEOF(mv_chain->mv_st_cont.mvs_stab))

GBLREF symval		*curr_symval;

GBLREF mv_stent		*mv_chain;
GBLREF unsigned char	*stackbase, *stacktop, *msp, *stackwarn;
GBLREF stack_frame	*frame_pointer;

error_def(ERR_STACKOFLOW);
error_def(ERR_STACKCRIT);

int4 symbinit(void)
{
	unsigned char	*msp_save;
	mv_stent	*mv_st_ent, *mvst_tmp, *mvst_prev;
	stack_frame	*fp,*fp_prev,*fp_fix;
	symval		*ptr;
	int4		shift_size, ls_size, temp_size;
        int		size;
	unsigned char	*old_sp, *top, *l_syms;

	if (frame_pointer->type & SFT_COUNT)
	{
		temp_size = frame_pointer->rvector->temp_size;
		size = frame_pointer->vartab_len;
		ls_size = size * SIZEOF(ht_ent_mname *);
		if (frame_pointer->l_symtab != (ht_ent_mname **)((char *)frame_pointer - temp_size - ls_size))
		{
			msp_save = msp;
			msp -= ls_size;
		   	if (msp <= stackwarn)
		   	{
				if (msp <= stacktop)
		   		{
					msp = msp_save;
					rts_error(VARLSTCNT(1) ERR_STACKOFLOW);
		   		} else
		   			rts_error(VARLSTCNT(1) ERR_STACKCRIT);
		   	}
			frame_pointer->l_symtab = (ht_ent_mname **)msp;
		}
		PUSH_MV_STENT(MVST_STAB);
		mv_st_ent = mv_chain;
		l_syms =  (unsigned char *)frame_pointer->l_symtab;
		shift_size = 0;
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
		temp_size = fp_prev->rvector->temp_size;
		size = fp_prev->vartab_len;
		ls_size = size * SIZEOF(ht_ent_mname *);
		shift_size = MVST_STAB_SIZE;
		if (fp_prev->l_symtab != (ht_ent_mname **)((char *)fp_prev - ls_size - temp_size))
			shift_size += ls_size;
		msp -= shift_size;
	   	if (msp <= stackwarn)
	   	{
			if (msp <= stacktop)
	   		{
				msp = old_sp;
				rts_error(VARLSTCNT(1) ERR_STACKOFLOW);
	   		} else
	   			rts_error(VARLSTCNT(1) ERR_STACKCRIT);
	   	}
		memmove(msp, old_sp, top - (unsigned char *)old_sp);	/* Shift stack w/possible overlapping range */
		if (shift_size > MVST_STAB_SIZE)
			fp_prev->l_symtab = (ht_ent_mname **)(top - shift_size);
		l_syms = (unsigned char *)fp_prev->l_symtab;
		mv_st_ent = (mv_stent *)(top - MVST_STAB_SIZE);
		mv_st_ent->mv_st_type = MVST_STAB;
		ADJUST_FRAME_POINTER(frame_pointer, shift_size);
		for (fp_fix = frame_pointer; fp_fix != fp_prev ;fp_fix = fp_fix->old_frame_pointer)
		{
			if ((unsigned char *)fp_fix->l_symtab < top && (unsigned char *)fp_fix->l_symtab > stacktop)
			{
				fp_fix->l_symtab = (ht_ent_mname **)((char *)fp_fix->l_symtab - shift_size);
				if ((unsigned char *)fp_fix->l_symtab < (unsigned char *)fp_fix)
					memset((unsigned char *)fp_fix->l_symtab, 0, fp_fix->vartab_len * SIZEOF(ht_ent_mname *));
			}
			if (fp_fix->temps_ptr < top && fp_fix->temps_ptr > stacktop)
				fp_fix->temps_ptr -= shift_size;
			if (fp_fix->vartab_ptr < (char *)top && fp_fix->vartab_ptr > (char *)stacktop)
				fp_fix->vartab_ptr -= shift_size;
			if ((unsigned char *)fp_fix->old_frame_pointer < top && (unsigned char *)fp_fix->old_frame_pointer
				> stacktop)
			{
				ADJUST_FRAME_POINTER(fp_fix->old_frame_pointer, shift_size);
			}
		}
		if ((unsigned char *)mv_chain >= top)
		{
			mv_st_ent->mv_st_next = (uint4)((char *)mv_chain - (char *)mv_st_ent);
			mv_chain = mv_st_ent;
		} else
		{
			fp = (stack_frame *)((char *)fp - shift_size);
			mv_chain = (mv_stent *)((char *)mv_chain - shift_size);
			mvst_tmp = mv_chain;
			mvst_prev = (mv_stent *)((char *)mvst_tmp + mvst_tmp->mv_st_next);
			while (mvst_prev < (mv_stent *)fp)
			{
				mvst_tmp = mvst_prev;
				mvst_prev = (mv_stent *)((char *)mvst_tmp + mvst_tmp->mv_st_next);
			}
			mvst_tmp->mv_st_next = (uint4)((char *)mv_st_ent - (char *)mvst_tmp);
			mv_st_ent->mv_st_next = (uint4)((char *)mvst_prev - (char *)mv_st_ent + shift_size);
		}
	}
	mv_st_ent->mv_st_cont.mvs_stab = (symval *)NULL;	/* special case this so failed initialization can be detected */

	memset(l_syms, 0, ls_size);
	size++;
	ptr = (symval *)malloc(SIZEOF(symval));
	/* the order of initialization of fields mirrors the layout of the fields in the symval structure definition */
	ptr->ident = MV_SYM;
	ptr->sbs_depth = 0;
	ptr->tp_save_all = 0;
	ptr->xnew_var_list = NULL;
	ptr->xnew_ref_list = NULL;
	init_hashtab_mname(&ptr->h_symtab, size, HASHTAB_NO_COMPACT, HASHTAB_NO_SPARE_TABLE);
	ptr->lv_first_block = NULL;
	lv_newblock(ptr, size);
	ptr->lvtree_first_block = NULL;
	ptr->lvtreenode_first_block = NULL;
	ptr->lv_flist = NULL;
	ptr->lvtree_flist = NULL;
	ptr->lvtreenode_flist = NULL;
	ptr->last_tab = curr_symval;
	/* if we get here, our initialization must have been successful */
	if (curr_symval)
		ptr->symvlvl = curr_symval->symvlvl + 1;
	else
		ptr->symvlvl = 1;
	GTMTRIG_ONLY(ptr->trigr_symval = FALSE);
	ptr->alias_activity = FALSE;
	curr_symval = ptr;
	mv_st_ent->mv_st_cont.mvs_stab = ptr;
	return shift_size;
}
