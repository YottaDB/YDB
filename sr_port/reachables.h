/****************************************************************
 *								*
 * Copyright (c) 2026 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef REACHABLES_H_INCLUDED
#define REACHABLES_H_INCLUDED
#include "gtm_common_defs.h"
#include "mdq.h"

#define FOR_EACH_REACHABLE_CTIME(MSTR_ACTION, MVAL_ACTION)									\
MBSTART {															\
	GBLREF mliteral literal_chain;												\
	if (NULL != literal_chain.que.fl)											\
	{	/* If hashtable exists, pull it all from there rather that searching for it twice */				\
		if (complits_hashtab && complits_hashtab->base)									\
		{														\
			for (tabent_lit = complits_hashtab->base, topent_lit = complits_hashtab->top;				\
					tabent_lit < topent_lit; tabent_lit++)							\
			{													\
				if (HTENT_VALID_STR(tabent_lit, mliteral, mlit))						\
				{	/* Key first, then value */								\
					MSTR_ACTION(&(tabent_lit->key.str));							\
					MVAL_ACTION(&(mlit->v));								\
				}												\
			}													\
		} else														\
		{	/* No hash table, just the values */									\
			dqloop(&literal_chain, que, mlit)									\
			{													\
				MVAL_ACTION(&(mlit->v));									\
			}													\
		}														\
	}															\
	assert(offsetof(mvar, mvname) == offsetof(mlabel, mvname));								\
	/* mvartab and mlabtab not included and must be handled manually since they require recursion */			\
	MVAL_ACTION(&(TREF(director_mval)));											\
	MVAL_ACTION(&(TREF(window_mval)));											\
	MVAL_ACTION(&(TREF(indirection_mval)));											\
} MBEND

#define ASSERT_LV_NODE_MSTR_EQUIVALENCE										\
MBSTART {													\
	assert(OFFSETOF(lvTreeNode, key_len) - OFFSETOF(lvTreeNode, key_mvtype) == OFFSETOF(mstr, len));	\
	assert(SIZEOF(((lvTreeNode *)NULL)->key_len) == SIZEOF(((mstr *)NULL)->len));				\
	assert(OFFSETOF(lvTreeNode, key_addr) - OFFSETOF(lvTreeNode, key_mvtype) == OFFSETOF(mstr, addr));	\
	assert(SIZEOF(((lvTreeNode *)NULL)->key_addr) == SIZEOF(((mstr *)NULL)->addr));				\
} MBEND

#define	LV_NODE_KEY_STPG_ADD(NODE, MSTR_ACTION)										\
MBSTART {														\
	/* NODE is of type "lvTreeNode *" or "lvTreeNodeNum *".								\
	 * Only if it is a "lvTreeNode *" (MV_STR bit is set in this case only)						\
	 * should we go ahead with the STPG_ADD.									\
	 */														\
	if (LV_NODE_KEY_IS_STRING(NODE))										\
	{														\
		/* NODE is of type "lvTreeNode *". We need to now pass an mstr pointer to the MSTR_STPG_ADD macro.	\
		 * We cannot create a local mstr since the address of this mstr needs to be different for each		\
		 * NODE passed in to LV_NODE_KEY_STPG_ADD during stp_gcol. Therefore we use an offset inside NODE	\
		 * as the mstr address. Even though NODE is not exactly laid out as an mstr, we ensure the fields	\
		 * that stp_gcol most cares about "mstr->len" and "mstr->addr" are at the exact same offset in		\
		 * both NODE and an "mstr". The equivalent of "mstr->char_len" is not present in NODE but thankfully	\
		 * char_len is not used/touched in stp_gcol and therefore it is ok if something else is present		\
		 * at that offset in the mstr as long as that is exactly 4 bytes long (that happens to be the case	\
		 * in the NODE structure). We assert this below before using an mstr inside NODE.			\
		 * Note: VMS mstr does not have char_len so start accordingly below.					\
		 */													\
		ASSERT_LV_NODE_MSTR_EQUIVALENCE;									\
		/* Note: the typecast below is two-fold (first a "void *" and then a "mstr *"). Not having the "void *"	\
		 * gives compiler warnings in HPUX Itanium since source is 2-byte aligned whereas destination		\
		 * is 8-byte aligned. The void * in between makes the compiler forget the 2-byte alignment.		\
		 */													\
		MSTR_ACTION((mstr *)(void *)&(NODE)->key_mvtype);							\
	} else														\
	{														\
		assert(!glist_lvTreeNode_key_protected(NODE));								\
		DOUBLECHECK_GCOL_ONLY(old_ops++;)									\
	}														\
} MBEND


