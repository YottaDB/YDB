/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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

GBLREF connection_struct *curr_entry;
GBLREF gv_namehead	*gv_target;
GBLREF gv_key		*gv_currkey;

bool gtcmtr_get(void)
{
	unsigned char	*ptr, regnum;
	unsigned short	top, len, temp_short;
	mval		v;
	cm_region_list	*reg_ref;

	ptr = curr_entry->clb_ptr->mbf;
	assert(CMMS_Q_GET == *ptr);
	ptr++;
	GET_USHORT(len, ptr);
	ptr += SIZEOF(unsigned short);
	regnum = *ptr++;
	len--;	/* subtract size of regnum */
	reg_ref = gtcm_find_region(curr_entry, regnum);
	CM_GET_GVCURRKEY(ptr, len);
	gtcm_bind_name(reg_ref->reghead, TRUE);
	ptr = curr_entry->clb_ptr->mbf;
	if (gv_target->root ? gvcst_get(&v) : FALSE)
	{
		temp_short = (unsigned short)v.str.len;
		assert((int4)temp_short == v.str.len); /* ushort <- int4 assignment lossy? */
		if (curr_entry->clb_ptr->mbl < 1 +  /* msg header */
					       SIZEOF(temp_short) + /* size of length of $GET return value */
					       temp_short) /* length of $GET return value */
		{ /* resize buffer */
			cmi_realloc_mbf(curr_entry->clb_ptr, 1 + SIZEOF(temp_short) + temp_short);
			ptr = curr_entry->clb_ptr->mbf;
		}
		*ptr++ = CMMS_R_GET;
		PUT_USHORT(ptr, temp_short);
		ptr += SIZEOF(unsigned short);
		memcpy(ptr, v.str.addr, temp_short);
		ptr += temp_short;
	} else
		*ptr++ = CMMS_R_UNDEF;
	curr_entry->clb_ptr->cbl = ptr - curr_entry->clb_ptr->mbf;
	return TRUE;
}
