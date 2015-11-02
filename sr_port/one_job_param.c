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

#include "compiler.h"
#include "opcode.h"
#include "toktyp.h"
#include "nametabtyp.h"
#include "job.h"
#include "advancewindow.h"
#include "namelook.h"
#include "mvalconv.h"

/* JOB Parameter tables */
#define JPSDEF(a,b,c) {a, b}
const static readonly nametabent job_param_names[] =
{
#include "jobparamstrs.h"
};

#undef JPSDEF
#define JPSDEF(a,b,c) c
const static readonly jp_type job_param_data[] =
{
#include "jobparamstrs.h"
};

const static readonly unsigned char job_param_index[27] =
{
	 0,  2,  2,  2,  6,  8,  8, 10, 10, 14, 14, 14, 16,
	16, 22, 24, 28, 28, 28, 34, 34, 34, 34, 34, 34, 34,
	34
};

#undef JPDEF
#define JPDEF(a,b) b
LITDEF jp_datatype	job_param_datatypes[] =
{
#include "jobparams.h"
};

GBLREF char	window_token, director_token;
GBLREF mval	window_mval;
GBLREF mident	window_ident;

int one_job_param (char **parptr)
{
	boolean_t	neg;
	int		x, num;
        int		len;

	error_def	(ERR_JOBPARUNK);
	error_def	(ERR_JOBPARNOVAL);
	error_def	(ERR_JOBPARVALREQ);
	error_def	(ERR_JOBPARNUM);
	error_def	(ERR_JOBPARSTR);

	if ((window_token != TK_IDENT) ||
	    ((x = namelook (job_param_index, job_param_names, window_ident.addr, window_ident.len)) < 0))
	{
		stx_error (ERR_JOBPARUNK);
		return FALSE;
	}
	advancewindow ();
	*(*parptr)++ = job_param_data[x];
	if (job_param_datatypes[job_param_data[x]] != jpdt_nul)
	{
		if (window_token != TK_EQUAL)
		{
			stx_error (ERR_JOBPARVALREQ);
			return FALSE;
		}
		advancewindow ();
		switch (job_param_datatypes[job_param_data[x]])
		{
			case jpdt_num:
				neg = FALSE;
				if (window_token == TK_MINUS && director_token == TK_INTLIT)
				{
					advancewindow();
					neg = TRUE;
				}
				if (window_token != TK_INTLIT)
				{
					stx_error (ERR_JOBPARNUM);
					return FALSE;
				}
				num = MV_FORCE_INTD(&window_mval);
				*((int4 *) (*parptr)) = (neg ? -num : num);
				*parptr += SIZEOF(int4);
				break;
			case jpdt_str:
				if (window_token != TK_STRLIT)
				{
					stx_error (ERR_JOBPARSTR);
					return FALSE;
				}
				len = window_mval.str.len;
				*(*parptr)++ = len;
				memcpy (*parptr, window_mval.str.addr, len);
				*parptr += len;
				break;
			default:
				GTMASSERT;
		}
		advancewindow ();
	} else if (window_token == TK_EQUAL)
	{
		stx_error (ERR_JOBPARNOVAL);
		return FALSE;
	}
	return TRUE;
}
