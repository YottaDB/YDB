/****************************************************************
 *								*
 * Copyright (c) 2003-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "urx.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "op.h"
#include <auto_zlink.h>
#include "arlinkdbg.h"
#include "linktrc.h"

#ifndef AUTORELINK_SUPPORTED
# error "Routine should not be built by non-autorelink-enabled platforms"
#endif

GBLREF stack_frame	*frame_pointer;

/* Routine to locate the entry being linked from generated code and link it in and return to the glue code
 * code to drive it.
 *
 * Arguments:
 *
 *   rtnhdridx  - Index into linkage table for the routine that needs linking.
 */
void auto_zlink(int rtnhdridx)
{
	mstr		rname;
	mident_fixed	rname_buff;
	mval		rtn;
	rhdtyp		*rhd;

	assert(0 <= rtnhdridx);			/* rtnhdridx must never be negative */
	assert(rtnhdridx <= frame_pointer->rvector->linkage_len);
	assert(NULL == frame_pointer->rvector->linkage_adr[rtnhdridx].ext_ref);
	rname = frame_pointer->rvector->linkage_names[rtnhdridx];
	rname.addr += (INTPTR_T)frame_pointer->rvector->literal_text_adr;	/* Perform relocation on name */
	memcpy(rname_buff.c, rname.addr, rname.len);
	memset(rname_buff.c + rname.len, 0, SIZEOF(rname_buff) - rname.len);	/* Clear rest of mident_fixed */
	rname.addr = rname_buff.c;
	assert(rname.len <= MAX_MIDENT_LEN);
	assert(NULL == find_rtn_hdr(&rname));
	rtn.mvtype = MV_STR;
	rtn.str.len = rname.len;
	rtn.str.addr = rname.addr;
	op_zlink(&rtn, NULL);			/* op_zlink() takes care of '%' -> '_' translation of routine name */
	DEBUG_ONLY(if ('_' == rname_buff.c[0]) rname_buff.c[0] = '%');
	assert(NULL != (rhd = find_rtn_hdr(&rname)));
	DBGARLNK((stderr, "auto_zlink: Linked in rtn %.*s to "lvaddr"\n", rname.len, rname.addr, find_rtn_hdr(&rname)));
	return;
}

/* Routine to check routine if autorelink is needed.
 *
 * Arguments:
 *
 *   rtnhdridx  - Index into linkage table for the routine that may need re-linking.
 *   lbltblidx  - Index into linkage table for the label table entry needed to get label offset. Main purpose of using this
 *		  routine is it is the indicator for when indirect code is using the lnk_proxy mechanism to pass in entries
 *		  because indirects have no linkage table of their own.
 */
