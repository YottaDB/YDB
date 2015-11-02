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

mval	*unw_mv_ent(mv_stent *mv_st_ent)
{
	lv_blk		*lp, *lp1;
	symval		*symval_ptr;
	mval		*ret_value;
	ht_ent_mname	*hte;
	d_socket_struct	*dsocketptr;
	socket_interrupt *sockintr;
	UNIX_ONLY(d_tt_struct	*tt_ptr;)

	active_lv = (lv_val *)0; /* if we get here, subscript set was successful, clear active_lv to avoid later cleanup problems */
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
			}
			else if (mv_st_ent->mv_st_cont.mvs_msav.addr == &dollar_zgbldir)
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
			return 0;
		case MVST_MVAL:
		case MVST_IARR:
			return 0;
		case MVST_STAB:
			if (mv_st_ent->mv_st_cont.mvs_stab)
			{
				assert(mv_st_ent->mv_st_cont.mvs_stab == curr_symval);
				symval_ptr = curr_symval;
				curr_symval = symval_ptr->last_tab;
				free(symval_ptr->first_block.lv_base);
				for (lp = symval_ptr->first_block.next ; lp ; lp = lp1)
				{
					free(lp->lv_base);
					lp1 = lp->next;
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
			}
			return 0;
		case MVST_NTAB:
			hte = lookup_hashtab_mname(&curr_symval->h_symtab, mv_st_ent->mv_st_cont.mvs_ntab.nam_addr);
			assert(hte);
			hte->value = (char *)mv_st_ent->mv_st_cont.mvs_ntab.save_value;
			if (mv_st_ent->mv_st_cont.mvs_ntab.lst_addr)
				*mv_st_ent->mv_st_cont.mvs_ntab.lst_addr = (lv_val *)hte->value;
			return 0;
		case MVST_PARM:
			if (mv_st_ent->mv_st_cont.mvs_parm.mvs_parmlist)
				free(mv_st_ent->mv_st_cont.mvs_parm.mvs_parmlist);
			ret_value = mv_st_ent->mv_st_cont.mvs_parm.ret_value;
			if (ret_value)
				dollar_truth = (boolean_t)mv_st_ent->mv_st_cont.mvs_parm.save_truth;
			return ret_value;
		case MVST_PVAL:
			if (mv_st_ent->mv_st_cont.mvs_pval.mvs_ptab.nam_addr)
			{
				hte = lookup_hashtab_mname(&curr_symval->h_symtab,
							   mv_st_ent->mv_st_cont.mvs_pval.mvs_ptab.nam_addr);
				assert(hte);
				hte->value = (char *)mv_st_ent->mv_st_cont.mvs_pval.mvs_ptab.save_value;
				if (mv_st_ent->mv_st_cont.mvs_pval.mvs_ptab.lst_addr)
					*mv_st_ent->mv_st_cont.mvs_pval.mvs_ptab.lst_addr = (lv_val *)hte->value;
				if (mv_st_ent->mv_st_cont.mvs_pval.mvs_val->ptrs.val_ent.children)
					lv_killarray(mv_st_ent->mv_st_cont.mvs_pval.mvs_val->ptrs.val_ent.children);
			}
			mv_st_ent->mv_st_cont.mvs_pval.mvs_val->ptrs.free_ent.next_free = curr_symval->lv_flist;
			curr_symval->lv_flist = mv_st_ent->mv_st_cont.mvs_pval.mvs_val;
			return 0;
		case MVST_NVAL:
			hte = lookup_hashtab_mname(&curr_symval->h_symtab, &mv_st_ent->mv_st_cont.mvs_nval.name);
			assert(hte);
			hte->value = (char *)mv_st_ent->mv_st_cont.mvs_nval.mvs_ptab.save_value;
			if (mv_st_ent->mv_st_cont.mvs_nval.mvs_ptab.lst_addr)
				*mv_st_ent->mv_st_cont.mvs_nval.mvs_ptab.lst_addr = (lv_val *)hte->value;
			if (mv_st_ent->mv_st_cont.mvs_nval.mvs_val->ptrs.val_ent.children)
				lv_killarray(mv_st_ent->mv_st_cont.mvs_nval.mvs_val->ptrs.val_ent.children);
			mv_st_ent->mv_st_cont.mvs_nval.mvs_val->ptrs.free_ent.next_free = curr_symval->lv_flist;
			curr_symval->lv_flist = mv_st_ent->mv_st_cont.mvs_nval.mvs_val;
			return 0;
		case MVST_STCK:
			if (0 < mv_st_ent->mv_st_cont.mvs_stck.mvs_stck_size)
			{
				memcpy(*(mv_st_ent->mv_st_cont.mvs_stck.mvs_stck_addr), (char*)mv_st_ent+mvs_size[MVST_STCK],
				       mv_st_ent->mv_st_cont.mvs_stck.mvs_stck_size);
			}
			else {
				*(mv_st_ent->mv_st_cont.mvs_stck.mvs_stck_addr) =
					(unsigned char *)mv_st_ent->mv_st_cont.mvs_stck.mvs_stck_val;
			}
			return 0;
		case MVST_TVAL:
			dollar_truth = (boolean_t)mv_st_ent->mv_st_cont.mvs_tval;
			return 0;
		case MVST_TPHOLD:
			return 0;	/* just a place holder for TP */
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
			return 0;
		case MVST_ZINTDEV:
			/* Since the interrupted device frame is popping off, there is no way that the READ
			   that was interrupted will be resumed (if it already hasn't been). We don't bother
			   to check if it is or isn't. We just reset the device.
			*/
			if (NULL == mv_st_ent->mv_st_cont.mvs_zintdev.io_ptr)
				return 0;	/* already processed */
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
					return 0;
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
					return 0;
				default:
					GTMASSERT;	/* No other device should be here */
			}
			break;
		default:
			GTMASSERT;
			return 0;
	}

	return 0; /* This should never get executed, added to make compiler happy */
}
