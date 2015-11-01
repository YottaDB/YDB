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

#include "lockconst.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "copy.h"
#include "bit_op.h"
#include "wcs_backoff.h"
#include "bit_opi.h"
#include "caller_id.h"

GBLREF int4 process_id;
GBLREF gd_region *gv_cur_region;

uint4 bit_opi(bit_op_t bit_op, uint4 bit, uint4 *base, sm_int_ptr_t latch)
{
	/*
	 * bit_op is either BIT_SET or BIT_CLEAR
	 * BIT_SET   - Set the bit and return non-zero if bit was already set,
	 *             zero otherwise
	 * BIT_CLEAR - Clear the bit and return non-zero if bit was already
	 *             clear, zero otherwise
	 */
	int4	retval, aswp();
	int	i;
	uint4	*ptr, bit_shift;

	error_def(ERR_DBCCERR);
	error_def(ERR_ERRCALL);

	for (i = 0; i < 2000; i++)
	{
		switch(aswp(latch, LOCK_IN_USE))
		{
		    	case LOCK_AVAILABLE:
				LOCK_HIST("OBTN", latch, process_id, i);
				ptr = base + bit / (sizeof(uint4) * 8);
				bit_shift = bit & 31;
				retval = (1 << bit_shift) & *ptr;
				switch(bit_op)
				{
		  	    		case BIT_SET:
						*ptr |= 1 << bit_shift;
						retval = (retval != 0);
						break;
			    		case BIT_CLEAR:
						*ptr &= ~(1 << bit_shift);
						retval = (retval == 0);
						break;
				}
				LOCK_HIST("RLSE", latch, process_id, i);
				aswp(latch, LOCK_AVAILABLE);
				return(retval);

			case LOCK_IN_USE:
				break;

			default:
				DUMP_LOCKHIST();
				assert(FALSE);
				rts_error(VARLSTCNT(9) ERR_DBCCERR, 2, LEN_AND_LIT("*unknown*"), ERR_ERRCALL, 3, CALLFROM);
		}
		if (0 != i)
			wcs_backoff(i);
	}
	DUMP_LOCKHIST();
	assert(FALSE);
	rts_error(VARLSTCNT(9) ERR_DBCCERR, 2, LEN_AND_LIT("*unknown*"), ERR_ERRCALL, 3, CALLFROM);
}
