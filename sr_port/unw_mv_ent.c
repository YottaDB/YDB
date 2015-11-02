/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef VMS
#include <fab.h>		/* needed for dbgbldir_sysops.h */
#endif

#include "gtm_string.h"
#include "hashtab_mname.h"	/* needed for lv_val.h */
#include "lv_val.h"
#include "rtnhdr.h"
#include "mv_stent.h"
#include "sbs_blk.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "dpgbldir.h"
#include "dpgbldir_sysops.h"
#include "error_trap.h"		/* for STACK_ZTRAP_EXPLICIT_NULL macro */
#include "op.h"
#include "iotcpdef.h"
#include "gt_timer.h"
#include "iosocketdef.h"
#ifdef UNIX
#include "iottdef.h"
#endif
#include "stack_frame.h"
#include "alias.h"

GBLREF symval			*curr_symval;
GBLREF sbs_blk			*sbs_blk_hdr;
GBLREF boolean_t		dollar_truth;
GBLREF lv_val			*active_lv;
GBLREF gv_key			*gv_currkey;
GBLREF gv_namehead		*gv_target;
GBLREF gd_addr			*gd_header;
GBLREF gd_addr			*gd_targ_addr;
GBLREF gd_binding		*gd_map;
GBLREF gd_binding		*gd_map_top;
GBLREF mval			dollar_etrap;
GBLREF mval			dollar_ztrap;
GBLREF mval			dollar_zgbldir;
GBLREF volatile boolean_t	dollar_zininterrupt;
GBLREF boolean_t		ztrap_explicit_null;		/* whether $ZTRAP was explicitly set to NULL in this frame */
GBLREF mstr			extnam_str;
GBLREF stack_frame		*frame_pointer;
GBLREF lv_xnew_var		*xnewvar_anchor;

