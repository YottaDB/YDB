/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "compiler.h"
#include "comp_esc.h"

GBLREF unsigned char *source_buffer;
GBLREF short int source_column;
GBLREF char *lexical_ptr;
GBLREF struct ce_sentinel_desc	*ce_def_list;

void ce_substitute(struct ce_sentinel_desc *shp, int4 source_col, int4 *skip_ct)
{
	unsigned char	*cp, sub_buffer[MAX_SRCLINE];
	short		source_length, tail_length;
	int4		lcl_src_col, skip_count, status;
	bool		run_or_compile;

	/* Invoke user-supplied routine in order to obtain substitution text at sentinel string. */

	/* Just a stub for Unix. */

	return;
}
