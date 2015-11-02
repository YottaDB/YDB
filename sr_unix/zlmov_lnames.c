/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <string.h>
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
	/* Store the pointer to malloc'd area in literal_text_adr so it can be accessable from the routine header.
	 * Although we do not need this pointer, it is kept in literal_text_adr which otherwise anyway becomes
	 * dangling after ptext_adr is released */
	hdr->literal_text_adr = (unsigned char *)lab_ptr;
	hdr->literal_text_len = size;
	for (lab_ent = lab_bot + 1; lab_ent < lab_top; lab_ent++)
	{
		memcpy(lab_ptr, lab_ent->lab_name.addr, lab_ent->lab_name.len);
		lab_ent->lab_name.addr = lab_ptr;
		lab_ptr += lab_ent->lab_name.len;
	}
}
#endif /* USHBIN_SUPPORTED */
