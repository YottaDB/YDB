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

/* mupip_io_dev_dispatch.h is a subset of this file which includes those needed only by MUPIP
 * so, if you need to make a change here, please keep the other one in sync.
 */

#include "mdef.h"

#define	iotype(X,Y) 								\
{ 										\
	X##_open, X##_close, X##_use, X##_read, X##_rdone, 			\
	X##_write, X##_wtone, X##_wteol, X##_wtff, X##_wttab,			\
	X##_flush, X##_readfl, Y##_iocontrol, Y##_dlr_device, Y##_dlr_key 	\
}

LITDEF dev_dispatch_struct io_dev_dispatch[]=
{
	iotype(iott, nil),
	iotype(iomt, nil),
	iotype(iorm, nil),
	iotype(ious, ious),
	iotype(iomb, nil),
	{
	ionl_open	,
	ionl_close	,
	ionl_use	,
	ionl_read	,
	(short(*)(mint *,int4))ionl_read	,
	ionl_write	,
	ionl_wtone	,
	ionl_wteol	,
	ionl_wtff	,
	ionl_wttab	,
	ionl_null	,
	(short(*)(mval*,int4,int4))ionl_read	,
	nil_iocontrol	,
	nil_dlr_device	,
	nil_dlr_key
	},
	{
	ioff_open	,
	iorm_close	,
	iorm_use	,
	iorm_read	,
	iorm_rdone	,
	iorm_write	,
	iorm_wtone	,
	iorm_wteol	,
	iorm_wtff	,
	iorm_wttab	,
	iorm_flush	,
	iorm_readfl	,
	nil_iocontrol	,
	nil_dlr_device	,
	nil_dlr_key
	},
	iotype(iotcp, iotcp),
	iotype(iosocket, iosocket)
};
