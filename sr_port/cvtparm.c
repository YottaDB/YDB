/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "cvtprot.h"

GBLREF spdesc stringpool;
LITREF unsigned char io_params_size[];

error_def(ERR_DEVPARTOOBIG);
error_def(ERR_DEVPARPROT);

#define IOP_DESC(a,b,c,d,e) {d, e}
LITDEF dev_ctl_struct dev_param_control[] =
{
#include "iop.h"
};
#undef IOP_DESC

int4 cvtparm(int iocode, mval *src, mval *dst)
{
	int4 		status, nl,  tim[2];
	int		siz, extra_siz, cnt, strlen;
	short		ns;
	unsigned char 	*cp, msk;
	io_termmask 	lngmsk;

	assert(MV_DEFINED(src));
	strlen = -1;
	siz = io_params_size[iocode];
	extra_siz = ((IOP_VAR_SIZE_4BYTE == siz) ? IOP_VAR_SIZE_4BYTE_LEN : 1);
	/* For all "source_type" cases except IOP_SRC_STR, the value of "siz" is a maximum of the needed length so
	 * we can invoke "ENSURE_STP_FREE_SPACE" macro here. But for IOP_SRC_STR, we don't know the actual string
	 * length (which can be as high as MAX_STRLEN i.e. 1Mb) and therefore we need to invoke the macro again later.
	 */
	ENSURE_STP_FREE_SPACE(siz + extra_siz);
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
			assert((IOP_VAR_SIZE == siz) || (IOP_VAR_SIZE_4BYTE == siz));
			MV_FORCE_STR(src);
			if ((IOP_VAR_SIZE == siz) && (MAX_COMPILETIME_DEVPARLEN < src->str.len))
			{	/* For IOP_VAR_SIZE, we allow a maximum length of MAX_COMPILETIME_DEVPARLEN (i.e. 255)
				 * as we allocate only 1-byte to store the string length. Issue error.
				 */
				assert(255 == MAX_COMPILETIME_DEVPARLEN);
				return (int4) ERR_DEVPARTOOBIG;
			}
			/* Note: For IOP_VAR_SIZE_4BYTE, we allow a maximum length of MAX_RUNTIME_DEVPARLEN (i.e. 1MiB)
			 * the maximum allowed string length in YottaDB as we allocate 4-bytes to store the string length.
			 * We don't do any checks of MAX_RUNTIME_DEVPARLEN here (like we do for MAX_COMPILETIME_DEVPARLEN above)
			 * because "src->str.len" is guaranteed to be less than or equal to MAX_STRLEN.
			 */
			assert(MAX_RUNTIME_DEVPARLEN == MAX_STRLEN);
			strlen = src->str.len;
			siz = strlen + extra_siz;
			/* Now that we know the real string length, allocate enough space in the stringpool to hold it.
			 * See comment before the "switch()" above for why this second invocation of the macro is needed.
			 */
			ENSURE_STP_FREE_SPACE(siz);
			cp = (unsigned char *) src->str.addr;
			break;
		case IOP_SRC_MSK:
			MV_FORCE_STR(src);
			assert(siz == SIZEOF(int4));
			nl = 0;
			for (cp = (unsigned char *) src->str.addr, cnt = src->str.len ; cnt > 0 ; cnt--)
			{
				if (*cp < 32)
					nl |= (1 << *cp++);
			}
			cp = (unsigned char *)&nl;
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
		default:
			assert(FALSE);
			GTM_UNREACHABLE();
	}
	dst->mvtype = MV_STR;
	dst->str.addr = (char *) stringpool.free;
	dst->str.len = siz;
	if (strlen >= 0)
	{
		if (1 == extra_siz)
		{
			assert(256 > strlen);
			*stringpool.free = strlen;
		} else
		{
			assert(IOP_VAR_SIZE_4BYTE_LEN == extra_siz);
			PUT_LONG(stringpool.free, strlen);
		}
		stringpool.free += extra_siz;
		siz -= extra_siz;
	}
	memcpy(stringpool.free, cp, siz);
	stringpool.free += siz;
	return 0;
}
