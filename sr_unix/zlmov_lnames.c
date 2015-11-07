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
#include "arlinkdbg.h"

#ifdef USHBIN_SUPPORTED
/* Routine to copy label names out of a shared object's literal text pool for a version of an object
 * about to be released to allow label text to still be available for other routines who have links to
 * the label table which has pointers to these label names. The label names are copied into malloc'd
 * storage for the duration of that routine's life in this process.
 *
 * Parameter:
 *
 *   - hdr - Routine header address whose label table needs to be saved.
 *
 * Future change (GTM-8144) would eliminate the need for this.
 */
void zlmov_lnames(rhdtyp *hdr)
{
	lab_tabent	*lab_ent, *lab_bot, *lab_top;
	uint4		size;
	char		*lab_ptr;

	lab_bot = hdr->labtab_adr;
	lab_top = hdr->labtab_adr + hdr->labtab_len;
	size = 0;
	assert((NULL != lab_bot) && (0 == lab_bot->lab_name.len)); /* The first label is null label */
	/* Compute size of label names */
	for (lab_ent = lab_bot + 1; lab_ent < lab_top; lab_ent++)
	{
		assert((lab_ent->lab_name.addr >= (char *)hdr->literal_text_adr)
		       && (lab_ent->lab_name.addr < (char *)(hdr->literal_text_adr + hdr->literal_text_len)));
		size += lab_ent->lab_name.len;
	}
	if (0 < size)
	{
		lab_ptr = (char *)malloc(size);
		DBGARLNK((stderr, "zlmov_lnames: Label names copied from rtn %.*s (rtnhdr 0x"lvaddr") to malloc'd space at 0x"lvaddr
			  " len %d\n", hdr->routine_name.len, hdr->routine_name.addr, hdr, lab_ptr, size));
		/* Store address of malloc'd label text block in the routine header so we can find it to release it on an unlink-all
		 * (ZGOTO 0:entryref).
		 */
		hdr->lbltext_ptr = (unsigned char *)lab_ptr;
		for (lab_ent = lab_bot + 1; lab_ent < lab_top; lab_ent++)
		{
			memcpy(lab_ptr, lab_ent->lab_name.addr, lab_ent->lab_name.len);
			lab_ent->lab_name.addr = lab_ptr;
			lab_ptr += lab_ent->lab_name.len;
		}
	} else
		DBGARLNK((stderr, "zlmov_lnames: Label names for rtn %.*s (rtnhdr 0x"lvaddr" not copied (nothing to copy)\n",
			  hdr->routine_name.len, hdr->routine_name.addr, hdr));
}
#endif /* USHBIN_SUPPORTED */
