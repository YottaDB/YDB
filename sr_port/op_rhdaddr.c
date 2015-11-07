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

#include "gtm_string.h"

#include <rtnhdr.h>
#include "stack_frame.h"
#include "op.h"
#include "min_max.h"
#include "linktrc.h"

/* Rebuffering macro for routine and label name for use when needed. Note we don't even do the
 * MV_FORCE_STR() on the given mval until we know we are going to use it. Primary purpose here is
 * to rebuffer a routine name coming from user space truncating it as necessary.
 */
#define REBUFFER_MIDENT(MVAL, NEWMVAL, BUFFER)					\
{										\
	MV_FORCE_STR(MVAL);							\
	*(NEWMVAL) = *(MVAL);							\
	(NEWMVAL)->str.len = MIN(MAX_MIDENT_LEN, (NEWMVAL)->str.len);		\
	memcpy((void *)&BUFFER, (NEWMVAL)->str.addr, (NEWMVAL)->str.len);	\
	(NEWMVAL)->str.addr = (char *)&(BUFFER);				\
}
/* Macro to do extra VMS checks in INITZLINK below*/
#if defined (__alpha) && defined (__vms)
# define VMSCHECK 												\
{														\
	RHD = RHD->linkage_ptr;											\
	if (NULL == RHD)											\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_ZLINKFILE, 2, NAME->str.len, NAME->str.addr,	\
			      ERR_ZLMODULE, 2, strlen(&zlink_mname.c[0]), zlink_mname.c);			\
}
#else
# define VMSCHECK
#endif
/* Macro to provide initial link to a given routine */
#define INITZLINK(NAME, RHD, RTNNAME, RTNNAME_BUFF)								\
{														\
	REBUFFER_MIDENT(NAME, &RTNNAME, RTNNAME_BUFF);								\
	op_zlink(&RTNNAME, NULL);										\
	RHD = find_rtn_hdr(&RTNNAME.str);									\
	if (NULL == RHD)											\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_ZLINKFILE, 2, NAME->str.len, NAME->str.addr,	\
			      ERR_ZLMODULE, 2, strlen(&zlink_mname.c[0]), &zlink_mname);			\
	VMSCHECK;												\
	ARLINK_ONLY(DBGINDCOMP((stderr, "op_rhdaddr: routine linked (initial) to 0x"lvaddr"\n", RHD)));		\
	ARLINK_ONLY(TADR(lnk_proxy)->rtnhdr_adr = RHD);								\
}

GBLREF mident_fixed	zlink_mname;
GBLREF rtn_tabent	*rtn_names;
GBLREF stack_frame	*frame_pointer;

error_def(ERR_ZLINKFILE);
error_def(ERR_ZLMODULE);

/* Locate and return the routine header for a given routine. If routine is already linked in, and this platform
 * is auto-relink enabled, also verify this is the most current version of this routine. Since this routine is
 * only called from indirects or from call-ins, for an autorelink-enabled build, set the address into the proxy
 * linkage table and return its index. Else return the routine header address itself.
 *
 * Arguments:
 *
 *    name   - An mval containing the name of the routine.
 *    rhd    - (Non-autorelink-enabled build only) routine header address if available.
 *    rhdidx - (Autorelink-enabled build only) index into linkage table or -1 if not available.
 *
 * Return value:
 *
 *    autorelink-enabled build - returns 0 (index into lnk_proxy where routine address is saved).
 *    non-autorelink-enabled build - returns rtnhdr address.
 *
 * Note OC_RHDADDR1 comes here also but with an always NULL rhd parm. The rhd parm is provided for those times
 * where M code like 'SET X=@LBL^FIXEDRTN' is used where the routine is fixed and the label is indirect. In this
 * one case, we can pass the routine header address to op_rhdaddr so it doesn't have to be looked up on each
 * execution like it does in a true indirect that lives in the indirect cache.
 */
