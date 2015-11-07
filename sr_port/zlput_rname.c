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
#include "gtmlink.h"
#endif
#include "mmemory.h"

STATICFNDCL boolean_t handle_active_old_versions(rhdtyp *old_rhead, rhdtyp *hdr);
void zr_release(rhdtyp *old_rhead);

#define S_CUTOFF 		7
#define FREE_RTNTBL_SPACE 	17
#define RTNTBL_EXP_MIN (SIZEOF(rtn_tabent) * FREE_RTNTBL_SPACE)	/* never expand the routine name table by less than 17 entries */
#define RTNTBL_EXP_MAX ((16 * 1024) + 1)		/* never expand the routine name table by more than 16KB (at one time) */

GBLREF rtn_tabent	*rtn_fst_table, *rtn_names, *rtn_names_end, *rtn_names_top;
GBLREF stack_frame	*frame_pointer;

bool zlput_rname (rhdtyp *hdr)
{
	rhdtyp		*old_rhead, *rhead, *prev_active;
	rtn_tabent	*mid;
	char		*src, *new, *old_table;
	mident		*rtn_name;
	size_t		size, src_len;
	boolean_t	found;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	rtn_name = &hdr->routine_name;
	found = find_rtn_tabent(&mid, rtn_name);
	if (!found)
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
		rtn_names_end++;
		if (old_table && old_table != (char *)rtn_fst_table)
			free(old_table);		/* original table can't be freed */
		assert(NON_USHBIN_ONLY(!hdr->old_rhead_ptr) USHBIN_ONLY(!hdr->old_rhead_adr));
	} else
	{	/* Entry exists. Update it */
		old_rhead = (rhdtyp *)mid->rt_adr;
		/* Verify routine is not currently active. If it is, we cannot replace it -- wrong */
		USHBIN_ONLY(prev_active = old_rhead->active_rhead_adr);
		if (!handle_active_old_versions(old_rhead, hdr))
			return FALSE;
		USHBIN_ONLY( if (prev_active == old_rhead->active_rhead_adr))
		{	/* old version not in use. free it */
			zr_remove(old_rhead, NOBREAKMSG); /* remove breakpoints (now inactive) */
			/* If source has been read in for old routine, free space. On VMS, source is associated with a routine name
			 * table entry. On UNIX, source is associated with a routine header, and we may have different sources for
			 * different linked versions of the same routine name.
			 */
			free_src_tbl(old_rhead);
			zr_release(old_rhead); /* release private code section */
		}
#		ifndef USHBIN_SUPPORTED
		hdr->old_rhead_ptr = (int4)old_rhead;
#		else /* USHBIN_SUPPORTED */
		hdr->old_rhead_adr = old_rhead;
#		endif
	}
	mid->rt_name = *rtn_name;
	mid->rt_adr = hdr;
	UNIX_ONLY(hdr->source_code = NULL);
	return TRUE;
}

STATICFNDEF boolean_t handle_active_old_versions(rhdtyp *old_rhead, rhdtyp *hdr)
{
	stack_frame	*fp;
	rhdtyp 		*rhead, *new_rhead;
	boolean_t	need_duplicate, on_stack;
	ssize_t		sect_rw_nonrel_size;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(old_rhead == CURRENT_RHEAD_ADR(old_rhead));
	on_stack = FALSE;
	need_duplicate = FALSE;
	for (fp = frame_pointer; NULL != fp; fp = SKIP_BASE_FRAME(fp->old_frame_pointer))
	{
		if (MSTR_EQ(&fp->rvector->routine_name, &old_rhead->routine_name))
		{
			on_stack = TRUE;
			if (CURRENT_RHEAD_ADR(fp->rvector) == old_rhead)
				need_duplicate = TRUE;
		}
	}
	if (on_stack)
	{
#		ifdef USHBIN_SUPPORTED
		if (LINK_NORECURSIVE == TREF(relink_allowed))
#		endif
			return FALSE;
	}
#	ifdef USHBIN_SUPPORTED
	if (need_duplicate)
	{
		new_rhead = (rhdtyp *)malloc(SIZEOF(rhdtyp));
		*new_rhead = *old_rhead;
		new_rhead->current_rhead_adr = new_rhead;
		old_rhead->active_rhead_adr = new_rhead; /* reserve previous version on active chain */
		for (fp = frame_pointer; NULL != fp; fp = SKIP_BASE_FRAME(fp->old_frame_pointer))
			if (CURRENT_RHEAD_ADR(fp->rvector) == old_rhead)
				fp->rvector = new_rhead; /* point frame's code vector at reserved copy of old routine version */
		/* any field (e.g. the label table) that is currently shared by old_rhead needs to be copied to a separate area
		 * other fields (e.g. literal mvals) will be redirected for old_rhead, so we just need to keep the older version
		 * around (don't free it).
		 */
		sect_rw_nonrel_size = old_rhead->labtab_len * SIZEOF(lab_tabent);
		new_rhead->labtab_adr = (lab_tabent *)malloc(sect_rw_nonrel_size);
		memcpy(new_rhead->labtab_adr, old_rhead->labtab_adr, sect_rw_nonrel_size);
		/* make sure to: skip urx_remove, do not free code section, etc. */
		/* ALSO: we need to go through resolve linkage table entries corresponding to this the old version, and re-resolve
		 * them to point into the new version */
	}
#	endif /* USHBIN_SUPPORTED */
	return TRUE;
}

#ifdef USHBIN_SUPPORTED
void zr_release(rhdtyp *old_rhead)
{
	if (!old_rhead->shlib_handle)
        { 	/* Migrate text literals pointing into text area we are about to throw away into the stringpool.
		   We also can release the read-only releasable segment as it is no longer needed.
		*/
		stp_move((char *)old_rhead->literal_text_adr,
			 (char *)(old_rhead->literal_text_adr + old_rhead->literal_text_len));
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
}
#else /* non-USHBIN_SUPPORTED platforms */
void zr_release(rhdtyp *old_rhead)
{
	if (!old_rhead->old_rhead_ptr)
	{
	        fix_pages((unsigned char *)old_rhead, (unsigned char *)LNRTAB_ADR(old_rhead)
			  + (SIZEOF(lnr_tabent) * old_rhead->lnrtab_len));
	}
}
#endif
