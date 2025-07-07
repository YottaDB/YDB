/****************************************************************
 *								*
 * Copyright (c) 2001-2025 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdio.h"

#include "lv_val.h"
#include "gtmio.h"
#include <rtnhdr.h>
#include "mv_stent.h"		/* this includes lv_val.h which also includes hashtab_mname.h and hashtab.h */
#include "stack_frame.h"
#include "mdq.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"

#define MVST_STAB_SIZE (SIZEOF(*mv_chain) - SIZEOF(mv_chain->mv_st_cont) + SIZEOF(mv_chain->mv_st_cont.mvs_stab))

GBLREF symval		*curr_symval;
GBLREF mv_stent		*mv_chain;
GBLREF unsigned char	*stackbase, *stacktop, *msp, *stackwarn;
GBLREF stack_frame	*frame_pointer;

error_def(ERR_STACKOFLOW);
error_def(ERR_STACKCRIT);

boolean_t symbinit(void)
{
	long		ls_size;
	int		size;
	int4		shift_size, temp_size;
	mv_stent	*mv_st_ent, *mvst_tmp, *mvst_prev, *mv_st_stab;
	stack_frame	*fp, *fp_prev = NULL, *fp_fix;
	symval		*ptr;
	ht_ent_mname	**new_lsym_addr = NULL, **old_lsym_addr = NULL;
	unsigned char	*l_syms, *msp_save, *old_sp, *top;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;

	/* An MVST_L_SYMTAB generally contains references to entries in an MVST_STAB, so it is safer to pop the L_SYMTAB
	 * before popping the STAB. Push them in the reverse order to guarantee that.
	 */
	PUSH_MV_STENT(MVST_STAB);
	mv_st_stab = mv_chain;
	/* Special case to detect initialization failure. It would be better not to malloc at all and instead to reserve
	 * stack space - in principle it is possible since the necessary space would be no larger than an ordinary l_symtab,
	 * many of which are already on the stack without a problem. The only concern is the limited mvs_size array, which
	 * limits maximum mv_stent sizes to CHAR_MAX. If this is ever expanded to a uint array, the malloc here should be
	 * re-examined.
	 */
	mv_st_stab->mv_st_cont.mvs_stab = (symval *)NULL;
	/* Find the most recent counted frame */
	for (fp = frame_pointer; !(fp->type & SFT_COUNT); fp_prev = fp, fp = fp->old_frame_pointer)
		;
	/* Check if it needs an l_symtab independent of its parent */
	temp_size = fp->rvector->temp_size;
	size = fp->vartab_len;
	ls_size = size * SIZEOF(ht_ent_mname *);
	if (fp->old_frame_pointer && (fp->l_symtab == fp->old_frame_pointer->l_symtab))
	{
		assert(fp->l_symtab != (ht_ent_mname **)((char *)fp - temp_size - ls_size));
		/* Assert that this is the only frame we need to treat like this */
		assert(!fp_prev || fp_prev->l_symtab != fp->l_symtab);
		old_lsym_addr = fp->l_symtab;
		PUSH_MV_STENT(MVST_L_SYMTAB);
		mv_st_ent = mv_chain;
		mv_st_ent->mv_st_cont.mvs_l_symtab.size = ls_size;
		/* handle OOM condition similar to MVST_STAB with originally NULL ptr */
		mv_st_ent->mv_st_cont.mvs_l_symtab.l_symtab = NULL;
#		ifdef DEBUG
		mv_st_ent->mv_st_cont.mvs_l_symtab.old_l_symtab = old_lsym_addr;
#		endif
		new_lsym_addr = malloc(ls_size);
		mv_st_ent->mv_st_cont.mvs_l_symtab.l_symtab = new_lsym_addr;
		fp->l_symtab = new_lsym_addr;
	}
	for (fp_fix = frame_pointer; fp_fix != fp; fp_fix = fp_fix->old_frame_pointer)
	{
		assert(!(fp_fix->type & SFT_COUNT));
		memset(fp_fix->l_symtab, 0, fp_fix->vartab_len * SIZEOF(ht_ent_mname *));
	}
	assert(ls_size == fp->vartab_len * SIZEOF(ht_ent_mname *));
	memset(fp->l_symtab, 0, ls_size);
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
	DBGRFCT((stderr,"symbinit: Allocated new symbol table at 0x"lvaddr" pushing old symbol table on M stack (0x"lvaddr")\n",
		 ptr, curr_symval));
	curr_symval = ptr;
	(TREF(curr_symval_cycle))++;				/* curr_symval is changing - update cycle */
	mv_st_stab->mv_st_cont.mvs_stab = ptr;
	return fp != frame_pointer;
}
