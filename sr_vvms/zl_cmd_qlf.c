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

#include "cmd_qlf.h"
#include "stringpool.h"
#include <descrip.h>

unsigned int clich();

#define	COMMAND			"MUMPS "

GBLREF spdesc stringpool;

void zl_cmd_qlf (mstr *quals, command_qualifier *qualif)
{
	unsigned		status, mumps_clitab ();
	struct dsc$descriptor	comdsc;
	error_def		(ERR_COMPILEQUALS);

	ENSURE_STP_FREE_SPACE(quals->len + SIZEOF(COMMAND) - 1);

	memcpy (stringpool.free, COMMAND, SIZEOF(COMMAND) - 1);
	memcpy (stringpool.free + SIZEOF(COMMAND) - 1, quals->addr, quals->len);
	comdsc.dsc$w_length	= SIZEOF(COMMAND) - 1 + quals->len;
	comdsc.dsc$b_dtype	= DSC$K_DTYPE_T;
	comdsc.dsc$b_class	= DSC$K_CLASS_S;
	comdsc.dsc$a_pointer	= stringpool.free;
	lib$establish (clich);
	status = cli$dcl_parse (&comdsc, &mumps_clitab, 0, 0);
	lib$revert ();
	if (!(status & 1))
		rts_error(VARLSTCNT(5) ERR_COMPILEQUALS, 2, quals->len, quals->addr, status);
	qualif->object_file.mvtype = qualif->list_file.mvtype = qualif->ceprep_file.mvtype = 0;
	get_cmd_qlf (qualif);
}
