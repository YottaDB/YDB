/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "copy.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cmidef.h"
#include "hashdef.h"
#include "cmmdef.h"
#include "gtcmd.h"
#include "gtcm_add_region.h"
#include "gtcmtr_initreg.h"

GBLREF connection_struct *curr_entry;

bool gtcmtr_initreg(void)
{
	cm_region_head	*region;
	cm_region_list	*list_entry;
	unsigned char	*reply;
	unsigned short temp_short;

	assert(*curr_entry->clb_ptr->mbf == CMMS_S_INITREG);
	region = gtcmd_ini_reg(curr_entry);
	gtcm_add_region(curr_entry,region);

	if (region->reg->max_rec_size + CM_BUFFER_OVERHEAD > curr_entry->clb_ptr->mbl)
	{
		free(curr_entry->clb_ptr->mbf);
		curr_entry->clb_ptr->mbf = (unsigned char *)malloc(region->reg->max_rec_size + CM_BUFFER_OVERHEAD);
		curr_entry->clb_ptr->mbl = region->reg->max_rec_size + CM_BUFFER_OVERHEAD;
	}
	reply = curr_entry->clb_ptr->mbf;
	*reply++ = CMMS_T_REGNUM;
	*reply++ = curr_entry->current_region->regnum;
	*reply++ = region->reg->null_subs;
	temp_short = (unsigned short)region->reg->max_rec_size;
	assert((int4)temp_short == region->reg->max_rec_size); /* ushort <- int4 assignment lossy? */
	PUT_USHORT(reply, temp_short);
	reply += sizeof(unsigned short);
	temp_short = (unsigned short)region->reg->max_key_size;
	assert((int4)temp_short == region->reg->max_key_size); /* ushort <- int4 assignment lossy? */
	PUT_USHORT(reply, temp_short);
	reply += sizeof(unsigned short);
	curr_entry->clb_ptr->cbl = reply - curr_entry->clb_ptr->mbf;
	return TRUE;
}
