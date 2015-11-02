/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "hashtab_mname.h"	/* needed for lv_val.h */
#include "lv_val.h"
#include "rtnhdr.h"
#include "mv_stent.h"
#include "stack_frame.h"
#include "mdq.h"

#define MVST_STAB_SIZE (SIZEOF(*mv_chain) - SIZEOF(mv_chain->mv_st_cont) + SIZEOF(mv_chain->mv_st_cont.mvs_stab))

GBLREF symval		*curr_symval;

GBLREF mv_stent		*mv_chain;
GBLREF unsigned char	*stackbase, *stacktop, *msp, *stackwarn;
GBLREF stack_frame	*frame_pointer;

int4 symbinit(void)
{
	unsigned char	*msp_save;
	mv_stent	*mv_st_ent, *mvst_tmp, *mvst_prev;
	stack_frame	*fp,*fp_prev,*fp_fix;
	symval		*ptr;
	int4		shift, ls_size, temp_size;
        int		size;
	unsigned char	*old_sp, *top, *l_syms;
	error_def(ERR_STACKOFLOW);
	error_def(ERR_STACKCRIT);

	if (frame_pointer->type & SFT_COUNT)
	{
		temp_size = frame_pointer->rvector->temp_size;
		size = frame_pointer->vartab_len;
		ls_size = size * SIZEOF(mval *);
		if (frame_pointer->l_symtab != (mval **)((char *) frame_pointer - temp_size - ls_size))
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
			frame_pointer->l_symtab = (mval **)msp;
		}
		PUSH_MV_STENT(MVST_STAB);
		mv_st_ent = mv_chain;
		l_syms =  (unsigned char *)frame_pointer->l_symtab;
		shift = 0;
	}
	else
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
		ls_size = size * SIZEOF(mval *);
		shift = MVST_STAB_SIZE;
		if (fp_prev->l_symtab != (mval **)((char *) fp_prev - ls_size - temp_size))
			shift += ls_size;
		msp -= shift;
	   	if (msp <= stackwarn)
	   	{
			if (msp <= stacktop)
	   		{
				msp = old_sp;
				rts_error(VARLSTCNT(1) ERR_STACKOFLOW);
	   		} else
	   			rts_error(VARLSTCNT(1) ERR_STACKCRIT);
	   	}
		memcpy(msp, old_sp, top - (unsigned char *) old_sp);
		if (shift > MVST_STAB_SIZE)
			fp_prev->l_symtab = (mval **)(top - shift);
		l_syms = (unsigned char *)fp_prev->l_symtab;
		mv_st_ent = (mv_stent *)(top - MVST_STAB_SIZE);
		mv_st_ent->mv_st_type = MVST_STAB;
		frame_pointer = (stack_frame *)((char *) frame_pointer - shift);
		for (fp_fix = frame_pointer; fp_fix != fp_prev ;fp_fix = fp_fix->old_frame_pointer)
		{
			if ((unsigned char *) fp_fix->l_symtab < top && (unsigned char *) fp_fix->l_symtab > stacktop)
			{
				fp_fix->l_symtab = (mval **)((char *) fp_fix->l_symtab - shift);
				if ((unsigned char *) fp_fix->l_symtab < (unsigned char *) fp_fix)
					memset((unsigned char *) fp_fix->l_symtab, 0, fp_fix->vartab_len * sizeof(mval *));
			}
			if (fp_fix->temps_ptr < top && fp_fix->temps_ptr > stacktop)
				fp_fix->temps_ptr -= shift;
			if (fp_fix->vartab_ptr < (char *) top && fp_fix->vartab_ptr > (char *) stacktop)
				fp_fix->vartab_ptr -= shift;
			if ((unsigned char *) fp_fix->old_frame_pointer < top && (unsigned char *) fp_fix->old_frame_pointer
				> stacktop)
				fp_fix->old_frame_pointer = (stack_frame *)((char *) fp_fix->old_frame_pointer - shift);
		}
		if ((unsigned char *) mv_chain >= top)
		{
			mv_st_ent->mv_st_next = (uint4)((char *) mv_chain - (char *) mv_st_ent);

			mv_chain = mv_st_ent;
		}
		else
		{
			fp = (stack_frame *)((char *) fp - shift);
			mv_chain = (mv_stent *)((char *) mv_chain - shift);
			mvst_tmp = mv_chain;
			mvst_prev = (mv_stent *)((char *) mvst_tmp + mvst_tmp->mv_st_next);
			while (mvst_prev < (mv_stent *)fp)
			{
				mvst_tmp = mvst_prev;
				mvst_prev = (mv_stent *)((char *) mvst_tmp + mvst_tmp->mv_st_next);
			}
			mvst_tmp->mv_st_next = (uint4)((char *) mv_st_ent - (char *) mvst_tmp);
			mv_st_ent->mv_st_next = (uint4)((char *) mvst_prev - (char *) mv_st_ent + shift);
		}
	}
	mv_st_ent->mv_st_cont.mvs_stab = (symval *)NULL;	/* special case this so failed initialization can be detected */

	memset(l_syms, 0, ls_size);
	size++;
	ptr = (symval *)malloc(sizeof(symval));
	init_hashtab_mname(&ptr->h_symtab, size);
	ptr->last_tab = curr_symval;
	ptr->lv_flist = 0;
	ptr->tp_save_all = 0;

	/* dqinit (ptr, sbs_que); */
	ptr->sbs_que.bl = (struct sbs_blk_struct *)ptr;
	ptr->sbs_que.fl = ptr->sbs_que.bl;

	ptr->ident = MV_SYM;
	lv_newblock(&ptr->first_block, 0, size);
	/* if we get here, our initialization must have been successful */
	curr_symval = ptr;
	mv_st_ent->mv_st_cont.mvs_stab = ptr;
	return shift;
}
