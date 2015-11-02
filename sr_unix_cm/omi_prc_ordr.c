/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 *  omi_prc_ordr.c ---
 *
 *	Process a ORDER request.
 *
 */

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

#include "mdef.h"

#include "gtm_string.h"

#include "omi.h"
#include "error.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "stringpool.h"
#include "op.h"
#include "gvcst_protos.h"	/* for gvcst_root_search in GV_BIND_NAME_AND_ROOT_SEARCH macro */

GBLREF gv_namehead	*gv_target;
GBLREF gv_key		*gv_currkey;
GBLREF gv_key		*gv_altkey;
GBLREF bool		undef_inhibit;

int omi_prc_ordr(omi_conn *cptr, char *xend, char *buff, char *bend)
{
	char		*bptr;
	int			 rv;
	omi_li		 len;
	mval		 vo, vd, vg;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	bptr = buff;
	/* Global Ref */
	OMI_LI_READ(&len, cptr->xptr);
	/* Condition handler for DBMS operations */
	ESTABLISH_RET(omi_dbms_ch,0);
	rv = omi_gvextnam(cptr, len.value, cptr->xptr);
	/* If true, there was an error finding the global reference in the DBMS */
	if (rv < 0)
	{
		REVERT;
		return rv;
	}
	cptr->xptr += len.value;

	/* Bounds checking */
	if (cptr->xptr > xend)
	{
		REVERT;
		return -OMI_ER_PR_INVMSGFMT;
	}

	/* We want to make sure there is plenty of space in the string pool for all three operations ($ORDER, $GET, $DATA) */
	if (cptr->exts & OMI_XTF_NEWOP)
		INVOKE_STP_GCOL(0);

	/* $ORDER */
	op_gvorder(&vo);
	/* $ORDER (buffer write) */
	OMI_SI_WRIT(vo.str.len, bptr);
	if (vo.str.len)
	{
		memcpy(bptr, vo.str.addr, vo.str.len);
		bptr += vo.str.len;
	}
	/* Bunching */
	if (cptr->exts & OMI_XTF_NEWOP)
	{
		if (vo.str.len)
		{
			if (!gv_currkey->prev)
			{
				if (*vo.str.addr != '^')
				{
					REVERT;
					return -OMI_ER_PR_INVGLOBREF;
				}
				vo.str.addr++;	vo.str.len--;
				GV_BIND_NAME_AND_ROOT_SEARCH(cptr->ga, &vo.str);
				vo.str.addr--;	vo.str.len++;
				TREF(gv_last_subsc_null) = FALSE;
			} else
			{
				if (gv_currkey->top != gv_altkey->top)
				{
					REVERT
					return -OMI_ER_DB_UNRECOVER;
				}
				memcpy(gv_currkey, gv_altkey, gv_altkey->end + SIZEOF(gv_key));
				TREF(gv_last_subsc_null) = FALSE;
			}

			/* $DATA */
			op_gvdata(&vd);
			if (!(vd.mvtype & MV_INT))
			{
				REVERT;
				return -OMI_ER_DB_UNRECOVER;
			}
			/* $GET */
			undef_inhibit = TRUE;
			rv = op_gvget(&vg);
			/* $DATA (buffer write) */
			OMI_SI_WRIT(vd.m[1] / MV_BIAS, bptr);
			/* $GET (buffer write) */
			OMI_SI_WRIT((rv ? 1 : 0), bptr);
			if (!rv || !vg.str.len)
				OMI_LI_WRIT(0, bptr);
			else
			{
				OMI_LI_WRIT(vg.str.len, bptr);
				memcpy(bptr, vg.str.addr, vg.str.len);
				bptr += vg.str.len;
			}
		} else
		{	/* Otherwise $ORDER returned a null */
			/* $DATA (buffer write) */
			OMI_SI_WRIT(0, bptr);
			/* $GET (buffer write) */
			OMI_SI_WRIT(0, bptr);
			OMI_LI_WRIT(0, bptr);
		}
	}
	REVERT;
	return (int)(bptr - buff);
}
