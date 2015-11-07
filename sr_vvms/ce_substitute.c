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

#include <ssdef.h>
#include <descrip.h>

#include "compiler.h"
#include "comp_esc.h"

GBLREF unsigned char		*source_buffer;
GBLREF boolean_t		run_time;
GBLREF struct ce_sentinel_desc	*ce_def_list;

error_def(ERR_CEUSRERROR);
error_def(ERR_CEBIGSKIP);
error_def(ERR_CETOOLONG);

void ce_substitute (struct ce_sentinel_desc *shp, int4 source_column, int4 *skip_count)
{
	unsigned char	*cp, sub_buffer[MAX_SRCLINE];
	int4		source_length, tail_length;
	int4		lcl_src_col, sav_last_src_col, status;
	boolean_t	run_or_compile;
	struct dsc$descriptor_s	buffer, substitution;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	for (source_length = source_column;
	     source_buffer[source_length-1] != '\0'  &&  source_length <= MAX_SRCLINE;
	     source_length++)
		;
	buffer.dsc$a_pointer = source_buffer;
	buffer.dsc$w_length = source_length;
	substitution.dsc$a_pointer = sub_buffer;
	substitution.dsc$w_length = MAX_SRCLINE;
	/* Copy important values to locals to prevent inadvertent user modification. */
	run_or_compile = run_time;
	lcl_src_col = source_column;
	status = shp->user_routine(&buffer, &lcl_src_col, run_or_compile, &substitution, skip_count);
	if (status != SS$_NORMAL)
	{
		sav_last_src_col = TREF(last_source_column);
		TREF(last_source_column) = source_column;
		stx_error (ERR_CEUSRERROR, 1, status);
		TREF(last_source_column) = sav_last_src_col;
		return;
	}
	tail_length = source_length - (source_column + *skip_count) + 1;
	if (0 > tail_length)
	{
		sav_last_src_col = TREF(last_source_column);
		TREF(last_source_column) = source_column;
		stx_error (ERR_CEBIGSKIP);
		TREF(last_source_column) = sav_last_src_col;
		return;
	}
	source_length = (source_column - 1) + substitution.dsc$w_length + tail_length;
	if (source_length  >  MAX_SRCLINE - 2)
	{
		sav_last_src_col = TREF(last_source_column);
		TREF(last_source_column) = source_column;
		stx_error (ERR_CETOOLONG);
		TREF(last_source_column) = sav_last_src_col;
		return;
	}
	if (0 < tail_length)
	{
		memcpy (&source_buffer[(source_column-1) + substitution.dsc$w_length],
			&source_buffer[(source_column-1) + *skip_count],
			tail_length);
	}
	if (0 < substitution.dsc$w_length)
		memcpy (&source_buffer[source_column-1], substitution.dsc$a_pointer, substitution.dsc$w_length);
	source_buffer[source_length] = source_buffer[source_length+1] = '\0';
	return;
}
