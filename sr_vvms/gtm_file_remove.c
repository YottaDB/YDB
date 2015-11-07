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

#include "gtm_string.h"
#include "gtm_fcntl.h"

#include <ssdef.h>
#include <rms.h>
#include <devdef.h>
#include <descrip.h>
#include <libdtdef.h>
#include <libdef.h>
#include <starlet.h>

#include "gtm_file_remove.h"
#include "iosp.h"

int4 gtm_file_remove(char *fn, int fn_len, uint4 *ustatus)
{
	unsigned char	es_buffer[MAX_FN_LEN], name_buffer[MAX_FN_LEN];
	struct FAB 	fab;
	struct NAM 	nam;
	uint4		status;

	*ustatus = SS_NORMAL;
	nam = cc$rms_nam;
	nam.nam$l_rsa = name_buffer;
	nam.nam$b_rss = SIZEOF(name_buffer);
	nam.nam$l_esa = es_buffer;
	nam.nam$b_ess = SIZEOF(es_buffer);
	nam.nam$b_nop = NAM$M_NOCONCEAL;
	fab = cc$rms_fab;
	fab.fab$l_nam = &nam;
	fab.fab$l_fop = FAB$M_NAM;
	fab.fab$l_fna = fn;
	fab.fab$b_fns = fn_len;
	status = sys$erase(&fab);
	if (!(status & 1))
	{
		*ustatus = fab.fab$l_stv;
		return status;
	}
	*ustatus = status;
	return SS_NORMAL;
}
