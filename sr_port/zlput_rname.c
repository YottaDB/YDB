/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
# include "incr_link_sp.h"
#endif
#include "zr_unlink_rtn.h"
#ifdef UNIX
# include "srcline.h"
# include "gtmlink.h"
# include "arlinkdbg.h"
#endif
#include "mmemory.h"

STATICFNDCL boolean_t handle_active_old_versions(boolean_t *duplicated, rhdtyp *old_rhead);

#define S_CUTOFF 		7
#define FREE_RTNTBL_SPACE 	17
#define RTNTBL_EXP_MIN (SIZEOF(rtn_tabent) * FREE_RTNTBL_SPACE)	/* Never expand the routine name table by less than 17 entries */
#define RTNTBL_EXP_MAX ((16 * 1024) + 1)		/* Never expand the routine name table by more than 16KB (at one time) */

GBLREF rtn_tabent	*rtn_fst_table, *rtn_names, *rtn_names_end, *rtn_names_top;
GBLREF stack_frame	*frame_pointer;
GBLREF z_records	zbrk_recs;

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
		if (!handle_active_old_versions(&duplicated, old_rhead))
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
 *
 * How multiple versions of a given routine work:
 *   a. The original routine header needs to stay as it is. The address of this routine header can be in
 *      the linkage tables of multiple routines throughout the system. Those routines making a (new) call
 *	to this routine should get the newest version so the original routine header is modified by
 *	incr_link() to point to the stuff of the newest version.
 *   b. Ditto the label table whose entries are updated by incr_link() to point to the lnrtab of the most
 * 	recent routine.
 *   c. Due to a & b, in order to preserve the old rtnhdr and label table, we make a copy of both of them
 *      updating the routine header to point to the copied label table. This works because the copied label
 *      table still points to the old lnrtab which the copied rtnhdr "inherits" (incr_link() assumes
 * 	zlput_rname() released it but it didn't so the copied rtnhdr "owns" it now).
 *   d. As for the linkage table, that too is "inherited" by the copied rtnhdr. Its resolved addresses
 *      still point to what they used to and its unresolved addresses still have entries on the unresolved
 *	chain so if a new routine resolves, it would fill in the entries of both the latest version of the
 *	given routine as well as the copied version.
 *   e. The copied routine header is set into all M stack frames using the old routine or its predecessors.
 *	When the last user of the copied routine is popped, the copied routine is cleaned and the copied
 *      structures released.
 */
STATICFNDEF boolean_t handle_active_old_versions(boolean_t *duplicated, rhdtyp *old_rhead)
{
	stack_frame	*fp;
	rhdtyp 		*rhead, *copy_rhead;
	boolean_t	need_duplicate, is_on_stack;
	ssize_t		sect_rw_nonrel_size;
	DBGARLNK_ONLY(rhdtyp * fprhd;)
#	ifdef USHBIN_SUPPORTED
	zbrk_struct	*z_ptr;
#	endif
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
		DBGARLNK((stderr, "handle_active_old_versions: Routine %.*s (rtnhdr 0x"lvaddr") needs a duplicate created\n",
			  old_rhead->routine_name.len, old_rhead->routine_name.addr, old_rhead));
		copy_rhead = (rhdtyp *)malloc(SIZEOF(rhdtyp));
		*copy_rhead = *old_rhead;
		copy_rhead->current_rhead_adr = copy_rhead;	/* Grabs of current fhead need to stay with this one for this
								 * older version
								 */
		copy_rhead->old_rhead_adr = old_rhead;	/* Link copied routine back to its original flavor. Used to know which
							 * routine should have its active_rhead_adr field cleared when the copy
							 * becomes inactive and gets cleaned up.
							 */
		/* Since old_rhead comes from the most currently linked routine in the routine table, and that value gets
		 * replaced when we are done, it should be impossible for this value to be set more than once. Assert that.
		 */
		assert(NULL == old_rhead->active_rhead_adr);
		old_rhead->active_rhead_adr = copy_rhead; /* Reserve previous version on active chain */
		for (fp = frame_pointer; NULL != fp; fp = SKIP_BASE_FRAME(fp->old_frame_pointer))
		{
			DBGARLNK_ONLY(fprhd = CURRENT_RHEAD_ADR(fp->rvector));
			if (CURRENT_RHEAD_ADR(fp->rvector) == old_rhead)
			{
				DBGARLNK((stderr, "handle_active_old_versions: Frame pointer 0x"lvaddr" for routine %.*s ("
					  "rvector 0x"lvaddr") changed to copy-rtnhdr 0x"lvaddr"\n", fp,
					  fprhd->routine_name.len, fprhd->routine_name.addr, fprhd, copy_rhead));
				fp->rvector = copy_rhead; /* Point frame's code vector at reserved copy of old routine version */
			} else
			{
				DBGARLNK((stderr, "handle_active_old_versions: Frame pointer 0x"lvaddr" for routine %.*s ("
					  "rvector 0x"lvaddr") not modified - does not match old_rhead (0x"lvaddr")\n", fp,
					  fprhd->routine_name.len, fprhd->routine_name.addr, fprhd, old_rhead));
			}
		}
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
		old_rhead->lbltext_ptr = NULL;	 /* We can clean this up when the copy unwinds - ignore it in original rhdr */
		/* When a routine is relinked, the routine generally goes through zr_unlink_rtn() but since this routine is
		 * recursively relinked, most of the fields of this routine are now attached to the copy rtnhdr. Those fields
		 * will be returned when the last instance of the copy leaves the stack. But this routine still needs to have
		 * its label names saved. We need to do this now as the copy rtnhdr has its own separate copy of the label
		 * table so if it hasn't already been done, do it now.
		 */
		if (NULL == old_rhead->shlib_handle)
			zlmov_lnames(old_rhead);
		/* If any breakpoints are active for old_rhead, fix "rtnhdr" in those to point to "copy_rhead" instead */
		if (old_rhead->has_ZBREAK)
		{
			z_ptr = zr_find(&zbrk_recs, (zb_code *)PTEXT_ADR(old_rhead), RETURN_CLOSEST_MATCH_TRUE);
			assert(NULL != z_ptr);
			if (NULL != z_ptr)
			{
				for ( ; z_ptr >= zbrk_recs.beg; z_ptr--)
				{
					if (!ADDR_IN_CODE((unsigned char *)z_ptr->mpc, old_rhead))
						break;
					z_ptr->rtnhdr = copy_rhead;
				}
			}
			old_rhead->has_ZBREAK = FALSE;
		}
		*duplicated = TRUE;
		DBGARLNK((stderr, "handle_active_old_versions: Routine %.*s (rtnhdr 0x"lvaddr") recursively relinked - copied "
			  "rtnhdr: 0x"lvaddr"\n", old_rhead->routine_name.len, old_rhead->routine_name.addr, old_rhead,
			  copy_rhead));
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
		DBGARLNK((stderr, "zr_cleanup_recursive_rtn: Recursively linked routine copy %.*s (rtnhdr 0x"lvaddr") being "
			  "cleaned up\n", rtnhdr->routine_name.len, rtnhdr->routine_name.addr, rtnhdr));
		assert(rtnhdr->old_rhead_adr->active_rhead_adr == rtnhdr);
		assert(NULL == rtnhdr->active_rhead_adr);
		zr_unlink_rtn(rtnhdr, FALSE);			/* Cleans up the copy */
		assert(!rtnhdr->has_ZBREAK);
		assert(!rtnhdr->old_rhead_adr->has_ZBREAK);	/* These should both be cleared now if ever set */
		free(rtnhdr->labtab_adr);
		/* We should have avoided saving label text for this recursively relinked routine. Assert that no cleanup is
		 * necessary.
		 */
		assert(NULL == rtnhdr->lbltext_ptr);
		rtnhdr->old_rhead_adr->active_rhead_adr = NULL;
		free(rtnhdr);
	} else
	{
		DBGARLNK((stderr, "zr_cleanup_recursive_rtn: Recursively linked routine copy %.*s (rtnhdr 0x"lvaddr") being "
			  "left alone - still active\n", rtnhdr->routine_name.len, rtnhdr->routine_name.addr, rtnhdr));
	}
}
#endif /* USHBIN_SUPPORTED */
