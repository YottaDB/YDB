/****************************************************************
 *								*
 *	Copyright 2009 Fidelity Information Services, Inc	*
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

#include "hashtab_mname.h"	/* needed for lv_val.h */
#include "hashtab.h"
#include "lv_val.h"
#include "sbs_blk.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"

/* Routine to clone the children of a tree. The input lv_val should be a clone of the base lv_val
   owning the tree we wish to clone. The pointers in this copy will be duplicated and the new tree
   linked to this base mval. Note that the owning symval of clone_var should be set appropriately
   such as in the case of xnew processing where we are cloneing a tree out of one symtab and into
   another. The new tree will be created in the symtab of the input lv_val regardless of which
   symtab owns the processed lv_vals.
 */

void lv_var_clone(lv_val *clone_var)
{
	lv_val		*lv, *clv;
	sbs_blk		**num, **str, *newsbs;
	lv_sbs_tbl	*tbl, *newtbl;
	symval		*ownsym;
	int		i;

	assert(clone_var);
	DBGRFCT((stderr, "\nlv_var_clone: Cloning lv_val tree at 0x"lvaddr"\n", clone_var));
	if (tbl = clone_var->ptrs.val_ent.children)	/* tbl holds tree to be cloned as we build new tree for clone_var */
	{
		ownsym = (MV_SBS == clone_var->ptrs.val_ent.parent.sym->ident) ? clone_var->ptrs.val_ent.parent.sbs->sym :
			clone_var->ptrs.val_ent.parent.sym;
		assert(MV_SYM == ownsym->ident);
		assert(MV_SBS == tbl->ident);
		assert(MV_SYM == tbl->sym->ident);
		clone_var->ptrs.val_ent.children = (lv_sbs_tbl *)lv_getslot(ownsym);
		newtbl = clone_var->ptrs.val_ent.children;
		memcpy(newtbl, tbl, sizeof(lv_sbs_tbl));
		newtbl->lv = clone_var;
		newtbl->sym = ownsym;	/* Needed in clone calls from xnew processing since sym is going to be different */
		num = &newtbl->num;
		str = &newtbl->str;
		if (*num)
		{	/* Process numeric subscripts first */
			if (newtbl->int_flag)
			{	/* We have a fixed number of integer subscripts (index into table) */
				assert(!(*num)->nxt);
				newsbs = (sbs_blk *)lv_get_sbs_blk(ownsym);
				assert(0 == newsbs->cnt);
				assert(0 == newsbs->nxt);
				assert(newsbs->sbs_que.fl && newsbs->sbs_que.bl);
				newsbs->cnt = (*num)->cnt;
				memcpy(&newsbs->ptr, &(*num)->ptr, sizeof(newsbs->ptr));
				for (i = 0;  i < SBS_NUM_INT_ELE;  i++)
				{
					if (newsbs->ptr.lv[i])
					{	/* Since this is a direct indexed table element, not every element will have
						   a value. This one does. */
						lv = lv_getslot(ownsym);
						*lv = *newsbs->ptr.lv[i];
						lv->ptrs.val_ent.parent.sbs = newtbl;
						newsbs->ptr.lv[i] = lv;
						if (lv->ptrs.val_ent.children)
							lv_var_clone(lv);
					}
				}
				*num = newsbs;
			} else
			{	/* The numbers are in floating point representation or there are too many to fit in a single
				   indexed table of ints. Every element contains a pointer to a number and multiple blocks
				   may be chained together.
				*/
				while (*num)
				{
					newsbs = (sbs_blk *)lv_get_sbs_blk(ownsym);
					assert(0 == newsbs->cnt);
					assert(0 == newsbs->nxt);
					assert(newsbs->sbs_que.fl && newsbs->sbs_que.bl);
					newsbs->cnt = (*num)->cnt;
					memcpy(&newsbs->ptr, &(*num)->ptr, sizeof(newsbs->ptr));
					for (i = 0;  i < newsbs->cnt;  i++)
					{
						lv = lv_getslot(ownsym);
						*lv = *newsbs->ptr.sbs_flt[i].lv;
						lv->ptrs.val_ent.parent.sbs = newtbl;
						newsbs->ptr.sbs_flt[i].lv = lv;
						if (lv->ptrs.val_ent.children)
							lv_var_clone(lv);
					}
					newsbs->nxt = (*num)->nxt;
					*num = newsbs;
					num = &newsbs->nxt;
				}
			}
		}
		while (*str)
		{	/* Each element contains a string subscript. Multiple blocks may be chained together. */
			newsbs = (sbs_blk *)lv_get_sbs_blk(ownsym);
			assert(0 == newsbs->cnt);
			assert(0 == newsbs->nxt);
			assert(newsbs->sbs_que.fl && newsbs->sbs_que.bl);
			newsbs->cnt = (*str)->cnt;
			memcpy(&newsbs->ptr, &(*str)->ptr, sizeof(newsbs->ptr));
			for (i = 0;  i < newsbs->cnt;  i++)
			{
				lv = lv_getslot(ownsym);
				*lv = *newsbs->ptr.sbs_str[i].lv;
				lv->ptrs.val_ent.parent.sbs = newtbl;
				newsbs->ptr.sbs_str[i].lv = lv;
				if (lv->ptrs.val_ent.children)
					lv_var_clone(lv);
			}
			newsbs->nxt = (*str)->nxt;
			*str = newsbs;
			str = &newsbs->nxt;
		}
	}
}
