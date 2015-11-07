/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <sys/types.h>
#include <sys/mman.h>
#ifdef UNIX
#include <sys/shm.h>
#endif

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
#ifdef USHBIN_SUPPORTED
#include <incr_link_sp.h>
#endif
#include "zr_unlink_rtn.h"
#ifdef UNIX
#include "srcline.h"
#include "gtmlink.h"
#endif
#include "mmemory.h"

STATICFNDCL boolean_t handle_active_old_versions(boolean_t *duplicated, rhdtyp *old_rhead, rhdtyp *hdr);

#define S_CUTOFF 		7
#define FREE_RTNTBL_SPACE 	17
#define RTNTBL_EXP_MIN (SIZEOF(rtn_tabent) * FREE_RTNTBL_SPACE)	/* Never expand the routine name table by less than 17 entries */
#define RTNTBL_EXP_MAX ((16 * 1024) + 1)		/* Never expand the routine name table by more than 16KB (at one time) */

GBLREF rtn_tabent	*rtn_fst_table, *rtn_names, *rtn_names_end, *rtn_names_top;
GBLREF stack_frame	*frame_pointer;

/* Routine to perform routine table maintenance. In addition, if a routine is being replaced instead of just added, performs
 * the necessary maintenance to the replaced routine.
 *
 * Parameter:
 *   hdr - routine header address of the routine header just linked in
 *
 * Return value:
 *   TRUE  - If routine updated/added.
 *   FALSE - If routine could not be updated/added.
 */