void auto_relink_check(int rtnhdridx, int lbltblidx)
{
	rhdtyp		*rhd, *callerrhd;
	int		nameoff, clen;
	unsigned char	*cptr;
	boolean_t	fnddot, valid_calling_seq;
	DEBUG_ONLY(int	rtnname_len;)
	DEBUG_ONLY(char	*rtnname_adr;);
	DEBUG_ONLY(mident_fixed rtnname;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(0 <= rtnhdridx);
	if ((frame_pointer->flags & SFF_INDCE) || (0 > lbltblidx))
		return;		/* Don't deal with indirects - just let them use the vars they received from op_rhd/labaddr */
	assert(rtnhdridx <= frame_pointer->rvector->linkage_len);
	assert(lbltblidx <= frame_pointer->rvector->linkage_len);
	/* If routine hasn't been linked yet, drive auto_zlink() to pull it in and return (no further checking needed since
	 * it was just linked).
	 */
	if (NULL == frame_pointer->rvector->linkage_adr[rtnhdridx].ext_ref)
	{
		auto_zlink(rtnhdridx);
		return;
	}
	callerrhd = frame_pointer->rvector;				/* rtnhdr of routine doing the calling */
	rhd = (rhdtyp *)callerrhd->linkage_adr[rtnhdridx].ext_ref;	/* rtnhdr of routine being called */
#	ifdef DEBUG
	/* Validate name of routine is as we expect it to be */
	assert(NULL != rhd);
	rtnname_adr = (INTPTR_T)callerrhd->linkage_names[rtnhdridx].addr + (char *)callerrhd->literal_text_adr;
	rtnname_len = callerrhd->linkage_names[rtnhdridx].len;
	memcpy(rtnname.c, rtnname_adr, rtnname_len);
	if ('_' == rtnname.c[0])
		rtnname.c[0] = '%';
	assert((rtnname_len == rhd->routine_name.len)
	       && (0 == memcmp(rtnname.c, rhd->routine_name.addr, rtnname_len)));
#	endif
	DBGARLNK((stderr, "auto_relink_check: rtn %.*s calling rtn: %.*s - arlink_enabled: %d  arlink_loaded: %d\n",
		  callerrhd->routine_name.len, callerrhd->routine_name.addr,
		  rhd->routine_name.len, rhd->routine_name.addr, TREF(arlink_enabled), TREF(arlink_loaded)));
	explicit_relink_check(rhd, FALSE);
}

/* Routine called when have routine name and need to do an explicit autorelink check - either from above or from
 * $TEXT(), ZPRINT or any future users of get_src_line().
 *
 * Argument:
 *
 *   rhd       - routine header of routine to be checked if needs to be relinked.
 *   setproxy  - TRUE if need to set old/new routine header into TABENT_PROXY
 *
 * Routine is already linked, but we need to check if a new version is available. This involves traversing the
 * "validation linked list", looking for changes in different $ZROUTINES entries. But we also need to base our
 * checks on the most recent version of the routine loaded. Note autorelink is only allowed when no ZBREAKs are
 * defined in the given routine.
 */
void explicit_relink_check(rhdtyp *rhd, boolean_t setproxy)
{
	mval		rtnname;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(NULL != rhd);
	assert(!rhd->rtn_relinked);	/* Should never be calling recursively linked routine. All such calls should be going
					 * to the new current routine instead of the recusively linked copy rtnhdr.
					 */
	/* Routine is already linked, but we need to check if a new version is available. This involves traversing the
	 * "validation linked list", looking for changes in different $ZROUTINES entries. But we also need to base our
	 * checks on the most recent version of the routine loaded. Note autorelink is only possible when no ZBREAKs are
	 * defined in the given routine.
	 *
	 * Note the following section is very similar to code in op_rhdaddr.c. Any changes made here need to be echoed there
	 */
 	if (!CURRENT_RHEAD_ADR(rhd)->has_ZBREAK)
	{
		rhd = rhd->current_rhead_adr;		/* Update rhd to most currently linked version */
#		ifdef DEBUG_ARLINK
		DBGARLNK((stderr, "explicit_relink_check: rtn: %.*s  rtnhdr: 0x"lvaddr"  zhist: 0x"lvaddr,
			  rhd->routine_name.len, rhd->routine_name.addr, rhd, rhd->zhist));
		if (NULL != rhd->zhist)
		{
			DBGARLNK((stderr, "  zhist->cycle: %d  zrtns cycle: %d\n", rhd->zhist->zroutines_cycle,
				  TREF(set_zroutines_cycle)));
		} else
		{
			DBGARLNK((stderr, "\n"));
		}
#		endif
		if ((NULL != rhd->zhist) && need_relink(rhd, (zro_hist *)rhd->zhist))
		{	/* Relink appears to be needed. Note we have the routine name from the passed in header address so
			 * only debug builds fetch/compare it against the expected value from the name table.
			 */
			rtnname.mvtype = MV_STR;
			rtnname.str = rhd->routine_name;
			DBGARLNK((stderr,"explicit_relink_check: Routine needs relinking: %.*s\n",
				  rtnname.str.len, rtnname.str.addr));
			op_zlink(&rtnname, NULL);	/* op_zlink() takes care of '%' -> '_' translation of routine name */
			/* Use the fastest way to pickup the routine header address of the current copy. The linker would have
			 * updated the current rtnhdr address in our routine header so this provides the easiest way to find
			 * the newly linked header whether the link was bypassed or not. The only exception to this is when
			 * the current routine is a recursively linked copy routine whose current rtnhdr address always points
			 * to itself but those routines cannot end up here so this is the best way.
			 */
			rhd = rhd->current_rhead_adr;
			/* Note it is possible for multiple processes to interfere with each other and cause the below assert
			 * to fail. Should that happen, the assert can be removed but to date it has not failed so am leaving
			 * it in.
			 */
			assert((NULL == rhd->zhist) || (((zro_hist *)(rhd->zhist))->zroutines_cycle == TREF(set_zroutines_cycle)));
		} else
		{
			DBGARLNK((stderr,"explicit_relink_check: Routine does NOT need relinking: %.*s\n", rhd->routine_name.len,
				  rhd->routine_name.addr));
		}
	} else
	{
		DBGARLNK((stderr,"explicit_relink_check: Routine relink bypassed - has ZBREAKs: %.*s\n", rhd->routine_name.len,
			  rhd->routine_name.addr));
	}
	if (setproxy)
	{
		DBGINDCOMP((stderr, "explicit_relink_check: Indirect call to routine %.*s resolved to 0x"lvaddr"\n",
			    rhd->routine_name.len, rhd->routine_name.addr, rhd));
		(TABENT_PROXY).rtnhdr_adr = rhd;
	}
}
