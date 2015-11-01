/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#ifdef VMS
#include <descrip.h>	/* for GTM_ENV_TRANSLATE */
#endif

#ifdef EARLY_VARARGS
#include <varargs.h>
#endif
#include "gtm_limits.h"	/*for GTM_ENV_TRANSLATE */

#include "error.h"
#ifndef EARLY_VARARGS
#include <varargs.h>
#endif
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "mlkdef.h"
#include "mlk_region_lookup.h"
#include "mlk_pvtblk_insert.h"
#include "mlk_pvtblk_create.h"
#include "dpgbldir.h"

/*the header files below are for environment translation*/
#ifdef UNIX
#include "fgncalsp.h"
#endif
#include "gtm_env_xlate_init.h"

/*
 * ---------------------------------------------------------
 * Create a private block structure for one nref,
 * given by the subptr.
 *
 * Arguments:
 *	subptr	- address of 1st element of nref
 *
 * NOTE:
 *	See the description of nref in Technical Specification
 *	for Mumps Lock by R. Shear
 *	In short, each nref consists of:
 *		1. longword = number of MVALs in this nref.
 *		2. Address of external global.
 *		3-n. Variable length list of pointers to mvals in this nref.
 * ---------------------------------------------------------
 */
GBLREF gd_addr		*gd_header;
static mstr     	gtmgbldir_mstr;

void	mlk_pvtblk_create (va_list subptr)
{
	va_list		mp;
	int		i, len;
	int4		rlen;			/* Roundup each length to get clear len for mlk_shrsub */
	unsigned char	*cp;
	mval		*extgbl1, *extgbl2, *mp_temp, val_xlated;
	int		subcnt;
	mlk_pvtblk	*r;
	gd_region	*reg;
	sgmnt_addrs	*sa;
	gd_addr		*gld;


	/* Get count of mvals */
	subcnt = va_arg(subptr, int);
	assert (subcnt >= 2);

	extgbl1 = va_arg(subptr, mval *);
	subcnt--;
	if (extgbl1)
	{
		MV_FORCE_STR(extgbl1);
		extgbl2 = va_arg(subptr, mval *);
		subcnt--;
		GTM_ENV_TRANSLATE(extgbl1, extgbl2);
		if (extgbl1->str.len)
			gld = zgbldir(extgbl1);
		else
		{
			assert(gd_header);
			gld = gd_header;
		}
	} else
		gld = gd_header;

	VAR_COPY(mp, subptr);
	mp_temp = va_arg(mp, mval *);
	MV_FORCE_STR(mp_temp);
	reg = mlk_region_lookup((mp_temp), gld);

		/* Add up the sizes of all MVAL strings */
	for (len = 0, rlen=0, i = 0;  i < subcnt;  mp_temp=va_arg(mp, mval *), i++)
	{
		MV_FORCE_STR(mp_temp);
		assert((mp_temp)->mvtype & MV_STR);
		assert((mp_temp)->str.len < 256);
		len += (mp_temp)->str.len;
		rlen += ROUND_UP(((mp_temp)->str.len + 1), 4);
	}

/*
 * Allocate a buffer for all mval strings.
 * All strings are stored one after another in the buffer.
 * Each string is preceeded by 1 byte string len.
 */
	r = (mlk_pvtblk *) malloc(MLK_PVTBLK_SIZE(len, subcnt));
	memset(r, 0, sizeof(mlk_pvtblk) - 1);
	r->translev = 1;
	r->subscript_cnt = subcnt;
		/* Each string is preceeded by string length byte */
	r->total_length = len + subcnt;
	r->total_len_padded = rlen;		/* len byte already accounted for */
	cp = &r->value[0];

		/* Copy all strings into the buffer one after another */
	for (i = 0, VAR_COPY(mp, subptr);  i < subcnt;  i++)
	{
		mp_temp = va_arg( mp, mval *);
		MV_FORCE_STR(mp_temp);
		len = (mp_temp)->str.len;
		*cp++ = len;
		memcpy(cp, (mp_temp)->str.addr, len);
		cp += len;
	}
	r->region = reg;
	sa = &FILE_INFO(r->region)->s_addrs;
	r->ctlptr = (mlk_ctldata_ptr_t)sa->lock_addrs[0];

	if (!mlk_pvtblk_insert(r))
		free(r);

	return;
}