#define PROCESS_CACHE_ENTRY(cp, MSTR_ACTION, MVAL_ACTION)			\
MBSTART {									\
	MSTR_ACTION(&(cp)->src.str);						\
	/* Run list of mvals for each code stream that exists */		\
	if ((cp)->obj.len)							\
	{									\
		ihdr = (ihdtyp *)(cp)->obj.addr;				\
		fixup_cnt = ihdr->fixup_vals_num;				\
		if (fixup_cnt)							\
		{								\
			m = (mval *)((char *)ihdr + ihdr->fixup_vals_off);	\
			for (mtop = m + fixup_cnt; m < mtop; m++)		\
				MVAL_ACTION(m);					\
		}								\
	}									\
} MBEND

#define LVZWRITE_BLOCK_GC(LVZWRITE_BLOCK, MVAL_ACTION)								\
MBSTART {														\
	if ((NULL != (LVZWRITE_BLOCK)) && ((LVZWRITE_BLOCK)->curr_subsc))					\
	{													\
		assert((LVZWRITE_BLOCK)->sub);									\
		zwr_sub = (zwr_sub_lst *)(LVZWRITE_BLOCK)->sub;							\
		for (index = 0; index < (int)(LVZWRITE_BLOCK)->curr_subsc; index++)				\
		{												\
			assert((zwr_sub->subsc_list[index].actual != zwr_sub->subsc_list[index].first)		\
			    || !zwr_sub->subsc_list[index].second);						\
			/* we cannot garbage collect duplicate mval pointers.					\
			 * So make sure zwr_sub->subsc_list[index].actual is not pointing to an			\
			 * existing (mval *) which  is already protected					\
			 */											\
			if (zwr_sub->subsc_list[index].actual							\
			    && (zwr_sub->subsc_list[index].actual != zwr_sub->subsc_list[index].first))		\
			{											\
				MVAL_ACTION(zwr_sub->subsc_list[index].actual);					\
			}											\
		}												\
	}													\
} MBEND

#define ZWRHTAB_GC(ZWRHTAB, MSTR_ACTION)									\
MBSTART {													\
	if ((ZWRHTAB))												\
	{													\
		for (tabent_addr = (ZWRHTAB)->h_zwrtab.base, topent_addr = (ZWRHTAB)->h_zwrtab.top;		\
		     tabent_addr < topent_addr; tabent_addr++)							\
		{												\
			if (HTENT_VALID_ADDR(tabent_addr, zwr_alias_var, zav))					\
			{											\
				x = &zav->zwr_var;								\
				/* Protect MSTRs we are using to process the zwrite */				\
				assert(x->len); /* if it is valid it should have a non-zero length */		\
				MSTR_ACTION(x);									\
			}											\
		}												\
	}													\
} MBEND


