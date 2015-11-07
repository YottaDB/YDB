/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "cmd_qlf.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "hashtab_mname.h"
#include "fix_pages.h"
#include "zbreak.h"
#include "private_code_copy.h"
#include "urx.h"
#include "min_max.h"
#include "stringpool.h"
#include "gtm_text_alloc.h"
#ifdef UNIX
#include "srcline.h"
#endif

#define S_CUTOFF 		7
#define FREE_RTNTBL_SPACE 	17
#define RTNTBL_EXP_MIN (SIZEOF(rtn_tabent) * FREE_RTNTBL_SPACE)	/* never expand the routine name table by less than 17 entries */
#define RTNTBL_EXP_MAX ((16 * 1024) + 1)		/* never expand the routine name table by more than 16KB (at one time) */

GBLREF rtn_tabent	*rtn_fst_table, *rtn_names, *rtn_names_end, *rtn_names_top;
GBLREF stack_frame	*frame_pointer;

bool zlput_rname (rhdtyp *hdr)
{
	rhdtyp		*old_rhead, *rhead;
	rtn_tabent	*rbot, *mid, *rtop;
	stack_frame	*fp, *fpprev;
	char		*src, *new, *old_table;
	int		comp;
	ht_ent_mname    *tabent;
	mname_entry	key;
	uint4		entries;
	mstr		*curline;
	mident		*rtn_name;
	size_t		size, src_len;
#	ifdef VMS
	uint4		*src_tbl;
#	else
	routine_source	*src_tbl;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	rtn_name = &hdr->routine_name;
	rbot = rtn_names;
	rtop = rtn_names_end;
	for (;;)
	{	/* See if routine exists in list via a binary search which reverts to serial
		   search when # of items drops below the threshold S_CUTOFF.
		*/
		if ((rtop - rbot) < S_CUTOFF)
		{
			comp = -1;
			for (mid = rbot; mid <= rtop ; mid++)
			{
				MIDENT_CMP(&mid->rt_name, rtn_name, comp);
				if (0 <= comp)
					break;
			}
			break;
		} else
		{	mid = rbot + (rtop - rbot)/2;
			MIDENT_CMP(&mid->rt_name, rtn_name, comp);
			if (0 == comp)
				break;
			else if (0 > comp)
			{
				rbot = mid + 1;
				continue;
			} else
			{
				rtop = mid - 1;
				continue;
			}
		}
	}
	if (comp)
	{	/* Entry was not found. Add in a new one */
		old_table = NULL;
		src = (char *)mid;
		src_len = (char *)rtn_names_end - (char *)mid + SIZEOF(rtn_tabent);
		if (rtn_names_end >= rtn_names_top)
		{	/* Not enough room, recreate table in larger area, try to expand exponentially */
			size = (char *)rtn_names_end - (char *)rtn_names;
			size = ROUND_UP(size +
				((RTNTBL_EXP_MIN > size) ? RTNTBL_EXP_MIN : ((RTNTBL_EXP_MAX < size) ? RTNTBL_EXP_MAX : size)),
				SIZEOF(rtn_tabent));
			new = malloc(size);
			memcpy(new, rtn_names, (char *)mid - (char *)rtn_names);
			mid = (rtn_tabent *)((char *)mid + (new - (char *)rtn_names));
			old_table = (char *)rtn_names;
			/* Adjust rtn_named_end to point into new table by applying offset to new block */
			rtn_names_end = (rtn_tabent *)((char *)rtn_names_end + (new - (char *)rtn_names));
			rtn_names = (rtn_tabent *)new;
			rtn_names_top = (rtn_tabent *)(new + size - SIZEOF(rtn_tabent));
			memset(rtn_names_end + 1, 0, size - ((char *)(rtn_names_end + 1) - new));
		}
		memmove(mid + 1, src, src_len);
		mid->rt_name = *rtn_name;
		rtn_names_end++;
		if (old_table && old_table != (char *)rtn_fst_table)
			free(old_table);		/* original table can't be freed */
		assert(NON_USHBIN_ONLY(!hdr->old_rhead_ptr) USHBIN_ONLY(!hdr->old_rhead_adr));
	} else
	{	/* Entry exists. Update it */
		old_rhead = (rhdtyp *)mid->rt_adr;
		/* Verify routine is not currently active. If it is, we cannot replace it */
		for (fp = frame_pointer; fp ; fp = fpprev)
		{
			fpprev = fp->old_frame_pointer;
#			ifdef GTM_TRIGGER
			if (NULL != fpprev && SFT_TRIGR & fpprev->type)
				fpprev = *(stack_frame **)(fpprev + 1);
#			endif
			/* Check all possible versions of each routine header */
			for (rhead = CURRENT_RHEAD_ADR(old_rhead); rhead;
			     rhead = (rhdtyp *)NON_USHBIN_ONLY(rhead->old_rhead_ptr)USHBIN_ONLY(rhead->old_rhead_adr))
				if (fp->rvector == rhead)
					return FALSE;
		}
		zr_remove(old_rhead, NOBREAKMSG); /* get rid of the inactive breakpoints and release private code section */

		/* If source has been read in for old routine, free space. Since routine name is the key, do this before
		   (in USHBIN builds) we release the literal text section as part of the releasable read-only section.
		*/
		tabent = NULL;
		if (NULL != (TREF(rt_name_tbl)).base)
		{
			key.var_name = mid->rt_name;
			COMPUTE_HASH_MNAME(&key);
			if (NULL != (tabent = lookup_hashtab_mname(TADR(rt_name_tbl), &key)) && tabent->value)
			{
#				ifdef VMS
				src_tbl = (uint4 *)tabent->value;
				/* Must delete the entries piece-meal */
				entries = *(src_tbl + 1);
				if (0 != entries)
					/* Don't count line 0 which we bypass */
					entries--;
				/* curline start is 2 uint4s into src_tbl and then space past line 0 or
				   we end up freeing the storage for line 0/1 twice since they have the
				   same pointers.
				*/
				for (curline = RECAST(mstr *)(src_tbl + 2) + 1; 0 != entries; --entries, ++curline)
				{
					assert(curline->len);
					free(curline->addr);
				}
				free(tabent->value);
#				elif defined(UNIX)
				/* Entries and source are malloc'd in two blocks on UNIX */
				src_tbl = (routine_source *)tabent->value;
				if (NULL != src_tbl->srcbuff)
					free(src_tbl->srcbuff);
				free(src_tbl);
#				else
#				  error "unsupported platform"
#				endif
				/* Note that there are two possible ways to proceed here to clear this entry:
				 *   1. Just clear the value as we do below.
				 *   2. Use the DELETE_HTENT() macro to remove the entry entirely from the hash table.
				 *
				 * We choose #1 since a routine that had had its source loaded is likely to have it reloaded
				 * and if the source load rtn has to re-add the key, it won't reuse the deleted key (if it
				 * remained a deleted key) until all other hashtable slots have been used up (creating a long
				 * collision chain). A deleted key may not remain a deleted key if it was reached with no
				 * collisions but will instead be turned into an unused key and be immediately reusable.
				 * But since it is likely to be reused, we just zero the entry but this creates a necessity
				 * that the key be maintained. If this is a non-USBHIN platform, everything stays around
				 * anyway so that's not an issue. However, in a USHBIN platform, the literal storage the key
				 * is pointing to gets released. For that reason, in the USHBIN processing section below, we
				 * update the key to point to the newly loaded module's routine name.
				 */
				tabent->value = NULL;
			}
		}
#		ifndef USHBIN_SUPPORTED
		hdr->old_rhead_ptr = (int4)old_rhead;
		if (!old_rhead->old_rhead_ptr)
		{
		        fix_pages((unsigned char *)old_rhead, (unsigned char *)LNRTAB_ADR(old_rhead)
				  + (SIZEOF(lnr_tabent) * old_rhead->lnrtab_len));
		}
#		else /* USHBIN_SUPPORTED */
		if (!old_rhead->shlib_handle)
	        { 	/* Migrate text literals pointing into text area we are about to throw away into the stringpool.
			   We also can release the read-only releasable segment as it is no longer needed.
			*/
			stp_move((char *)old_rhead->literal_text_adr,
				 (char *)(old_rhead->literal_text_adr + old_rhead->literal_text_len));
			if (tabent)
			{	/* There was (at one time) a $TEXT source section for this routine. We may have just
				   released it but whether the source was for the routine just replaced or for an earlier
				   replacement, the key for that segment is pointing into the readonly storage we
				   are just about to release. Replace the key with the current one for this routine.
				*/
				assert(MSTR_EQ(&tabent->key.var_name, rtn_name));
				tabent->key.var_name = *rtn_name;	/* Update key with newly saved mident */
			}
			zlmov_lnames(old_rhead); /* copy the label names from literal pool to malloc'd area */
			GTM_TEXT_FREE(old_rhead->ptext_adr);
			/* Reset the routine header pointers to the sections we just freed up.
			 * NOTE: literal_text_adr shouldn't be reset as it points to the label area malloc'd
			 * in zlmov_lnames() */
			old_rhead->ptext_adr = old_rhead->ptext_end_adr = NULL;
			old_rhead->lnrtab_adr = NULL;
		}
		urx_remove(old_rhead);
		free(RW_REL_START_ADR(old_rhead));	/* Release the read-write releasable segments */
		old_rhead->literal_adr = NULL;
		old_rhead->vartab_adr = NULL;
		free(old_rhead->linkage_adr);		/* Release the old linkage section */
		old_rhead->linkage_adr = NULL;
		hdr->old_rhead_adr = old_rhead;
#		endif
		mid->rt_name = *rtn_name;
	}
	mid->rt_adr= hdr;
	return TRUE;
}