ARLINK_ONLY(int op_rhdaddr(mval *name, int rhdidx))
NON_ARLINK_ONLY(rhdtyp *op_rhdaddr(mval *name, rhdtyp *rhd))
{
	mident_fixed	rtnname_buff;
	mval		rtnname;
#	ifdef AUTORELINK_SUPPORTED
	rhdtyp		*rhd;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	DBGINDCOMP((stderr, "op_rhdaddr: Args: name: %.*s  rhdidx: %d\n", name->str.len, name->str.addr, rhdidx));
	/* If rhd is not defined (NULL), the routine is not yet linked in so do that so set it into PROXY_TABENT
	 * and return 0 (index into lnk_proxy for the routine header address.
	 */
	assert((-1 == rhdidx) || ((0 <= rhdidx) && (rhdidx <= frame_pointer->rvector->linkage_len)));
	rhd = (-1 == rhdidx) ? NULL : (rhdtyp *)frame_pointer->rvector->linkage_adr[rhdidx].ext_ref;
	/* If below block is changed, check if need to update bock not related to autorelink at bottom of routine */
	if ((NULL == rtn_names) || ((NULL == rhd) && (NULL == (rhd = find_rtn_hdr(&name->str)))))	/* Note assignment */
	{	/* Initial check for rtn_names is so we avoid the call to find_rtn_hdr() if we have just
		 * unlinked all modules as find_rtn_hdr() does not deal well with an empty rtn table.
		 */
		INITZLINK(name, rhd, rtnname, rtnname_buff);
		return 0;
	}
	/* Routine is already linked, but we need to check if a new version is available. This involves traversing the
	 * "validation linked list", looking for changes in different $ZROUTINES entries. But we also need to base our
	 * checks on the most recent version of the routine loaded. Note autorelink is only possible when no ZBREAKs are
	 * defined in the given routine.
	 *
	 * Note the following section is very similar to code in explicit_relink_check() in auto_zlink.c. Any changes made
	 * here need to be echoed there.
	 */
	if (!rhd->has_ZBREAK)
	{
		rhd = rhd->current_rhead_adr;		/* Update rhd to most currently linked version */
		if ((NULL != rhd->zhist) && need_relink(rhd, (zro_hist *)rhd->zhist))
		{	/* Relink appears to be needed */
			REBUFFER_MIDENT(name, &rtnname, rtnname_buff);
			op_zlink(&rtnname, NULL);
			rhd = rhd->current_rhead_adr;		/* Pickup routine header of new version to avoid lookup */
			assert(NULL != rhd);
			TADR(lnk_proxy)->rtnhdr_adr = rhd;
			DBGINDCOMP((stderr, "op_rhdaddr: routine relinked resolved to 0x"lvaddr"\n", rhd));
			assert((NULL == rhd->zhist) || (((zro_hist *)(rhd->zhist))->zroutines_cycle == TREF(set_zroutines_cycle)));
		} else
		{
			TADR(lnk_proxy)->rtnhdr_adr = rhd;
			DBGINDCOMP((stderr, "op_rhdaddr: routine (no relink) resolved to 0x"lvaddr"\n", rhd));
		}
	} else
	{
		DBGINDCOMP((stderr, "op_rhdaddr: routine has ZBREAK - resolved to 0x"lvaddr"\n", rhd));
		TADR(lnk_proxy)->rtnhdr_adr = rhd;
	}
	return 0;
#	else	/* Not AUTORELINK_SUPPORTED - old-style */
	/* If below block is changed, check if need to update bock related to autorelink at near top of this routine */
	if ((NULL == rtn_names) || ((NULL == rhd) && (NULL == (rhd = find_rtn_hdr(&name->str)))))	/* Note assignment */
	{	/* Initial check for rtn_names is so we avoid the call to find_rtn_hdr() if we have just
		 * unlinked all modules as find_rtn_hdr() does not deal well with an empty rtn table.
		 */
		INITZLINK(name, rhd, rtnname, rtnname_buff);
	}
	return rhd;
#	endif
}
