/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2022 YottaDB LLC and/or its subsidiaries.	*
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
#include <stdarg.h>

#include "error.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "mlkdef.h"
#include "mlk_ops.h"
#include "mlk_region_lookup.h"
#include "mlk_pvtblk_insert.h"
#include "mlk_pvtblk_create.h"
#include "mmrhash.h"
#include "dpgbldir.h"
#include "gtm_reservedDB.h"
/* The header files below are for environment translation*/
#include "lv_val.h"	/* needed for "fgncalsp.h" */
#include "fgncalsp.h"
#include "gtm_env_xlate_init.h"
#include "mvalconv.h"

GBLDEF mlk_subhash_val_t	mlk_last_hash;

GBLREF gd_addr		*gd_header;
GBLREF mval		dollar_ztslate;

error_def(ERR_LOCKSUB2LONG);
error_def(ERR_PCTYRESERVED);
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
	unsigned char	*cp, *cp_prev;
	mval		*extgbl2, *mp_temp, val_xlated;
	mlk_pvtblk	*r;
	gd_region	*reg;
	gd_addr		*gld;
	mlk_subhash_state_t	accstate, tmpstate;
	mlk_subhash_res_t	hashres;
	boolean_t		do_hash;

	/* Get count of mvals including extgbl1 */
	assert(2 <= subcnt);
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
			gld = zgbldir_opt(extgbl1, TRUE, FALSE);
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
	/* If specified var name is global ^%Y*, the name is illegal to use in a LOCK command.
	 * Ths first byte is '^' so skip it in the comparison.
	 */
	if ((RESERVED_NAMESPACE_LEN <= (mp_temp->str.len - 1)) && (0 == MEMCMP_LIT(mp_temp->str.addr + 1, RESERVED_NAMESPACE)))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_PCTYRESERVED);
	reg = mlk_region_lookup((mp_temp), gld);
	/* Add up the sizes of all MVAL strings */
	for (len = 0, i = 0;  i < subcnt;  mp_temp=va_arg(mp, mval *), i++)
	{
		if (MV_IS_SQLNULL(mp_temp))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZYSQLNULLNOTVALID);
		MV_FORCE_STR(mp_temp);
		if (MAX_LK_SUB_LEN < (mp_temp)->str.len)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_LOCKSUB2LONG, 1, (mp_temp)->str.len,
				      ERR_TEXT, 2, (mp_temp)->str.len, (mp_temp)->str.addr);
		assert((mp_temp)->mvtype & MV_STR);
		len += (int)(mp_temp)->str.len;
	}
	va_end(mp);
	/*
	 * Allocate a buffer for all mval strings.
	 * All strings are stored one after another in the buffer.
	 * Each string is preceeded by 1 byte string len.
	 */
	MLK_PVTBLK_ALLOC(len + subcnt, subcnt, 0, r);
	MLK_PVTCTL_INIT(r->pvtctl, reg);
	/* We can't do proper hashes for remote locks, so just skip hashing in the remote case. */
	do_hash = (NULL != r->pvtctl.ctl);
	r->translev = 1;
	r->subscript_cnt = subcnt;
	/* Each string is preceeded by string length byte */
	r->nref_length = len + subcnt;
	/* Keep the hash code generation here in sync with MLK_PVTBLK_SUBHASH_GEN() */
	if (do_hash)
	{
		MLK_SUBHASH_INIT(r, accstate);
		hashres = (mlk_subhash_res_t){0, 0};
	}
	cp = &r->value[0];
	/* Copy all strings into the buffer one after another */
	for (i = 0, VAR_COPY(mp, subptr);  i < subcnt;  i++)
	{
		cp_prev = cp;
		mp_temp = va_arg(mp, mval *);
		MV_FORCE_STR(mp_temp);
		len = (int)(mp_temp)->str.len;
		*cp++ = len;
		memcpy(cp, (mp_temp)->str.addr, len);
		cp += len;
		if (do_hash)
		{
			MLK_SUBHASH_INGEST(accstate, cp_prev, len + 1);
			tmpstate = accstate;
			MLK_SUBHASH_FINALIZE(tmpstate, (cp - r->value), hashres);
			DBG_LOCKHASH_N_BITS(hashres.one);
			MLK_PVTBLK_SUBHASH(r, i) = MLK_SUBHASH_RES_VAL(hashres);
		}
	}
	if (do_hash)
		mlk_last_hash = MLK_SUBHASH_RES_VAL(hashres);
	va_end(mp);
	if (!mlk_pvtblk_insert(r))
		free(r);
	return;
}
