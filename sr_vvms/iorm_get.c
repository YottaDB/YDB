/****************************************************************
 *								*
 *	Copyright 2004, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/****************************************************************
 *								*
 * iorm_get is a wrapper around the RMS sys$get routine to	*
 * implement the GT.M BIGRECORD file format.			*
 *								*
 * Arguments:							*
 *		iod		io_desc structure		*
 *		timeout		timeout value from M OPEN	*
 *								*
 * Return:	RMS$_NORMAL or RMS error status			*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <rms.h>

#include "io.h"
#include "iormdef.h"
#include "iotimer.h"

int iorm_get(io_desc *iod, int4 timeout)
{
	d_rm_struct     *rm_ptr;
	struct RAB	*rab;
	int4		stat;
	int		len, reclen, toread, expected_reclen, extra_bytes;
	unsigned long	save_ubf;

	assert(0 <= timeout);
	rm_ptr = (d_rm_struct *)iod->dev_sp;
	rab = &rm_ptr->r;
	reclen = 0;	/* set to bytes read total */
	save_ubf = rab->rab$l_ubf;
	if (rm_ptr->largerecord)
	{
		if (FAB$C_FIX == rm_ptr->b_rfm)
		{
			if (iod->width < rm_ptr->l_mrs)
			{
				rab->rab$l_stv = rm_ptr->l_mrs;
				return RMS$_RTB;
			}
			expected_reclen = toread = rm_ptr->l_mrs;
		} else if (FAB$C_VAR == rm_ptr->b_rfm)
		{
			rab->rab$w_usz = SIZEOF(uint4);
			rab->rab$l_ubf = &toread;
			toread = 0;
			rab->rab$b_tmo = (timeout <= 255) ? timeout : 255;
			stat = sys$get(rab);
			rab->rab$l_ubf = save_ubf;
			if (RMS$_NORMAL != stat)
				return stat;
			expected_reclen = toread;
		} else
			GTMASSERT;
		toread = ROUND_UP2(toread, SIZEOF(uint4));
		extra_bytes = toread - expected_reclen;
		rab->rab$w_usz = (unsigned short)(MAX_RMS_UDF_RECORD < toread ? MAX_RMS_UDF_RECORD : toread);
	} else
	{
		rab->rab$w_usz = toread = (unsigned short)rm_ptr->l_usz;
		extra_bytes = 0;
	}
	do
	{
		do
		{
			rab->rab$b_tmo = (timeout <= 255) ? timeout : 255;
			stat = sys$get(rab);
			if (RMS$_TMO == stat && NO_M_TIMEOUT != timeout)
				(timeout > 255) ? (timeout -= 255) : (timeout = 0);
		} while ((RMS$_TMO == stat) && (timeout > 0));
		reclen += (unsigned int)rab->rab$w_rsz;
		if (!rm_ptr->largerecord)
			break;
		toread -= (unsigned int)rab->rab$w_rsz;
		rab->rab$l_ubf += (unsigned int)rab->rab$w_rsz;
		rab->rab$w_usz = (unsigned short)(MAX_RMS_UDF_RECORD < toread ? MAX_RMS_UDF_RECORD : toread);
	} while (RMS$_NORMAL == stat && 0 < toread);
	rab->rab$l_ubf = save_ubf;
	switch (stat)
	{
	case RMS$_NORMAL:
		assert(!rm_ptr->largerecord || (0 == toread && reclen - expected_reclen == extra_bytes));
		rm_ptr->l_rsz = reclen - extra_bytes;
		break;
	case RMS$_EOF:
		rm_ptr->l_rsz = reclen;
		break;
	}
	return stat;
}
