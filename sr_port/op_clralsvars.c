/****************************************************************
 *								*
 *	Copyright 2009, 2011 Fidelity Information Services, Inc	*
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

#include <rtnhdr.h>
#include "stack_frame.h"
#include "op.h"
#include "hashtab_mname.h"
#include "lv_val.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"

GBLREF symval		*curr_symval;
GBLREF stack_frame	*frame_pointer;
GBLREF unsigned char	*stackbase, *stacktop, *msp, *stackwarn;

/* Run the local var hash table looking for variables with the name "$ZWRTAC*" and performing
   "KILL *" processing on any that are found and remove them from the hash table.
*/
void op_clralsvars(lv_val *rslt)
{
	int			htcnt, delcnt, table_size_orig;
	boolean_t		done;
	hash_table_mname        *table;
	ht_ent_mname		*table_base, *table_top, **last_lsym_hte, **htep, *table_base_orig;
	ht_ent_mname    	*tabent, *tabent_top;
	stack_frame		*fp, *fpprev;
	lv_val			*lvp;
 	DEBUG_ONLY(boolean_t	first_sym;)

	delcnt = 0;
	/* Remove $ZWRTAC* vars from hash table. Hash table shrink may be triggered in the next hashtab call */
	for (tabent = curr_symval->h_symtab.base, tabent_top = curr_symval->h_symtab.top; tabent < tabent_top; tabent++)
	{
		if (HTENT_VALID_MNAME(tabent, lv_val, lvp))
		{	/* Check if this var is $ZWRTAC* */
			if ((STR_LIT_LEN(DOLLAR_ZWRTAC) <= tabent->key.var_name.len)
				&& (0 == MEMCMP_LIT(tabent->key.var_name.addr, DOLLAR_ZWRTAC)))
			{	/* Note use here of delete_hashtab_ent_mname() instead of delete_hashtab_mname() not
				 * only saves looking up the entry in the hashtab but avoids the chance of hashtable
				 * compaction which would be *really* bad since this call is not protected with the
				 * wrapper that add_hashtab_mname_symval() has to fix the l_symtab entries.
				 */
				DECR_BASE_REF_RQ(tabent, lvp, FALSE);		/* Our ref disappears like the symbol */
				delete_hashtab_ent_mname(&curr_symval->h_symtab, tabent);	/* Remove from symbol table */
				++delcnt;
			}
		}
	}
	if (delcnt)
	{	/* Vars have been removed from hash table. Now find/clear any l_symtab entries that pointed
		 * to these vars (basically looking for pointers to deleted entries). Note this loop is similar
		 * to stack frame loops in als_lsymtab_repair().
		 */
		last_lsym_hte = NULL;
		done = FALSE;
		fp = frame_pointer;
		table_base = curr_symval->h_symtab.base;
		table_top = table_base + curr_symval->h_symtab.size;
		assert(frame_pointer);
		do
		{
			if (fp->l_symtab != last_lsym_hte)
			{	/* Different l_symtab than last time (don't want to update twice) */
				last_lsym_hte = fp->l_symtab;
				if (fp->vartab_len)
				{	/* Only process non-zero length l_symtabs */
					DEBUG_ONLY(first_sym = TRUE);
					for (htep = fp->l_symtab, htcnt = fp->vartab_len; htcnt; --htcnt, ++htep)
					{
						tabent = *htep;
						if (NULL == tabent)
							continue;
						if (tabent < table_base || tabent >= table_top)
						{	/* Entry doesn't point to the current symbol table */
							assert(first_sym);
							done = TRUE;
							break;
						}
						if (HTENT_MARK_DELETED(tabent))
							*htep = NULL;		/* Clear l_symtab value for deleted hash entry */
						DEBUG_ONLY(first_sym = FALSE);
					}
				}
			}
			if (done)
				break;
			fpprev = fp;
			fp = fp->old_frame_pointer;
			if (SFF_CI & fpprev->flags)
			{	/* Callins needs to be able to crawl past apparent end of stack to earlier stack segments */
				/* We should be in the base frame now. See if an earlier frame exists */
				fp = *(stack_frame **)(fp + 1);	/* Backups up to "prev pointer" created by base_frame() */
				if (NULL == fp || fp >= (stack_frame *)stackbase || fp < (stack_frame *)stacktop)
					break;	/* Pointer not within the stack -- must be earliest occurence */
			}
		} while(fp);
		/* Now that $ZWRTAC* vars have been deleted, we should check if compaction is a suggested thing
		 * to get rid of the deleted vars which will improve the free slot search and reduce the slot scan
		 * on a gargabe collection (both types). If it is, compact in a safe way and do the necessary l_symtab
		 * fixup.
		 */
		if (COMPACT_NEEDED(&curr_symval->h_symtab))
		{	/* Step 1: Remember the current table */
			table = &curr_symval->h_symtab;
			table_base_orig = table->base;
			table_size_orig = table->size;
 			/* Step 2: compact the table */
			/* We'll do the base release once we do the reparations */
			DEFER_BASE_REL_HASHTAB(table, TRUE);
			compact_hashtab_mname(&curr_symval->h_symtab);
			/* Step 3: fix l_symtab and related entries */
			if (table_base_orig != curr_symval->h_symtab.base)
			{
				/* Only needed if expansion was successful */
				als_lsymtab_repair(table, table_base_orig, table_size_orig);
				FREE_BASE_HASHTAB(table, table_base_orig);
			}
			DEFER_BASE_REL_HASHTAB(table, FALSE);
		}
 	}
}
