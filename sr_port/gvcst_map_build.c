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
#include "gdsblk.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "gdsbml.h"
#include "gvcst_map_build.h"

GBLREF bool  run_time;

void gvcst_map_build(uint4 *array,
		     sm_uc_ptr_t base_addr,
		     cw_set_element *cs,
		     trans_num ctn)
{
	boolean_t	busy;
	uint4		ret, (*bml_func)();

	((blk_hdr_ptr_t)base_addr)->tn = ctn;
	base_addr += sizeof(blk_hdr);

	bml_func = (busy = (cs->reference_cnt > 0)) ? bml_busy : bml_free;

	while(*array)
	{
		if (0 == (ret = (* bml_func)((*array - cs->blk), base_addr)))
			cs->reference_cnt--;
		assert((FALSE == run_time) || ((ret != 0) == (busy != 0)));
		array++;
	}
}
