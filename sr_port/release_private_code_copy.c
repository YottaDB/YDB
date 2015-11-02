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

#include <rtnhdr.h>
#include "private_code_copy.h"
#include "gtm_text_alloc.h"

void adjust_frames(unsigned char *old_ptext_beg, unsigned char *old_ptext_end, unsigned char *new_ptext_beg);

void release_private_code_copy(rhdtyp *rtn)
{
#ifdef USHBIN_SUPPORTED
		assert(NULL != rtn->shlib_handle);
		assert(NULL != rtn->shared_ptext_adr);

		adjust_frames(rtn->ptext_adr, rtn->ptext_end_adr, rtn->shared_ptext_adr);
		GTM_TEXT_FREE(rtn->ptext_adr);
		rtn->ptext_end_adr = rtn->shared_ptext_adr + (rtn->ptext_end_adr - rtn->ptext_adr);
		rtn->ptext_adr = rtn->shared_ptext_adr;
		rtn->shared_ptext_adr = NULL;
#endif
	return;
}
