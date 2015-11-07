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
#include <rtnhdr.h>

#ifdef USHBIN_SUPPORTED
void zlmov_lnames(rhdtyp *hdr)
{
	lab_tabent	*lab_ent, *lab_bot, *lab_top;
	uint4		size;
	char		*lab_ptr;

	lab_bot = hdr->labtab_adr;
	lab_top = hdr->labtab_adr + hdr->labtab_len;
	size = 0;
	assert(NULL != lab_bot && 0 == lab_bot->lab_name.len); /* The first label is null label */
	for (lab_ent = lab_bot + 1; lab_ent < lab_top; lab_ent++)
	{
		assert(lab_ent->lab_name.addr >= (char *)hdr->literal_text_adr &&
			lab_ent->lab_name.addr < (char *)(hdr->literal_text_adr + hdr->literal_text_len));
		size += lab_ent->lab_name.len;
	}
	lab_ptr = (char *)malloc(size);
	/* Previously, we would store the pointer to this malloc'd area in literal_text_adr so it could be accessable from the
	 * routine header. However, we never use the field (e.g., to free it), so there's no point in saving it. With dynamic
	 * literals, we still need literal_text_adr, so let's not do that.
	 */
	for (lab_ent = lab_bot + 1; lab_ent < lab_top; lab_ent++)
	{
		memcpy(lab_ptr, lab_ent->lab_name.addr, lab_ent->lab_name.len);
		lab_ent->lab_name.addr = lab_ptr;
		lab_ptr += lab_ent->lab_name.len;
	}
}
#endif /* USHBIN_SUPPORTED */
