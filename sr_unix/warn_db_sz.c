/****************************************************************
 *								*
 * Copyright (c) 2018-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"
#include "gdsroot.h"
#include "gtmmsg.h"
#include "send_msg.h"
#include "warn_db_sz.h"
#include "wbox_test_init.h"
#include "gtm_string.h"
#include "gtmimagename.h"

#define SPCWARNTHRESHOLD 88

error_def(ERR_LOWSPC);

void warn_db_sz(char *db_fname, block_id prev_blocks, block_id curr_blocks, block_id tot_blocks)
{
	double new_sz_frac;
	double old_sz_frac;
	int diff;

	new_sz_frac = (WBTEST_ENABLED(WBTEST_DB_BLOCKS_WARN)) ?
			SPCWARNTHRESHOLD : (((double)curr_blocks) / ((double)tot_blocks)) * 100;
	if (new_sz_frac < SPCWARNTHRESHOLD)
		return;
	old_sz_frac = (((double)prev_blocks) / ((double)tot_blocks)) * 100;
	/*To check if we've crossed a 1% boundary*/
	diff = ((int)new_sz_frac) - ((int)old_sz_frac);
	if (diff >= 1)
	{
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_LOWSPC, 5, STRLEN(db_fname), db_fname, 100 - ((int)new_sz_frac),
			&curr_blocks, &tot_blocks);
		if (IS_MUPIP_IMAGE)
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_LOWSPC, 5, STRLEN(db_fname), db_fname,
				100 - ((int)new_sz_frac), &curr_blocks, &tot_blocks);
		}
	}
}
