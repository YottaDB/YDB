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
#include "probe.h"
#include "sec_shr_map_build.h"

int sec_shr_map_build(uint4 *array, unsigned char *base_addr, cw_set_element *cs, trans_num ctn, int bplmap)
{
	bool		busy;
	uint4		setbit;
	unsigned char	*ptr;

	((blk_hdr *)base_addr)->tn = ctn;
	base_addr += sizeof(blk_hdr);
	if (!GTM_PROBE(bplmap / 8, base_addr, WRITE))
		return FALSE;
	busy = cs->reference_cnt > 0;
	for (;;)
	{
		if (!GTM_PROBE(sizeof(*array), array, READ))
			return FALSE;
		if (*array == 0)
			return TRUE;
		setbit = (*array - cs->blk) * BML_BITS_PER_BLK;
		ptr = base_addr + setbit / 8;
		if (!GTM_PROBE(sizeof(*ptr), ptr, WRITE))
			return FALSE;
		setbit &= 7;
		if (busy)
			*ptr &= ~(3 << setbit);
		else
			if ((*ptr & (1 << setbit)) == 0)
			{
				*ptr |= 3 << setbit;
				cs->reference_cnt--;
			}
			else
				*ptr |= 1 << setbit;
		++array;
	}
}
