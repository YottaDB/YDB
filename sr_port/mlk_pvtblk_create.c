/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include <stdarg.h>

#include "error.h"
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
#include "lv_val.h"	/* needed for "fgncalsp.h" */
#include "fgncalsp.h"
#endif
#include "gtm_env_xlate_init.h"

GBLREF gd_addr		*gd_header;
static mstr     	gtmgbldir_mstr;

error_def(ERR_LOCKSUB2LONG);
error_def(ERR_TEXT);

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
void	mlk_pvtblk_create (int subcnt, mval *extgbl1, va_list subptr)
{
	va_list		mp;
	int		i, len;
	int4		rlen;			/* Roundup each length to get clear len for mlk_shrsub */
	unsigned char	*cp;
	mval		*extgbl2, *mp_temp, val_xlated;
	mlk_pvtblk	*r;
	gd_region	*reg;
	sgmnt_addrs	*sa;
	gd_addr		*gld;


	/* Get count of mvals including extgbl1 */
	assert (subcnt >= 2);

	subcnt--;
	/* compiler gives us extgbl1 always, even if the nref is not an extended ref */
	if (NULL == extgbl1)
	{	/* not an extended reference */
		if (!gd_header)
			gvinit();
		gld = gd_header;
	} else
	{
		MV_FORCE_STR(extgbl1);
		extgbl2 = va_arg(subptr, mval *);
		subcnt--;
		extgbl1 = gtm_env_translate(extgbl1, extgbl2, &val_xlated);
		if (extgbl1->str.len)
			gld = zgbldir(extgbl1);
		else
		{
			if (!gd_header)
				gvinit();
			gld = gd_header;
		}
	}

	VAR_COPY(mp, subptr);
	mp_temp = va_arg(mp, mval *);
	MV_FORCE_STR(mp_temp);
	reg = mlk_region_lookup((mp_temp), gld);

	/* Add up the sizes of all MVAL strings */
	for (len = 0, rlen=0, i = 0;  i < subcnt;  mp_temp=va_arg(mp, mval *), i++)
	{
		MV_FORCE_STR(mp_temp);
		if ((mp_temp)->str.len > 255)
			rts_error(VARLSTCNT(7) ERR_LOCKSUB2LONG, 1, (mp_temp)->str.len,
				  ERR_TEXT, 2, (mp_temp)->str.len, (mp_temp)->str.addr);
		assert((mp_temp)->mvtype & MV_STR);
		len += (int)(mp_temp)->str.len;
		rlen += ROUND_UP(((mp_temp)->str.len + 1), 4);
	}
	va_end(mp);

	/*
	 * Allocate a buffer for all mval strings.
	 * All strings are stored one after another in the buffer.
	 * Each string is preceeded by 1 byte string len.
	 */
	r = (mlk_pvtblk *) malloc(MLK_PVTBLK_SIZE(len, subcnt));
	memset(r, 0, SIZEOF(mlk_pvtblk) - 1);
	r->translev = 1;
	r->subscript_cnt = subcnt;
	/* Each string is preceeded by string length byte */
	r->total_length = len + subcnt;
	r->total_len_padded = rlen;		/* len byte already accounted for */
	cp = &r->value[0];

	/* Copy all strings into the buffer one after another */
	for (i = 0, VAR_COPY(mp, subptr);  i < subcnt;  i++)
	{
		mp_temp = va_arg(mp, mval *);
		MV_FORCE_STR(mp_temp);
		len = (int)(mp_temp)->str.len;
		*cp++ = len;
		memcpy(cp, (mp_temp)->str.addr, len);
		cp += len;
	}
	va_end(mp);
	r->region = reg;
	sa = &FILE_INFO(r->region)->s_addrs;
	r->ctlptr = (mlk_ctldata_ptr_t)sa->lock_addrs[0];

	if (!mlk_pvtblk_insert(r))
		free(r);
	return;
}
