/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "compiler.h"
#include "stringpool.h"
#include "io_params.h"
#include "io.h"
#include "iottdef.h"
#include "cvtparm.h"
#include "mvalconv.h"
#include "cvttime.h"
#include "cvtprot.h"

GBLREF spdesc stringpool;
LITREF unsigned char io_params_size[];

#define IOP_DESC(a,b,c,d,e) {d, e}
LITDEF dev_ctl_struct dev_param_control[] =
{
#include "iop.h"
};
#undef IOP_DESC

int4 cvtparm(int iocode, mval *src, mval *dst)
{
	error_def(ERR_DEVPARTOOBIG);
	error_def(ERR_DEVPARPROT);

	int4 		status, nl,  tim[2];
	int		siz, cnt, strlen;
	short		ns;
	unsigned char 	*cp, msk;
	io_termmask 	lngmsk;

	assert(MV_DEFINED(src));
	strlen = -1;
	siz = io_params_size[iocode];
	ENSURE_STP_FREE_SPACE(siz + 1);
	switch(dev_param_control[iocode].source_type)
	{
		case IOP_SRC_INT:
			assert(siz == SIZEOF(int4) || siz == SIZEOF(short));
			MV_FORCE_NUM(src);
			nl = MV_FORCE_INT(src);
			if (siz == SIZEOF(int4))
				cp = (unsigned char *)&nl;
			else
			{
				assert (siz == SIZEOF(short));

				ns = (short) nl;
				cp = (unsigned char *) &ns;
			}
			break;
		case IOP_SRC_STR:
			assert(siz == IOP_VAR_SIZE);
			MV_FORCE_STR(src);
			if (src->str.len > 255)	/*one byte string lengths within a parameter string*/
				return (int4) ERR_DEVPARTOOBIG;
			strlen = src->str.len;
			siz = strlen + SIZEOF(unsigned char);
			cp = (unsigned char *) src->str.addr;
			break;
		case IOP_SRC_MSK:
			MV_FORCE_STR(src);
			assert(siz == SIZEOF(int4));
			nl = 0;
			for (cp = (unsigned char *) src->str.addr, cnt = src->str.len ; cnt > 0 ; cnt--)
				nl |= (1 << *cp++);
			cp = (unsigned char *) &nl;
			break;
		case IOP_SRC_LNGMSK:
			MV_FORCE_STR(src);
			assert(siz == SIZEOF(io_termmask));
			memset(&lngmsk, 0, SIZEOF(io_termmask));
			for (cp = (unsigned char *) src->str.addr, cnt = src->str.len ; cnt > 0 ; cnt--)
			{
				msk = *cp++;
				nl = msk / 32;
				lngmsk.mask[nl] |= (1 << (msk - (nl * 32)));
			}
			cp = (unsigned char *) &lngmsk;
			break;
		case IOP_SRC_PRO:
			assert(siz == SIZEOF(unsigned char));
			MV_FORCE_STR(src);
			nl = cvtprot(src->str.addr, src->str.len);
			if (nl == -1)
				return  (int4) ERR_DEVPARPROT;
			msk = nl;
			cp = &msk;
			break;
		case IOP_SRC_TIME:
			status = cvttime(src, tim);
			if ((status & 1) == 0)
				return status;
			siz = SIZEOF(tim) ;
			cp = (unsigned char *) tim ;
			break;
		default:
			assert(FALSE);
	}
	dst->mvtype = MV_STR;
	dst->str.addr = (char *) stringpool.free;
	dst->str.len = siz;
	if (strlen >= 0)
	{
		*stringpool.free++ = strlen;
		siz -= SIZEOF(char);
	}
	memcpy(stringpool.free, cp, siz);
	stringpool.free += siz;
	return 0;
}
