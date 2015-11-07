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
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include <syidef.h>
#include <rms.h>

bool reg_cmcheck(gd_region *reg)
{
	struct FAB		fcb;
	struct NAM		nam;
	int4			status, iosb[2];
	short			i, len;
	char			node[16], nambuf[MAX_FN_LEN];
	struct {short blen; short code; char *buf; short *len;} itm[2] = {{15, SYI$_NODENAME, &node, &len},{0, 0, 0, 0}};
	error_def(ERR_DBOPNERR);

	nam = cc$rms_nam;
	nam.nam$l_esa = nambuf;
	nam.nam$b_ess = SIZEOF(nambuf);
	nam.nam$b_nop |= NAM$M_SYNCHK;
	fcb = cc$rms_fab;
	fcb.fab$l_fna = reg->dyn.addr->fname;
	fcb.fab$b_fns = reg->dyn.addr->fname_len;
	fcb.fab$l_dna = reg->dyn.addr->defext;
	fcb.fab$b_dns = SIZEOF(reg->dyn.addr->defext);
	fcb.fab$l_nam = &nam;
	fcb.fab$w_deq = 0;
	if ((status = sys$parse(&fcb, 0, 0)) != RMS$_NORMAL)
		rts_error(VARLSTCNT(5) ERR_DBOPNERR, 2, fcb.fab$b_fns, fcb.fab$l_fna, status);
	i = nam.nam$b_node ;
	if (0 != i)
	{
		sys$getsyi(0, 0, 0, itm, iosb, 0, 0);
		return ((i - 2) != len || 0 != memcmp(nam.nam$l_esa, node, len));
	}
	return FALSE;
}
