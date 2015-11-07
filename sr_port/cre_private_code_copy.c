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

#include "iosp.h"
#include "error.h"
#include <rtnhdr.h>
#include "inst_flush.h"
#include "private_code_copy.h"
#include "stack_frame.h"
#include "gtm_text_alloc.h"
#include "gtm_string.h"
#include "arlinkdbg.h"

error_def(UNIX_ONLY(ERR_MEMORY) VMS_ONLY(ERR_VMSMEMORY);)

CONDITION_HANDLER(cre_priv_ch)
{

	START_CH(TRUE);
	if (SIGNAL == UNIX_ONLY(ERR_MEMORY) VMS_ONLY(ERR_VMSMEMORY))
	{
		UNWIND(NULL, NULL); /* ignore "lack-of-memory" error, rather not set breakpoint than error out in such a case */
	}
	NEXTCH;
}

uint4 cre_private_code_copy(rhdtyp *rtn)
{
	unsigned char	*new_ptext;
	int		code_size;

#	ifdef USHBIN_SUPPORTED
	assert(NULL != rtn->shared_ptext_adr); 			/* Don't need private copy if not shared */
	assert(rtn->shared_ptext_adr == rtn->ptext_adr); 	/* If already private, we shouldn't be calling this routine */
	code_size = (int)(rtn->ptext_end_adr - rtn->ptext_adr) ;
	ESTABLISH_RET(cre_priv_ch, UNIX_ONLY(ERR_MEMORY) VMS_ONLY(ERR_VMSMEMORY));
	new_ptext = GTM_TEXT_ALLOC(code_size);
	REVERT;
	DBGARLNK((stderr, "cre_private_code_copy: Creating private code copy for rtnhdr 0x"lvaddr" at 0x"lvaddr"\n",
		  rtn, rtn->ptext_adr));
	memcpy(new_ptext, rtn->ptext_adr, code_size);
	adjust_frames(rtn->ptext_adr, rtn->ptext_end_adr, new_ptext);
	do
	{
		DBGARLNK((stderr, "cre_private_code_copy: rtnhdr 0x"lvaddr"  Previous values - ptext_adr: 0x"lvaddr
			  "  ptext_end_adr: 0x"lvaddr"\n", rtn, rtn->ptext_adr, rtn->ptext_end_adr));
		rtn->ptext_adr = new_ptext;
		rtn->ptext_end_adr = new_ptext + code_size;
		DBGARLNK((stderr, "cre_private_code_copy: rtnhdr 0x"lvaddr"  New values      - ptext_adr: 0x"lvaddr
			  "  ptext_end_adr: 0x"lvaddr"\n", rtn, rtn->ptext_adr, rtn->ptext_end_adr));
		/* Check for special case loop terminator. If this was a routine copy created when a routine in use was
		 * recursively relinked, we do not want to follow its backchain and change those modules because this
		 * routine copy is not part of that chain. The backpointer is only to find the original routine header
		 * when this routine terminates. So if this routine is a recursive copy, stop the loop now.
		 */
		rtn = ((NULL != rtn->old_rhead_adr) && (rtn != rtn->old_rhead_adr->active_rhead_adr))
			? (rhdtyp *)rtn->old_rhead_adr : NULL;
	} while (NULL != rtn);
	inst_flush(new_ptext, code_size);
	DBGARLNK((stderr, "cre_private_code_copy: Complete\n"));
#	endif
	return SS_NORMAL;
}
