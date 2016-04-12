/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include "hashtab_mname.h"    /* needed for cmmdef.h */
#include "cmmdef.h"
#include "gtcmd.h"
#include "gtcm_add_region.h"
#include "gtcmtr_protos.h"

GBLREF connection_struct *curr_entry;

error_def(ERR_UNIMPLOP);
error_def(ERR_TEXT);
error_def(ERR_GVIS);

bool gtcmtr_initreg(void)
{
	cm_region_head	*region;
	cm_region_list	*list_entry;
	unsigned char	*reply;
	unsigned short temp_short;
	unsigned char	buff[MAX_ZWR_KEY_SZ], *end;

	assert(*curr_entry->clb_ptr->mbf == CMMS_S_INITREG);
	region = gtcmd_ini_reg(curr_entry);
	gtcm_add_region(curr_entry,region);

	if (region->reg->max_rec_size > 32767)
	{
		rts_error(VARLSTCNT(10) ERR_UNIMPLOP, 0,
		ERR_TEXT, 2,
		LEN_AND_LIT("Client does not support spanning nodes"),
		ERR_TEXT, 2, DB_LEN_STR(region->reg));
	 }

	if (region->reg->max_rec_size + CM_BUFFER_OVERHEAD > curr_entry->clb_ptr->mbl)
	{
		free(curr_entry->clb_ptr->mbf);
		curr_entry->clb_ptr->mbf = (unsigned char *)malloc(region->reg->max_rec_size + CM_BUFFER_OVERHEAD);
		curr_entry->clb_ptr->mbl = region->reg->max_rec_size + CM_BUFFER_OVERHEAD;
	}
	if (0 == curr_entry->cli_supp_allowexisting_stdnullcoll)
	{
		if (ALLOWEXISTING == region->reg->null_subs)
		{
			rts_error(VARLSTCNT(10) ERR_UNIMPLOP, 0,
				ERR_TEXT, 2,
				LEN_AND_LIT("Client does not support ALLOWEXISTING for null subscripts"),
				ERR_TEXT, 2, DB_LEN_STR(region->reg));
		}
		if ( 0 != region->reg->std_null_coll)
		{
			rts_error(VARLSTCNT(10) ERR_UNIMPLOP, 0,
				ERR_TEXT, 2,
				LEN_AND_LIT("Client does not support standard null collation for null subscripts"),
				ERR_TEXT, 2, DB_LEN_STR(region->reg));
		}
	}

	reply = curr_entry->clb_ptr->mbf;
	*reply++ = CMMS_T_REGNUM;
	*reply++ = curr_entry->current_region->regnum;
	*reply++ = region->reg->null_subs;
	temp_short = (unsigned short)region->reg->max_rec_size;
	assert((int4)temp_short == region->reg->max_rec_size); /* ushort <- int4 assignment lossy? */
	PUT_USHORT(reply, temp_short);
	reply += SIZEOF(unsigned short);
	temp_short = (unsigned short)region->reg->max_key_size;
	assert((int4)temp_short == region->reg->max_key_size); /* ushort <- int4 assignment lossy? */
	PUT_USHORT(reply, temp_short);
	reply += SIZEOF(unsigned short);
	if (curr_entry->cli_supp_allowexisting_stdnullcoll)
		*reply++ = region->reg->std_null_coll;
	curr_entry->clb_ptr->cbl = reply - curr_entry->clb_ptr->mbf;
	return TRUE;
}