bool zlput_rname (rhdtyp *hdr)
{
	rhdtyp		*old_rhead, *rhead;
	rtn_tabent	*mid;
	char		*src, *new, *old_table;
	mident		*rtn_name;
	size_t		size, src_len;
	boolean_t	found, duplicated;
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
		/* Verify routine is not currently active. If it is, we must duplicate it and keep it around */
		if (!handle_active_old_versions(&duplicated, old_rhead, hdr))
			return FALSE;
		if (!duplicated)
			zr_unlink_rtn(old_rhead, FALSE); /* Free releasable pieces of old routines */
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

/* Routine to discover if a given routine name is on the M stack and, if TRUE, optionally identify whether the routine
 * is the same (exact) routine we are running now (which some routines care about since it signals a need to clone the
 * header so the original can be repurposed for a new flavor of the routine).
 *
 * Parameters:
 *   rtnhdr	    - The routine header to start our backwards search at.
 *   need_duplicate - Address of boolean_t to set if is the exact same routine (same routine header address).
 *
 * Return value:
 *   TRUE -  If a stack frame specifies the supplied routine header.
 *   FALSE - If no stack frame specifies the supplied routine header.
 */
boolean_t on_stack(rhdtyp *rtnhdr, boolean_t *need_duplicate)
{
	rhdtyp		*rhdr;
	stack_frame	*fp;

	if (NULL != need_duplicate)
		*need_duplicate = FALSE;
	for (fp = frame_pointer; NULL != fp; fp = SKIP_BASE_FRAME(fp->old_frame_pointer))
	{
		if (MSTR_EQ(&fp->rvector->routine_name, &rtnhdr->routine_name))
		{
			if ((NULL != need_duplicate) && (CURRENT_RHEAD_ADR(fp->rvector) == rtnhdr))
				*need_duplicate = TRUE;
			return TRUE;
		}
	}
	return FALSE;
}

/* Routine to:
 *   1. Check if the specified routine is currently on the M stack and if it is then
 *   2. If we are running with VIEW LINK:RECURSIVE.
 *   3. If no, then return FALSE to signify need for an error.
 *   4. If yes, we need to do a number of things to allow both versions of the same routine to
 *      exist on the M stack at the same time.
 *
 * Parameters:
 *   duplicated  - Pointer to boolean return flag indicating we duplicated the input routine header and
 *                 related fields.
 *   old_rhead   - The routine header being replaced/checked.
 *   hdr         - The new routine header doing the replacing.
 */
STATICFNDEF boolean_t handle_active_old_versions(boolean_t *duplicated, rhdtyp *old_rhead, rhdtyp *hdr)
{
	stack_frame	*fp;
	rhdtyp 		*rhead, *copy_rhead;
	boolean_t	need_duplicate, is_on_stack;
	ssize_t		sect_rw_nonrel_size;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(old_rhead == CURRENT_RHEAD_ADR(old_rhead));
	is_on_stack = FALSE;
	*duplicated = FALSE;
	is_on_stack = on_stack(old_rhead, &need_duplicate);
	if (is_on_stack)
	{
#		ifdef USHBIN_SUPPORTED
		if (LINK_NORECURSIVE == TREF(relink_allowed))
#		endif
			return FALSE;
	}
#	ifdef USHBIN_SUPPORTED
	if (need_duplicate)
	{	/* The routine is on the M stack so is active. To allow it to stay there while we also link in a newer version
		 * we need to make some changes to the older version. The way the replaced routine would normally be handled if
		 * we weren't keeping it is the various sections of it would be released and the fields in its routine header be
		 * reset to point to those sections in the latest version. That's still going to happen so what we do here is to
		 * create a SEPARATE copy of the routine header that WON'T be modified so all its fields are intact and as they
		 * were along with a separate copy of the label table since the replacement process also resets the label table
		 * to point to the new version of the routine. Lastly, we'll run back through the stack changing the stack frame's
		 * "rvector" field to point to the copied routine header so it continues to work as it did.
		 */
		copy_rhead = (rhdtyp *)malloc(SIZEOF(rhdtyp));
		*copy_rhead = *old_rhead;
		copy_rhead->current_rhead_adr = copy_rhead;	/* Grabs of current fhead need to stay with this one for this
								 * older version
								 */
		copy_rhead->old_rhead_adr = old_rhead;	/* Link copied routine back to its original flavor. Used to know which
							 * routine should have its active_rhead_adr field cleared when the copy
							 * becomes inactive and gets cleaned up.
							 */
		assert(NULL == old_rhead->active_rhead_adr);
		old_rhead->active_rhead_adr = copy_rhead; /* Reserve previous version on active chain */
		for (fp = frame_pointer; NULL != fp; fp = SKIP_BASE_FRAME(fp->old_frame_pointer))
			if (CURRENT_RHEAD_ADR(fp->rvector) == old_rhead)
				fp->rvector = copy_rhead; /* Point frame's code vector at reserved copy of old routine version */
		/* Need to preserve original copy of label table for as long as this routine is active since it is used by
		 * routines like get_symb_line (or symb_line() it calls) and find_line_start() and find_line_addr(). Once the
		 * routine is no longer active, this label table and the copied routine header can go away since no other
		 * routine would have been able to resolve to it. We call the label table copy sect_rw_nonrel_* here because
		 * that is how this section is identified everywhere else. But here, it is releasable when inactive.
		 */
		sect_rw_nonrel_size = old_rhead->labtab_len * SIZEOF(lab_tabent);
		copy_rhead->labtab_adr = (lab_tabent *)malloc(sect_rw_nonrel_size);
		memcpy(copy_rhead->labtab_adr, old_rhead->labtab_adr, sect_rw_nonrel_size);
		copy_rhead->rtn_relinked = TRUE; /* This flag is checked on unwind to see if routine should be cleaned up */
		*duplicated = TRUE;
	}
#	endif /* USHBIN_SUPPORTED */
	return TRUE;
}

#ifdef USHBIN_SUPPORTED
/* Routine called from op_unwind/unw_retarg/flush_jmp (via CLEANUP_COPIED_RECURSIVE_RTN macro) when a routine pops with the
 * rtn_relinked flag set indicating potential need to clean up a copied routine header and label table - along with the rest
 * of the routine's pieces which were purposely bypassed since the routine was on the M stack at the time.
 */
void zr_cleanup_recursive_rtn(rhdtyp *rtnhdr)
{
	stack_frame	*fp;

	/* See if routine is still in use */
	for (fp = frame_pointer; fp; fp = SKIP_BASE_FRAME(fp->old_frame_pointer))
	{
		if (rtnhdr == fp->rvector)
			break; /* Found reference - not done with it */
	}
	if (NULL == fp)
	{	/* We reached the end of the stack without finding the routine, seems ripe for cleaning */
		assert(rtnhdr->old_rhead_adr->active_rhead_adr == rtnhdr);
		assert(NULL == rtnhdr->active_rhead_adr);
		zr_unlink_rtn(rtnhdr, FALSE);			/* Cleans up the copy */
		assert(!rtnhdr->has_ZBREAK);
		assert(!rtnhdr->old_rhead_adr->has_ZBREAK);	/* These should both be cleared now if ever set */
		free(rtnhdr->labtab_adr);
		rtnhdr->old_rhead_adr->active_rhead_adr = NULL;
		free(rtnhdr);
	}
}
#endif /* USHBIN_SUPPORTED */
