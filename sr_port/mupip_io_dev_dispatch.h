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

/* io_dev_dispatch.h is a superset of this file which includes everything needed by MUPIP or GT.M
 * this module is interested in iorm_* only here cause these are all MUPIP needs
 * so, if you need to make a change here, please keep the other one in sync.
 */
                                                                        
#include "mdef.h"

LITDEF dev_dispatch_struct io_dev_dispatch_mupip[]=
{
#ifdef UNIX
	{
		iott_open, iott_close, iott_use, iott_read, iott_rdone,
		iott_write, iott_wtone, iott_wteol, iott_wtff, iott_wttab,
		iott_flush, iott_readfl, nil_iocontrol, nil_dlr_device,
		nil_dlr_key
	},
#else
	{
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	},
#endif
	{
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	},
	{
	iorm_open	,iorm_close	,iorm_use	,iorm_read	,iorm_rdone
	,iorm_write	,iorm_wtone	,iorm_wteol	,iorm_wtff	,iorm_wttab
	,iorm_flush	,iorm_readfl	,nil_iocontrol	,nil_dlr_device	,nil_dlr_key
	},
	{
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	},
	{
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	},
	{
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	},
	{
	ioff_open	,iorm_close	,iorm_use	,iorm_read	,iorm_rdone
	,iorm_write	,iorm_wtone	,iorm_wteol	,iorm_wtff	,iorm_wttab
	,iorm_flush	,iorm_readfl	,nil_iocontrol	,nil_dlr_device	,nil_dlr_key
	},
	{
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	},
	{
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	},
};
