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
 * iorm_put is a wrapper around the RMS sys$put routine to	*
 * implement the GT.M BIGRECORD file format.			*
 *								*
 * Argument:							*
 *		iod	io_desc structure			*
 *								*
 * Return:	RMS$_NORMAL or RMS error status			*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <rms.h>
#include <devdef.h>
#include "gtm_string.h"
#include "io.h"
#include "iormdef.h"

int iorm_put(io_desc *iod)
{
	int4 stat;
	unsigned int reclen;
	d_rm_struct  *rm_ptr;

	rm_ptr = iod->dev_sp;
	if (rm_ptr->f.fab$b_fac == FAB$M_GET)
		return RMS$_FAC;
	assert(rm_ptr->outbuf == rm_ptr->r.rab$l_rbf);
	if (!rm_ptr->largerecord)
	{
		rm_ptr->r.rab$w_rsz = rm_ptr->l_rsz;
		stat = sys$put(&rm_ptr->r);
	} else
	{
		if (FAB$C_VAR == rm_ptr->b_rfm)
		{
			rm_ptr->r.rab$l_rbf -= SIZEOF(uint4);	/* recordsize */
			assert(rm_ptr->r.rab$l_rbf >= rm_ptr->outbuf_start);
			*(uint4 *)rm_ptr->r.rab$l_rbf = rm_ptr->l_rsz;
			rm_ptr->l_rsz += SIZEOF(uint4);
		}
		reclen = ROUND_UP2(rm_ptr->l_rsz, SIZEOF(uint4));	/* fix and var records on longword boundary */
		if (reclen > rm_ptr->l_rsz)
		{
			assert((rm_ptr->r.rab$l_rbf + reclen) <= ROUND_UP((unsigned long)rm_ptr->outbuf_top, SIZEOF(uint4)));
			memset(&rm_ptr->r.rab$l_rbf[rm_ptr->l_rsz], 0, reclen - rm_ptr->l_rsz); /* null pad */
		}
		do
		{
			rm_ptr->r.rab$w_rsz = (unsigned short)(MAX_RMS_UDF_RECORD < reclen
				? MAX_RMS_UDF_RECORD : reclen);
			stat = sys$put(&rm_ptr->r);
			if (RMS$_NORMAL != stat)
				return stat;
			rm_ptr->r.rab$l_rbf += rm_ptr->r.rab$w_rsz;
			reclen -= (unsigned int)rm_ptr->r.rab$w_rsz;
		} while (0 < reclen);
	}
	return stat;
}
