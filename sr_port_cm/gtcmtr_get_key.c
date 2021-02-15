/****************************************************************
 *								*
 * Copyright (c) 2021 Fidelity National Information		*
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

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gtcm_bind_name.h"
#include "gvcst_protos.h"	/* for gvcst_get prototype */
#include "gtcm_find_region.h"
#include "gtcmtr_protos.h"
#include "copy.h"

unsigned char *gtcmtr_get_key(void *gvkey, unsigned char *ptr, unsigned short len)
{
	/* Fetch gv_key fields from message buffer and store into 'key' whose allocated buffer
	 * size is key->top, the maximum length allowed and enforced by the runtime. Return
	 * the advanced ptr which points to the byte after key */
	gv_key *key = (gv_key *)gvkey;
	ptr += sizeof(unsigned short);
	GET_USHORT(key->end, ptr);
	ptr += sizeof(unsigned short);
	GET_USHORT(key->prev, ptr);
	ptr += sizeof(unsigned short);
	assert((len > 6) && (key->top > (len - 6)));
	memcpy(key->base, ptr, len - 6);
	ptr += (len - 6);

	return ptr;
}
