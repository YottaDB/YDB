/****************************************************************
 *								*
 *	Copyright 2002, 2007 Fidelity Information Services, Inc	*
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

CONDITION_HANDLER(cre_priv_ch)
{
	error_def(UNIX_ONLY(ERR_MEMORY) VMS_ONLY(ERR_VMSMEMORY);)

	START_CH;
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

	error_def(UNIX_ONLY(ERR_MEMORY) VMS_ONLY(ERR_VMSMEMORY);)

#ifdef USHBIN_SUPPORTED
		assert(NULL != rtn->shlib_handle); /* don't need private copy if not shared */
		assert(NULL == rtn->shared_ptext_adr); /* if already private, we shouldn't be calling this routine */
		code_size = (int)(rtn->ptext_end_adr - rtn->ptext_adr) ;
		ESTABLISH_RET(cre_priv_ch, UNIX_ONLY(ERR_MEMORY) VMS_ONLY(ERR_VMSMEMORY));
		new_ptext = GTM_TEXT_ALLOC(code_size);
		REVERT;
		memcpy(new_ptext, rtn->ptext_adr, code_size);
		adjust_frames(rtn->ptext_adr, rtn->ptext_end_adr, new_ptext);
		rtn->shared_ptext_adr = rtn->ptext_adr;
		rtn->ptext_adr = new_ptext;
		rtn->ptext_end_adr = new_ptext + code_size;
		inst_flush(new_ptext, code_size);
#endif
	return SS_NORMAL;
}
