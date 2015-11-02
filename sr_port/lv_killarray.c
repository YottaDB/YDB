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

#include "gtm_stdio.h"
#include "gtm_string.h"

#include "hashtab_mname.h"	/* needed for lv_val.h */
#include "lv_val.h"
#include "sbs_blk.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"

/* Note it is important that callers of this routine make sure that the pointer that is passed as
   an argument is removed from the lv_val it came from prior to the call. This prevents arrays
   that have alias containers pointing that form a loop back to the originating lv_val from causing
   processing loops, in effect over-processing arrays that have already been processed or lv_vals
   that have been deleted.
*/
void lv_killarray(lv_sbs_tbl *a, boolean_t dotpsave)
{
       	register lv_val *lv;
	sbs_flt_struct	*f;
	sbs_str_struct	*s;
	int4		i;
	lv_sbs_tbl	*tbl, *tmpsbs;
	sbs_blk		*blk, *temp;
	char		*top;
	register lv_val **fl;

	assert(a);
	assert(a->sym->ident == MV_SYM);
	assert(NULL == a->lv->ptrs.val_ent.children);	/* Owner lv's children pointer MUST be NULL! */
       	fl = &(a->sym->lv_flist);
	if (a->num)
	{
		if (a->int_flag)
		{
			assert(a->num);
			blk = a->num;
 	 	   	for (i = 0; i < SBS_NUM_INT_ELE; i++)
       	       	       	{
				if (lv = blk->ptr.lv[i])
		   	 	{
		   			if (tmpsbs = lv->ptrs.val_ent.children)	/* note assignment */
					{
						assert(lv == tmpsbs->lv);
						lv->ptrs.val_ent.children = NULL;
						lv_killarray(tmpsbs, dotpsave);
					}
					DECR_AC_REF(lv, dotpsave);	/* Decrement alias contain ref and cleanup if necessary */
					LV_FLIST_ENQUEUE(fl, lv);
		   		}
	       	   	}
       	       	       	lv_free_sbs_blk(blk);
		   	a->int_flag = FALSE;
		} else
       	       	{
			for (blk = a->num; blk; )
		 	{
				for (f = &blk->ptr.sbs_flt[0], top = (char *)&blk->ptr.sbs_flt[blk->cnt];
				     f < (sbs_flt_struct *)top; f++)
		 	 	{
					lv = f->lv;
		 			if (tmpsbs = lv->ptrs.val_ent.children)	/* note assignment */
					{
						assert(lv == tmpsbs->lv);
						lv->ptrs.val_ent.children = NULL;
						lv_killarray(tmpsbs, dotpsave);
					}
					DECR_AC_REF(lv, dotpsave);	/* Decrement alias contain ref and cleanup if necessary */
					LV_FLIST_ENQUEUE(fl, lv);
		 		}
				temp = blk->nxt;
		 		lv_free_sbs_blk(blk);
		 		blk = temp;
	 	 	}
		}
		a->num = NULL;
	}
	if (a->str)
	{
		for (blk = a->str; blk; )
	 	{
			for (s = &blk->ptr.sbs_str[0], top = (char *)&blk->ptr.sbs_str[blk->cnt];
			     s < (sbs_str_struct *)top; s++)
	 	 	{
				lv = s->lv;
	 			if (tmpsbs = lv->ptrs.val_ent.children)	/* note assignment */
				{
					assert(lv == tmpsbs->lv);
					lv->ptrs.val_ent.children = NULL;
					lv_killarray(tmpsbs, dotpsave);
				}
				DECR_AC_REF(lv, dotpsave);	/* Decrement alias contain ref and cleanup if necessary */
				LV_FLIST_ENQUEUE(fl, lv);
	 		}
			temp = blk->nxt;
	 		lv_free_sbs_blk(blk);
	 		blk = temp;
	 	}
	 	a->str = NULL;
       	}
	/* now free the subscript table */
	lv = (lv_val *)a;
	LV_FLIST_ENQUEUE(fl, lv);
	return;
}
