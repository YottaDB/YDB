/****************************************************************
 *								*
 * Copyright (c) 2002-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <rtnhdr.h>
#include "private_code_copy.h"
#include "gtm_text_alloc.h"
#include "arlinkdbg.h"
#include "stack_frame.h"

void release_private_code_copy(rhdtyp *rtn)
{
#	ifdef USHBIN_SUPPORTED
	assert(NULL != rtn->shared_ptext_adr);
	assert(rtn->shared_ptext_adr != rtn->ptext_adr);
	DBGARLNK((stderr, "release_private_code_copy: Releasing private code copy at 0x"lvaddr" for rtnhdr 0x"lvaddr"\n",
		  rtn->ptext_adr, rtn));
	adjust_frames(rtn->ptext_adr, rtn->ptext_end_adr, rtn->shared_ptext_adr);
	GTM_TEXT_FREE(rtn->ptext_adr);
	do
	{	/* Since more than one version of a module may exist at a given time, run the routine
		 * header chain and check all versions - releasing the private copy if it exists.
		 */
		DBGARLNK((stderr, "release_private_code_copy: rtnhdr 0x"lvaddr"  Previous values - ptext_adr: 0x"lvaddr
			  "  ptext_end_adr: 0x"lvaddr"\n", rtn, rtn->ptext_adr, rtn->ptext_end_adr));
		rtn->ptext_end_adr = rtn->shared_ptext_adr + (rtn->ptext_end_adr - rtn->ptext_adr);
		rtn->ptext_adr = rtn->shared_ptext_adr;
		DBGARLNK((stderr, "release_private_code_copy: rtnhdr 0x"lvaddr"  New values      - ptext_adr: 0x"lvaddr
			  "  ptext_end_adr: 0x"lvaddr"\n", rtn, rtn->ptext_adr, rtn->ptext_end_adr));
		/* Check for special case loop terminator. If this was a routine copy created when a routine in use was
		 * recursively relinked, we do not want to follow its backchain and change those modules because this
		 * routine copy is not part of that chain. The backpointer is only to find the original routine header
		 * when this routine terminates. So if this routine is a recursive copy, stop the loop now.
		 */
		rtn = ((NULL != rtn->old_rhead_adr) && (rtn != rtn->old_rhead_adr->active_rhead_adr))
			? (rhdtyp *)rtn->old_rhead_adr : NULL;
	} while (NULL != rtn);
	DBGARLNK((stderr, "release_private_code_copy: Complete\n"));
#	endif
	return;
}