#define FOR_EACH_REACHABLE_RTIME(MSTR_ACTION, MVAL_ACTION, MSTR_FAST_ACTION)						\
MBSTART {														\
	for (tabent_objcode = cache_table.base, topent = cache_table.top; tabent_objcode < topent; tabent_objcode++)	\
	{														\
		if (HTENT_VALID_OBJCODE(tabent_objcode, cache_entry, cp))						\
		{													\
			MSTR_ACTION(&(tabent_objcode->key.str));							\
			PROCESS_CACHE_ENTRY(cp, MSTR_ACTION, MVAL_ACTION);						\
		}													\
	}														\
	for (symtab = curr_symval; NULL != symtab; symtab = symtab->last_tab)						\
	{														\
		for (tabent_mname = symtab->h_symtab.base, topent_mname = symtab->h_symtab.top;				\
		     tabent_mname < topent_mname; tabent_mname++)							\
		{													\
			if (HTENT_VALID_MNAME(tabent_mname, lv_val, lvp))						\
			{												\
				MSTR_ACTION(&(tabent_mname->key.var_name));						\
			}												\
		}													\
		for (xnewvar = symtab->xnew_var_list; xnewvar; xnewvar = xnewvar->next)					\
			MSTR_ACTION(&xnewvar->key.var_name);								\
	}														\
	if ((x = comline_base))												\
	{														\
		for (index = MAX_RECALL; index > 0 && x->len; index--, x++)						\
		{	/* These strings are guaranteed to be in the stringpool so use MSTR_STPG_PUT macro directly	\
			 * instead of going through MSTR_ACTION. But assert accordingly to be sure.			\
			 */												\
			assert(IS_PTR_IN_RANGE(x->addr, stringpool.base, stringpool.free));				\
			MSTR_FAST_ACTION(x);										\
		}													\
	}														\
	for (lvzwrblk = lvzwrite_block; NULL != lvzwrblk; lvzwrblk = lvzwrblk->prev)					\
	{														\
		LVZWRITE_BLOCK_GC(lvzwrblk, MVAL_ACTION);								\
	}														\
	ZWRHTAB_GC(zwrhtab, MSTR_ACTION);										\
	for (l = io_root_log_name; NULL != l; l = l->next)								\
	{														\
		if ((NULL == l->iod) || (n_io_dev_types == l->iod->type))						\
		{													\
			assert(FALSE);											\
			continue;       /* skip it on pro */								\
		}													\
		if ((IO_ESC != l->dollar_io[0]) && (l->iod->trans_name == l))						\
		{													\
			MSTR_ACTION(&l->iod->error_handler);								\
			/* If this is a split $principal, protect error_handler defined on the output side */		\
			if ((l->iod->pair.in != l->iod->pair.out)							\
			    && (l->iod->pair.out == io_std_device->out))						\
				MSTR_ACTION(&l->iod->pair.out->error_handler);						\
			else												\
				assert((&l->iod->pair.out->error_handler == &l->iod->error_handler) ||			\
						!glist_str_protected(&l->iod->pair.out->error_handler));		\
			rm_ptr = (rm == l->iod->type) ? (d_rm_struct *)l->iod->dev_sp : NULL;				\
			if (NULL != rm_ptr)										\
			{	/* Protect the IVs and KEYs as needed. */						\
				if (rm_ptr->input_encrypted)								\
				{											\
					MSTR_ACTION(&rm_ptr->input_iv);						\
					MSTR_ACTION(&rm_ptr->input_key);						\
				} else											\
				{											\
															\
					assert(!glist_str_protected(&rm_ptr->input_iv)					\
							|| !rm_ptr->input_iv.len || !rm_ptr->input_iv.addr);		\
					assert(!glist_str_protected(&rm_ptr->input_key) || !rm_ptr->input_key.len	\
							|| !rm_ptr->input_key.addr);					\
				}											\
				if (rm_ptr->output_encrypted)								\
				{											\
					MSTR_ACTION(&rm_ptr->output_iv);						\
					MSTR_ACTION(&rm_ptr->output_key);						\
				} else											\
				{											\
					assert(!glist_str_protected(&rm_ptr->output_iv) || !rm_ptr->output_iv.len	\
							|| !rm_ptr->output_iv.addr);					\
					assert(!glist_str_protected(&rm_ptr->output_key) || !rm_ptr->output_key.len	\
							|| !rm_ptr->output_key.addr);					\
				}											\
			}												\
			if ((l->iod->pair.in != l->iod->pair.out)							\
			    && (l->iod->pair.out == io_std_device->out))						\
			{												\
				rm_ptr = (rm == l->iod->pair.out->type) ? (d_rm_struct *)l->iod->pair.out->dev_sp : NULL;\
				if (NULL != rm_ptr)									\
				{	/* Protect the IVs and KEYs as needed. */					\
					if (rm_ptr->input_encrypted)							\
					{										\
						MSTR_ACTION(&rm_ptr->input_iv);						\
						MSTR_ACTION(&rm_ptr->input_key);					\
					} else										\
					{										\
						assert(!glist_str_protected(&rm_ptr->input_iv)				\
							|| !rm_ptr->input_iv.len || !rm_ptr->input_iv.addr);		\
						assert(!glist_str_protected(&rm_ptr->input_key)				\
							|| !rm_ptr->input_key.len || !rm_ptr->input_key.addr);		\
					}										\
					if (rm_ptr->output_encrypted)							\
					{										\
						MSTR_ACTION(&rm_ptr->output_iv);					\
						MSTR_ACTION(&rm_ptr->output_key);					\
					} else										\
					{										\
						assert(!glist_str_protected(&rm_ptr->output_iv)				\
							|| !rm_ptr->output_iv.len || !rm_ptr->output_iv.addr);		\
						assert(!glist_str_protected(&rm_ptr->output_key)			\
							|| !rm_ptr->output_key.len || !rm_ptr->output_key.addr);	\
					}										\
				}											\
			}												\
		}													\
	}														\
	MVAL_ACTION(&(TREF(dollar_etrap)));										\
	MVAL_ACTION(&dollar_system);											\
	MVAL_ACTION(&dollar_zsource);											\
	MVAL_ACTION(&(TREF(dollar_ztrap)));										\
	MVAL_ACTION(&dollar_zstatus);											\
	MVAL_ACTION(&dollar_zgbldir);											\
	MVAL_ACTION(&dollar_zinterrupt);										\
	MVAL_ACTION(&(TREF(dollar_zstep)));										\
	MVAL_ACTION(&(TREF(zstep_action)));										\
	MVAL_ACTION(&dollar_zerror);											\
	MVAL_ACTION(&dollar_ztexit);											\
	MVAL_ACTION(&dollar_zyerror);											\
	GTMTRIG_ONLY(MVAL_ACTION(&dollar_ztwormhole);)									\
	GTMTRIG_ONLY(MVAL_ACTION(&dollar_ztslate);)									\
	MVAL_ACTION(&(TREF(last_fnquery_return_varname)));								\
	MVAL_ACTION(&(TREF(trestart_xpel_rtnlab)));									\
	for (index = 0; index < TREF(last_fnquery_return_subcnt); index++)						\
		MVAL_ACTION(&TAREF1(last_fnquery_return_sub, index));							\
	for (mvs = mv_chain; mvs < (mv_stent *)stackbase; mvs = (mv_stent *)((char *)mvs + mvs->mv_st_next))		\
	{														\
		switch (mvs->mv_st_type)										\
		{													\
			case MVST_MVAL:											\
				m = &mvs->mv_st_cont.mvs_mval;								\
				break;											\
			case MVST_MSAV:											\
				m = &mvs->mv_st_cont.mvs_msav.v;							\
				break;											\
			case MVST_STAB:											\
				if (NULL != (symtab = mvs->mv_st_cont.mvs_stab))					\
				{	/* if initalization of the table was successful */				\
					for (lv_blk_ptr = symtab->lv_first_block; NULL != lv_blk_ptr;			\
					     lv_blk_ptr = lv_blk_ptr->next)						\
					{										\
						for (lvp = (lv_val *)LV_BLK_GET_BASE(lv_blk_ptr),			\
						    lvlimit = LV_BLK_GET_FREE(lv_blk_ptr, lvp);				\
						    lvp < lvlimit; lvp++)						\
						{									\
							/* lvp could be actively in use or free (added to the symval's	\
							 * lv_flist). Ignore the free ones. Those should have their	\
							 * "parent" (aka "symval" since this is a lv_val) set to NULL.	\
							 */								\
							if (NULL == LV_PARENT(lvp))					\
							{								\
								/* One cost savings over the original algorithm is that	\
								 * these are not even checked				\
								 * in the new one. Assert that here.			\
								 */							\
								assert(!lvp->v.str.in_array);				\
								continue;						\
							}								\
							assert(LV_IS_BASE_VAR(lvp));					\
							/* For a base variable, we need to protect only the node value	\
							 * (if it exists and is of type string).			\
							 */								\
							MVAL_ACTION(&(lvp->v));					\
						}									\
					}										\
					for (lv_blk_ptr = symtab->lvtreenode_first_block; NULL != lv_blk_ptr;		\
					     lv_blk_ptr = lv_blk_ptr->next)						\
					{										\
						for (node = (lvTreeNode *)LV_BLK_GET_BASE(lv_blk_ptr),			\
							     node_limit = LV_BLK_GET_FREE(lv_blk_ptr, node);		\
						     node < node_limit; node++)						\
						{									\
							/* node could be actively in use or free (added to the symval's	\
							 * lvtreenode_flist). Ignore the free ones. Those should have	\
							 * their "parent"  set to NULL.					\
							 */								\
															\
							if (NULL == LV_PARENT(node))					\
							{								\
								assert(!node->v.str.in_array);				\
								assert(!glist_lvTreeNode_key_protected(node));		\
								continue;						\
							}								\
							assert(!LV_IS_BASE_VAR(node));					\
							/* For a subscripted variable, we need to protect the subscript	\
							 * (if string) as well as the subscripted node value (if exists	\
							 * and is of type string).					\
							 */								\
							/* Protect node subscript */					\
							LV_NODE_KEY_STPG_ADD(node, MSTR_ACTION);			\
							MVAL_ACTION(&(node->v));	/* Protect node value */	\
						}									\
					}										\
				}											\
				continue;	/* continue loop, not after switch stmt */				\
			case MVST_LVAL:											\
			case MVST_NTAB:											\
			case MVST_TVAL:											\
			case MVST_STCK:											\
			case MVST_STCK_SP:										\
			case MVST_PVAL:											\
			case MVST_STORIG:										\
			case MVST_ZINTCMD:										\
			case MVST_L_SYMTAB:										\
				continue;										\
			case MVST_TPHOLD:										\
				GTMTRIG_ONLY(										\
				if (0 == mvs->mv_st_cont.mvs_tp_holder.tphold_tlevel)					\
				{	/* $ZTWORMHOLE only saved at initial TP level */				\
					MVAL_ACTION(&mvs->mv_st_cont.mvs_tp_holder.ztwormhole_save);			\
				})											\
				continue;										\
			case MVST_NVAL:											\
				/* The var_name field is only present in a debug build */				\
				DEBUG_ONLY(MSTR_ACTION(&mvs->mv_st_cont.mvs_nval.name.var_name));			\
				continue;										\
			GTMTRIG_ONLY(case MVST_TRIGR:									\
				/* Top elements of MVST_TRIGR and MVST_ZINTR structures are the same. These common	\
				 * elements are processed when we fall through to the MVST_ZINTR entry below. Here	\
				 * we process the unique MVST_TRIGR elements.						\
				 */											\
				MVAL_ACTION(&mvs->mv_st_cont.mvs_trigr.dollar_etrap_save);				\
				MVAL_ACTION(&mvs->mv_st_cont.mvs_trigr.dollar_ztrap_save);				\
				/* Note fall into MVST_ZINTR to process common entries */				\
				)											\
			case MVST_ZINTR: /* Or case MVST_ZTIMEOUT: */							\
				MSTR_ACTION(&mvs->mv_st_cont.mvs_zintr.savextref);					\
				m = &mvs->mv_st_cont.mvs_zintr.savtarg;							\
				break;											\
			case MVST_ZINTDEV:										\
				if (mvs->mv_st_cont.mvs_zintdev.buffer_valid)						\
					MSTR_ACTION(&mvs->mv_st_cont.mvs_zintdev.curr_sp_buffer);			\
				continue;										\
			case MVST_MRGZWRSV:										\
				LVZWRITE_BLOCK_GC(mvs->mv_st_cont.mvs_mrgzwrsv.save_lvzwrite_block, MVAL_ACTION);	\
				ZWRHTAB_GC(mvs->mv_st_cont.mvs_mrgzwrsv.save_zwrhtab, MSTR_ACTION);			\
				continue;										\
			default:											\
				assertpro(FALSE && mvs->mv_st_type);							\
		}													\
		MVAL_ACTION(m);												\
	}														\
	/* If LINK:RECURSIVE is enabled and the first M frame on the stack is recursively replaced, when it unwinds,	\
	 * op_unwind() is going to unlink that version of the routine. We will eventually come here (as stp_move)	\
	 * with a zeroed frame pointer so the test below needs to deal with a NULL frame_pointer value.			\
	 */														\
	if (NULL != frame_pointer)											\
	{														\
		for (sf = frame_pointer, old_sf = sf->old_frame_pointer;						\
		     sf && (sf < (stack_frame *)stackbase); sf = old_sf)						\
		{	/* Cover temp mvals in use */									\
			old_sf = sf->old_frame_pointer;									\
			if (NULL == old_sf)										\
			{	/* If trigger enabled, may need to jump over a base frame */				\
				GTMTRIG_ONLY(if (SFT_TRIGR & sf->type)							\
				{	/* We have a trigger base frame, back up over it */				\
					old_sf = *(stack_frame **)(sf + 1);						\
					assert(old_sf);									\
					assert(old_sf->old_frame_pointer);						\
				} else if (SFT_BASE & sf->type))							\
				NON_GTMTRIG_ONLY(if (SFT_BASE & sf->type))						\
				{											\
					old_sf = *(stack_frame **)(sf + 1);						\
					if ((old_sf >= (stack_frame *)stackbase) || (old_sf < (stack_frame *)stacktop))	\
						old_sf = NULL;								\
				}											\
			}												\
			assert(sf->temps_ptr);										\
			if (sf->temps_ptr >= (unsigned char *)sf)							\
				continue;										\
			m = (mval *)sf->temps_ptr;									\
			for (mtop = m + sf->temp_mvals; m < mtop; m++)							\
			{	/* DM frames should not normally have temps. If they do then it better have mvtype 0	\
				 * thereby guaranteeing it will not need stp_gcol protection by the MVAL_ACTION macro	\
				 * below.										\
				 */											\
				assert(!(sf->type & SFT_DM) || !MV_DEFINED(m));						\
				MVAL_ACTION(m);									\
			}												\
		}													\
	}														\
	if (NULL != TREF(glvn_pool_ptr))										\
	{	/* Protect everything in the glvn pool. */								\
		m = (TREF(glvn_pool_ptr))->mval_stack;									\
		for (mtop = m + (TREF(glvn_pool_ptr))->mval_top; m < mtop; m++)						\
			MVAL_ACTION(m);										\
	}														\
	if (NULL != alias_retarg)											\
	{	/* An alias return value is in-flight - process it */							\
		assert(alias_retarg->mvtype & MV_ALIASCONT);								\
		if (alias_retarg->mvtype & MV_ALIASCONT)								\
		{	/* Protect the refs were are about to make in case ptr got banged up somehow */			\
			lvp = (lv_val *)alias_retarg;									\
			assert(LV_IS_BASE_VAR(lvp));									\
			MVAL_ACTION(&lvp->v);										\
		}													\
	}														\
	if (tp_pointer)													\
	{														\
		tf = tp_pointer;											\
		while (tf)												\
		{													\
			MVAL_ACTION(&tf->trans_id);									\
			MVAL_ACTION(&tf->zgbldir);									\
			for (restore_ent = tf->vars; restore_ent; restore_ent = restore_ent->next)			\
				MSTR_ACTION(&(restore_ent->key.var_name));						\
			tf = tf->old_tp_frame;										\
		}													\
	}														\
} MBEND

#endif
