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
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cmidef.h"
#include "cmmdef.h"

GBLREF connection_struct *curr_entry;

bool gtcmtr_initreg()
{
	cm_region_head	*region, *gtcmd_ini_reg();
	cm_region_list	*list_entry;
	unsigned char	*reply;
	void		gtcm_add_region();

	assert(*curr_entry->clb_ptr->mbf == CMMS_S_INITREG);
	region = gtcmd_ini_reg(curr_entry);
	gtcm_add_region(curr_entry,region);

	if (region->reg->max_rec_size + CM_BUFFER_OVERHEAD > curr_entry->clb_ptr->mbl)
	{	free(curr_entry->clb_ptr->mbf);
		curr_entry->clb_ptr->mbf = (unsigned char *)malloc(region->reg->max_rec_size + CM_BUFFER_OVERHEAD);
		curr_entry->clb_ptr->mbl = region->reg->max_rec_size + CM_BUFFER_OVERHEAD;
	}
	reply = curr_entry->clb_ptr->mbf;
	*reply++ = CMMS_T_REGNUM;
	*reply++ = curr_entry->current_region->regnum;
	*reply++ = region->reg->null_subs;
	*((unsigned short *)reply) = region->reg->max_rec_size;
	reply += sizeof(unsigned short);
	*((unsigned short *)reply) = region->reg->max_key_size;
	reply += sizeof(unsigned short);
	curr_entry->clb_ptr->cbl = reply - curr_entry->clb_ptr->mbf;
	return TRUE;
}
