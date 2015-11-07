/****************************************************************
 *								*
 *	Copyright 2003, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_rename.h"
#include "iosp.h"

#include <ssdef.h>
#include <rms.h>
#include <devdef.h>
#include <descrip.h>
#include <libdtdef.h>
#include <libdef.h>
#include <starlet.h>


uint4 gtm_rename(char *org_fn, int org_fn_len, char *rename_fn, int rename_len, uint4 *ustatus)
{
	char		es1_buffer[MAX_FN_LEN], name1_buffer[MAX_FN_LEN];
	char		es2_buffer[MAX_FN_LEN], name2_buffer[MAX_FN_LEN];
	struct FAB	fab1, fab2;
	struct NAM	nam1, nam2;
	uint4		status;

	*ustatus = SS_NORMAL;
	nam1 = cc$rms_nam;
	nam1.nam$l_rsa = name1_buffer;
	nam1.nam$b_rss = SIZEOF(name1_buffer);
	nam1.nam$l_esa = es1_buffer;
	nam1.nam$b_ess = SIZEOF(es1_buffer);
	fab1 = cc$rms_fab;
	fab1.fab$l_nam = &nam1;
	fab1.fab$b_shr = FAB$M_SHRPUT | FAB$M_SHRGET | FAB$M_SHRDEL | FAB$M_SHRUPD;
	fab1.fab$l_fna = org_fn;
	fab1.fab$b_fns = org_fn_len;

	nam2 = cc$rms_nam;
	nam2.nam$l_rsa = name2_buffer;
	nam2.nam$b_rss = SIZEOF(name2_buffer);
	nam2.nam$l_esa = es2_buffer;
	nam2.nam$b_ess = SIZEOF(es2_buffer);
	fab2 = cc$rms_fab;
	fab2.fab$l_nam = &nam2;
	fab2.fab$b_shr = FAB$M_SHRPUT | FAB$M_SHRGET | FAB$M_SHRDEL | FAB$M_SHRUPD;
	fab2.fab$l_fna = rename_fn;
	fab2.fab$b_fns = rename_len;
	status = sys$rename(&fab1, 0, 0, &fab2); /* Rename the file */
	if (!(status & 1))
	{
		*ustatus = fab1.fab$l_stv;
		return status;
	}
	*ustatus = status;
	return SS_NORMAL;
}
