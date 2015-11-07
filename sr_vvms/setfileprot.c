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

/* should not be called from runtime routines, otherwise, get rid of the util_out_print/gtm_putmsg */
#include "mdef.h"

#include <rms.h>
#include "util.h"
#include "setfileprot.h"
#include "gtmmsg.h"

boolean_t setfileprot(char *filename, int4 filelen, unsigned short mask)
{
	struct FAB	fab;
	struct XABPRO	xabpro;
	int4		status;

	fab = cc$rms_fab;
	xabpro = cc$rms_xabpro;

	fab.fab$l_fop = FAB$M_MXV | FAB$M_CBT | FAB$M_TEF | FAB$M_CIF;
	fab.fab$b_fac = FAB$M_GET | FAB$M_PUT | FAB$M_BIO | FAB$M_TRN;
	fab.fab$l_xab = &xabpro;
	fab.fab$l_fna = filename;
	fab.fab$b_fns = filelen;

	if (RMS$_NORMAL != (status = sys$open(&fab)))
	{
		gtm_putmsg(VARLSTCNT(1) status);
		util_out_print("Error openning !AD", TRUE, filelen, filename);
		return FALSE;
	}

	xabpro.xab$w_pro = mask;

	if (RMS$_NORMAL != (status = sys$close(&fab)))
	{
                gtm_putmsg(VARLSTCNT(1) status);
                util_out_print("Error closing !AD", TRUE, filelen, filename);
                return FALSE;
        }

	return TRUE;
}
