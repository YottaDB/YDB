/****************************************************************
 *                                                              *
 *      Copyright 2009 Fidelity Information Services, Inc *
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_string.h"

#include "hashtab.h"
#include "hashtab_mname.h"      /* needed for lv_val.h */
#include "lv_val.h"
#include "op.h"
#include "gdsroot.h"
#include "stringpool.h"
#include "stp_parms.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"

GBLREF symval           *curr_symval;
GBLREF lv_val		*active_lv;
GBLREF mstr             **stp_array;
GBLREF int              stp_array_size;

/* Delete all aliases and the data they point to.

   Two things need to happen:
   1) Scan the hash table for aliased base variables and remove them.
   2) Scan ALL subscripted vars for containers, delete the data they point to maintaining
      proper maintenance of reference counts and unmark the container making it a normal value.

   Note that use of lv_kill(lv, TRUE) to clear values also does the requisite TP restart
   var protection of current values so we need not worry about it in this routine.

   Since an alias with two references ceases being an alias if a reference is killed, we
   cannot just do a simple scan and delete references. We will leave "klingons" who used to
   be aliases but now are not. So we do this in a 3 step procedure:
   1) Identify all overt aliases in the hash table.
   2) For the entries that are not aliases but which contain alias containers, scan those
      arrays for containers, remove the container attribute and do the necessary refcnt
      cleanup. When all containers have been removed (including step 3), any remaining
      orphaned data will be recovered by the next LVGC.
   3) Go through the list of HTEs recorded in step one and kill their hash table reference.
*/
void op_killaliasall(void)
{
	ht_ent_mname    *tabent, *tabent_top, **htearray, **htearraytop, **htep;
	lv_blk		*lvbp;
	lv_val		*lvp, *lvp_top, *lvrefp;

	active_lv = (lv_val *)NULL;	/* if we get here, subscript set was successful.  clear active_lv to avoid later
					   cleanup problems */
        if (NULL == stp_array)
                /* Same initialization as is in stp_gcol_src.h */
                stp_array = (mstr **)malloc((stp_array_size = STP_MAXITEMS) * sizeof(mstr *));
	htearray = htep = (ht_ent_mname **)stp_array;
	htearraytop = htearray + stp_array_size;

	/* First pass through hash table we record HTEs that have > 1 trefcnt. We will delete these in a later
	   loop but don't want to delete any until all are found.
	*/
	for (tabent = curr_symval->h_symtab.base, tabent_top = curr_symval->h_symtab.top;
	     tabent < tabent_top;
	     tabent++)
	{
		if (HTENT_VALID_MNAME(tabent, lv_val, lvp) && lvp && (1 < lvp->stats.trefcnt))
		{	/* Verify room in the table, expand if necessary */
			if (htep >= htearraytop)
			{	/* No room and the inn .. expand */
				stp_expand_array();
				htearray = htep = (ht_ent_mname **)stp_array;
				htearraytop = htearray + stp_array_size;
			}
			*htep++ = tabent;
		}
	}
	/* This next, less scenic trip through the hash table entries we scan any arrays we
	   find for containers that must be dealt with. We couldn't deal with these until all
	   the "blatant" aliases were identified.
	*/
	for (tabent = curr_symval->h_symtab.base, tabent_top = curr_symval->h_symtab.top;
	     tabent < tabent_top;
	     tabent++)
	{
		if (HTENT_VALID_MNAME(tabent, lv_val, lvp) && lvp && (1 == lvp->stats.trefcnt))
		{	/* Var was not an alias but now need to check if var has any containers in it
			   that likewise need to be processed (and de-container-ized).
			*/
			if (lvp->ptrs.val_ent.children)
				KILL_CNTNRS_IN_TREE(lvp);
		}
	}
	/* Now we can go through the hash table entries we identified in the first step and
	   delete them.
	*/
	for (htearraytop = htep, htep = htearray; htep < htearraytop; ++htep)
	{
		assert(htep);
		tabent = *htep;
		lvp = (lv_val *)tabent->value;
		assert(lvp);
		assert(0 < lvp->stats.trefcnt);
		lv_kill(lvp, TRUE);
		DECR_BASE_REF(tabent, lvp, TRUE);
	}
  }
