/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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

int sec_shr_map_build(sgmnt_addrs *csa, uint4 *array, unsigned char *base_addr, cw_set_element *cs, trans_num ctn, int bplmap)
{
	boolean_t		busy, recycled;
	uint4			setbit;
	unsigned char		*ptr;
	sgmnt_data_ptr_t	csd;

	((blk_hdr *)base_addr)->tn = ctn;
	base_addr += sizeof(blk_hdr);
	if (!GTM_PROBE(bplmap / 8, base_addr, WRITE))
	{
		assert(FALSE);
		return FALSE;
	}
	busy = cs->reference_cnt > 0;
	if (!busy)
	{
		if (!GTM_PROBE(sizeof(sgmnt_addrs), csa, READ))
		{
			assert(FALSE);
			return FALSE;
		}
		csd = csa->hdr;
		if (!GTM_PROBE(sizeof(sgmnt_data), csd, READ))
		{
			assert(FALSE);
			return FALSE;
		}
		recycled = csd->db_got_to_v5_once ? TRUE : FALSE;
	}
	for (;;)
	{
		if (!GTM_PROBE(sizeof(*array), array, READ))
		{
			assert(FALSE);
			return FALSE;
		}
		if (*array == 0)
			return TRUE;
		setbit = (*array - cs->blk) * BML_BITS_PER_BLK;
		ptr = base_addr + setbit / 8;
		if (!GTM_PROBE(sizeof(*ptr), ptr, WRITE))
		{
			assert(FALSE);
			return FALSE;
		}
		setbit &= 7;
		if (busy)
			*ptr &= ~(3 << setbit);	/* mark block as BUSY (00) */
		else
		{
			if (recycled)
				*ptr |= (3 << setbit);	/* mark block as RECYCLED (11) */
			else
			{	/* mark block as FREE (01) */
				*ptr &= ~(3 << setbit);	/* first mark block as BUSY (00) */
				*ptr |= (1 << setbit);	/* then  mark block as FREE (01) */
			}
			cs->reference_cnt--;
		}
		++array;
	}
}
