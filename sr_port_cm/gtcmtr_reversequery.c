/****************************************************************
 *								*
 * Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Code in this module is based on gtcmtr_query.c and hence has an
 * FIS copyright even though this module was not created by FIS.
 */

#include "mdef.h"

#include <stddef.h>

#include "gtm_string.h"

#include "copy.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gtcm_find_region.h"
#include "gtcm_bind_name.h"
#include "gtcmtr_protos.h"
#include "gv_xform_key.h"
#include "gvcst_protos.h"	/* for gvcst_reversequery prototype */

GBLREF connection_struct *curr_entry;
GBLREF gv_namehead	*gv_target;
GBLREF gv_key		*gv_currkey;
GBLREF gv_key		*gv_altkey;
GBLREF gd_region        *gv_cur_region;

/* This function is invoked by the GT.CM GNP server on behalf of a $query(gvn,-1) operation on the client.
 * This function sets "gv_currkey" from the input message and invokes "gvcst_reversequery" (which does the $query)
 * and copies the result (which is in "gv_altkey") in a message that is sent back to the client by the caller.
 * Also note that the general flow below is similar to that of "gtcmtr_query.c".
 */
bool gtcmtr_reversequery(void)
{
	unsigned char	*ptr, *gv_key_top_ptr, regnum;
	unsigned short	top, old_top;
	unsigned short	key_len, tmp_len;
	cm_region_list	*reg_ref;
	mval		val;
	boolean_t	found, is_null;
	uint4		msg_len;

	ASSERT_IS_LIBGNPSERVER;
	ptr = curr_entry->clb_ptr->mbf;
	assert(CMMS_Q_REVERSEQUERY == *ptr);
	ptr++;
	GET_USHORT(tmp_len, ptr);
	ptr += SIZEOF(short);
	regnum = *ptr++;
	reg_ref = gtcm_find_region(curr_entry, regnum);
	tmp_len--;	/* subtract size of regnum */
	assert(0 == offsetof(gv_key, top));
	GET_USHORT(old_top, ptr); /* old_top = ((gv_key *)ptr)->top; */
	/* See gtcmtr_zprevious.c for comments on "old_top" */
	CM_GET_GVCURRKEY(ptr, tmp_len);
	gtcm_bind_name(reg_ref->reghead, FALSE); /* gtcm_bind_name sets gv_target; do not use gv_target before gtcm_bind_name */
	GTCMTR_SUBS2STR_XFORM_IF_NEEDED(gv_target, gv_currkey, old_top);
	found = (0 != gv_target->root) ? gvcst_reversequery() : FALSE;
	if (found)
	{
		if (gv_target->nct || gv_target->collseq)
			gv_xform_key(gv_altkey, TRUE);
		/* key_len = SIZEOF(gv_key) + gv_altkey->end; */
		key_len = gv_altkey->end + SIZEOF(unsigned short) + SIZEOF(unsigned short) + SIZEOF(unsigned short) + SIZEOF(char);
	} else
		key_len = 0;
	msg_len = SIZEOF(unsigned char) + SIZEOF(unsigned short) + SIZEOF(unsigned char) + key_len;
	if (msg_len > curr_entry->clb_ptr->mbl)
		cmi_realloc_mbf(curr_entry->clb_ptr, msg_len);
	ptr = curr_entry->clb_ptr->mbf;
	*ptr++ = CMMS_R_REVERSEQUERY;
	tmp_len = key_len + 1;
	PUT_USHORT(ptr, tmp_len);
	ptr += SIZEOF(unsigned short);
	*ptr++ = regnum;
	gv_key_top_ptr = ptr;
	if (found)
	{
		ptr += SIZEOF(unsigned short);
		PUT_USHORT(ptr, gv_altkey->end);
		ptr += SIZEOF(unsigned short);
		PUT_USHORT(ptr, gv_altkey->prev);
		ptr += SIZEOF(unsigned short);
		memcpy(ptr, gv_altkey->base, key_len - SIZEOF(unsigned short) - SIZEOF(unsigned short) - SIZEOF(unsigned short));
		ptr += (key_len - SIZEOF(unsigned short) - SIZEOF(unsigned short) - SIZEOF(unsigned short));
	}
	PUT_USHORT(gv_key_top_ptr, old_top); /* ((gv_key *)ptr)->top = old_top; */
	curr_entry->clb_ptr->cbl = msg_len;
	return TRUE;
}