mval	*unw_mv_ent(mv_stent *mv_st_ent)
{
	lv_blk		*lp, *lpnext;
	lv_val		*lvval_ptr;
	symval		*symval_ptr;
	mval		*ret_value;
	ht_ent_mname	*hte;
	lv_xnew_var	*xnewvar, *xnewvarnext;
	d_socket_struct	*dsocketptr;
	socket_interrupt *sockintr;
	UNIX_ONLY(d_tt_struct	*tt_ptr;)
	DBGRFCT_ONLY(mident_fixed vname;)

	active_lv = (lv_val *)NULL; /* if we get here, subscript set was successful, clear active_lv to avoid cleanup problems */
	switch (mv_st_ent->mv_st_type)
	{
		case MVST_MSAV:
			*mv_st_ent->mv_st_cont.mvs_msav.addr = mv_st_ent->mv_st_cont.mvs_msav.v;
			if (mv_st_ent->mv_st_cont.mvs_msav.addr == &dollar_etrap)
			{
				if (dollar_etrap.str.len)
					ztrap_explicit_null = FALSE;
				dollar_ztrap.str.len = 0;
			} else if (mv_st_ent->mv_st_cont.mvs_msav.addr == &dollar_ztrap)
			{
				if (STACK_ZTRAP_EXPLICIT_NULL == dollar_ztrap.str.len)
				{
					dollar_ztrap.str.len = 0;
					ztrap_explicit_null = TRUE;
				} else
					ztrap_explicit_null = FALSE;
				dollar_etrap.str.len = 0;
			} else if (mv_st_ent->mv_st_cont.mvs_msav.addr == &dollar_zgbldir)
			{
				if (dollar_zgbldir.str.len != 0)
				{
					gd_header = zgbldir(&dollar_zgbldir);
					/* update the gd_map */
					SET_GD_MAP;
				} else
				{
					dpzgbini();
					gd_header = NULL;
				}
				if (gv_currkey)
					gv_currkey->base[0] = 0;
				if (gv_target)
					gv_target->clue.end = 0;
			}
			return NULL;
		case MVST_MVAL:
		case MVST_IARR:
			return NULL;
		case MVST_LVAL:
			/* Reduce the reference count of this unanchored lv_val (current usage as callin argument
			   holder) and put on free list if no longer in use.
			*/
			lvval_ptr = mv_st_ent->mv_st_cont.mvs_lvval;
			DECR_BASE_REF_NOSYM(lvval_ptr, FALSE);
			return NULL;
		case MVST_STAB:
			if (mv_st_ent->mv_st_cont.mvs_stab)
			{
				assert(mv_st_ent->mv_st_cont.mvs_stab == curr_symval);
				symval_ptr = curr_symval;
				curr_symval = symval_ptr->last_tab;
				DBGRFCT((stderr, "\n\n***** unw_mv_ent-STAB: ** Symtab pop with 0x"lvaddr" replacing 0x"
					 lvaddr"\n\n", curr_symval, symval_ptr));
				if (symval_ptr->alias_activity && symval_ptr->xnew_var_list)
					/* Special cleanup for aliases and passed through vars */
					als_check_xnew_var_aliases(symval_ptr, curr_symval);
				else
				{	/* Drop reference counts & Release any lv_xnew_var blocks we have */
					assert(NULL == symval_ptr->xnew_ref_list);	/* Without aliases, no ref list possible */
					for (xnewvar = symval_ptr->xnew_var_list; xnewvar; xnewvar = xnewvarnext)
					{
						hte = lookup_hashtab_mname(&curr_symval->h_symtab, &xnewvar->key);
						lvval_ptr = (lv_val *)hte->value;
						assert(lvval_ptr);
						DECR_CREFCNT(lvval_ptr);
						assert(1 < lvval_ptr->stats.trefcnt);
						DECR_BASE_REF_NOSYM(lvval_ptr, TRUE);
						xnewvarnext = xnewvar->next;
						xnewvar->next = xnewvar_anchor;
						xnewvar_anchor = xnewvar;
					}
				}
				free(symval_ptr->first_block.lv_base);
				for (lp = symval_ptr->first_block.next ; lp ; lp = lpnext)
				{
					free(lp->lv_base);
					lpnext = lp->next;
					free(lp);
				}
				if (symval_ptr->sbs_que.fl == (sbs_blk *)symval_ptr)
					assert(symval_ptr->sbs_que.fl == symval_ptr->sbs_que.bl);
				else
				{
					symval_ptr->sbs_que.bl->sbs_que.fl = sbs_blk_hdr;
					sbs_blk_hdr = symval_ptr->sbs_que.fl;
				}
				free(symval_ptr->h_symtab.base);
				free(symval_ptr);
				frame_pointer->flags |= SFF_UNW_SYMVAL;
			}
			return NULL;
		case MVST_NTAB:
			DEBUG_ONLY(hte = lookup_hashtab_mname(&curr_symval->h_symtab, mv_st_ent->mv_st_cont.mvs_ntab.nam_addr));
			assert(hte);
			assert(hte == mv_st_ent->mv_st_cont.mvs_ntab.hte_addr);
			hte = mv_st_ent->mv_st_cont.mvs_ntab.hte_addr;
			lvval_ptr = (lv_val *)hte->value;
			DBGRFCT_ONLY(
				memcpy(vname.c, hte->key.var_name.addr, hte->key.var_name.len);
				vname.c[hte->key.var_name.len] = '\0';
			);
			DBGRFCT((stderr, "unw_mv_ent1: var '%s' hte 0x"lvaddr" being reset from 0x"lvaddr" to 0x"lvaddr"\n",
				 &vname.c, hte, hte->value, mv_st_ent->mv_st_cont.mvs_ntab.save_value));
			hte->value = (char *)mv_st_ent->mv_st_cont.mvs_ntab.save_value;
			if (lvval_ptr)
			{
				DECR_BASE_REF_NOSYM(lvval_ptr, FALSE);
			}
			return NULL;
		case MVST_PARM:
			if (mv_st_ent->mv_st_cont.mvs_parm.mvs_parmlist)
				free(mv_st_ent->mv_st_cont.mvs_parm.mvs_parmlist);
			ret_value = mv_st_ent->mv_st_cont.mvs_parm.ret_value;
			if (ret_value)
				dollar_truth = (boolean_t)mv_st_ent->mv_st_cont.mvs_parm.save_truth;
			return ret_value;
		case MVST_PVAL:
			if (mv_st_ent->mv_st_cont.mvs_pval.mvs_ptab.hte_addr)
			{
				assert(mv_st_ent->mv_st_cont.mvs_pval.mvs_ptab.nam_addr);
				DEBUG_ONLY(hte = lookup_hashtab_mname(&curr_symval->h_symtab,
								      mv_st_ent->mv_st_cont.mvs_pval.mvs_ptab.nam_addr));
				assert(hte);
				assert(hte == mv_st_ent->mv_st_cont.mvs_pval.mvs_ptab.hte_addr);
				hte = mv_st_ent->mv_st_cont.mvs_pval.mvs_ptab.hte_addr;
				lvval_ptr = (lv_val *)hte->value;	/* lv_val being popped */
				DBGRFCT_ONLY(
					memcpy(vname.c, hte->key.var_name.addr, hte->key.var_name.len);
					vname.c[hte->key.var_name.len] = '\0';
				);
				DBGRFCT((stderr, "unw_mv_ent2: var '%s' hte 0x"lvaddr" being reset from 0x"lvaddr" to 0x"lvaddr"\n",
					 &vname.c, hte, hte->value, mv_st_ent->mv_st_cont.mvs_pval.mvs_ptab.save_value));
				hte->value = (char *)mv_st_ent->mv_st_cont.mvs_pval.mvs_ptab.save_value;
				/* At this point lvval_ptr has one of two values:
				   1 - It has the same value as in mv_st_ent->mv_st_cont.mvs_pval.mvs_val which is the lv_val
				       allocated for use for the duration of this mv_stent (replaced the lv_val saved in
				       our save_value field which was restored just above). This is the normal case when
				       either this var is not aliased or if it was the source of the alias.
				   2 - Say this was variable "A" and someone did a "SET *A=B". In that case, the hashtable entry
				       would be pointed to "B" and this temporary lv_val would be already returned to the free
				       queue and we no longer need to worry about it. In this case lvval would not be the
				       same address as the temp mval yet the same operations need to be done on it. Note that
				       an lvval_ptr value of NULL is just a special version of this case since the hashtable
				       value would not have changed except for alias processing that freed the original lv_val.

				   So we end up with two possible types of addrs but we handle them exactly the same - namely
				   decrement their reference counts and if they have gone to zero meaning nothing else points to
				   them, then delete them and requeue them.

				   There really isn't an assert we can put here to validate these conditions since in either case
				   our var could have become an alias and been "unaliased" many times over.
				*/
				if (lvval_ptr)
				{
					DECR_BASE_REF_NOSYM(lvval_ptr, FALSE);
				}
				return NULL;
			}
			LV_FLIST_ENQUEUE(&curr_symval->lv_flist, mv_st_ent->mv_st_cont.mvs_pval.mvs_val);
			return NULL;
		case MVST_NVAL:
			assert(mv_st_ent->mv_st_cont.mvs_nval.mvs_ptab.hte_addr);
			DEBUG_ONLY(hte = lookup_hashtab_mname(&curr_symval->h_symtab, &mv_st_ent->mv_st_cont.mvs_nval.name));
			assert(hte);
			assert(hte == mv_st_ent->mv_st_cont.mvs_nval.mvs_ptab.hte_addr);
			hte = mv_st_ent->mv_st_cont.mvs_nval.mvs_ptab.hte_addr;
			lvval_ptr = (lv_val *)hte->value;	/* lv_val being popped */
			DBGRFCT_ONLY(
				memcpy(vname.c, hte->key.var_name.addr, hte->key.var_name.len);
				vname.c[hte->key.var_name.len] = '\0';
			);
			DBGRFCT((stderr, "unw_mv_ent3: var '%s' hte 0x"lvaddr" being reset from 0x"lvaddr" to 0x"lvaddr"\n",
				 &vname.c, hte, hte->value, mv_st_ent->mv_st_cont.mvs_nval.mvs_ptab.save_value));
			hte->value = (char *)mv_st_ent->mv_st_cont.mvs_nval.mvs_ptab.save_value;
			/* See comment in handling of MVST_PVAL above for content and treatment of lvval_ptr */
			if (lvval_ptr)
			{
				DECR_BASE_REF_NOSYM(lvval_ptr, FALSE);
			}
			return NULL;
		case MVST_STCK:
		case MVST_STCK_SP:
			assert(mvs_size[MVST_STCK] == mvs_size[MVST_STCK_SP]);
			if (0 < mv_st_ent->mv_st_cont.mvs_stck.mvs_stck_size)
				memcpy(*(mv_st_ent->mv_st_cont.mvs_stck.mvs_stck_addr), (char*)mv_st_ent+mvs_size[MVST_STCK],
				       mv_st_ent->mv_st_cont.mvs_stck.mvs_stck_size);
			else
				*(mv_st_ent->mv_st_cont.mvs_stck.mvs_stck_addr) =
					(unsigned char *)mv_st_ent->mv_st_cont.mvs_stck.mvs_stck_val;
			return NULL;
		case MVST_TVAL:
			dollar_truth = (boolean_t)mv_st_ent->mv_st_cont.mvs_tval;
			return NULL;
		case MVST_TPHOLD:
			return NULL;	/* just a place holder for TP */
		case MVST_ZINTR:
			/* Restore environment to pre-$zinterrupt evocation */
			dollar_zininterrupt = FALSE;
			dollar_truth = (boolean_t)mv_st_ent->mv_st_cont.mvs_zintr.saved_dollar_truth;
			op_gvrectarg(&mv_st_ent->mv_st_cont.mvs_zintr.savtarg);
			extnam_str.len = mv_st_ent->mv_st_cont.mvs_zintr.savextref.len;
			if (extnam_str.len)
			{
				memcpy(extnam_str.addr, mv_st_ent->mv_st_cont.mvs_zintr.savextref.addr, extnam_str.len);
				free(mv_st_ent->mv_st_cont.mvs_zintr.savextref.addr);
			}
			return NULL;
		case MVST_ZINTDEV:
			/* Since the interrupted device frame is popping off, there is no way that the READ
			   that was interrupted will be resumed (if it already hasn't been). We don't bother
			   to check if it is or isn't. We just reset the device.
			*/
			if (NULL == mv_st_ent->mv_st_cont.mvs_zintdev.io_ptr)
				return NULL;	/* already processed */
			switch(mv_st_ent->mv_st_cont.mvs_zintdev.io_ptr->type)
			{
#ifdef UNIX
				case tt:
					if (NULL != mv_st_ent->mv_st_cont.mvs_zintdev.io_ptr)
					{	/* This mv_stent has not been processed yet */
						tt_ptr = (d_tt_struct *)(mv_st_ent->mv_st_cont.mvs_zintdev.io_ptr->dev_sp);
						tt_ptr->mupintr = FALSE;
						tt_ptr->tt_state_save.who_saved = ttwhichinvalid;
						mv_st_ent->mv_st_cont.mvs_zintdev.buffer_valid = FALSE;
						mv_st_ent->mv_st_cont.mvs_zintdev.io_ptr = NULL;
					}
					return NULL;
#endif
				case gtmsocket:
					if (NULL != mv_st_ent->mv_st_cont.mvs_zintdev.io_ptr)
					{	/* This mv_stent has not been processed yet */
						dsocketptr = (d_socket_struct *)(mv_st_ent->mv_st_cont.mvs_zintdev.io_ptr->dev_sp);
						sockintr = &dsocketptr->sock_save_state;
						sockintr->end_time_valid = FALSE;
						sockintr->who_saved = sockwhich_invalid;
						dsocketptr->mupintr = FALSE;
					}
					return NULL;
				default:
					GTMASSERT;	/* No other device should be here */
			}
			break;
		default:
			GTMASSERT;
			return NULL;
	}

	return NULL; /* This should never get executed, added to make compiler happy */
}
