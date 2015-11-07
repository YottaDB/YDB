/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include "jobparamstrs.h"	/* BYPASSOK */
};
/* Index is the number of param strings before the character
 * For instance index(D) = n(A) + n(B) + n(C) = 2 + 0 + 0 =2
 */
#ifdef UNIX
const static readonly unsigned char job_param_index[27] =
{
      /* A(2)    B(0)   C(2)   D(4)   E(2)   F(0)  G(2)  H(0)  I(4)   J(0)  K(0)  L(2)  M(0) */
	 0,	2,    	 2,     4,     8,    10,     10,   12,   12,    16,   16,   16,   18,
      /* N(6)	O(2)	P(4)	Q(0)	R(0)	S(6)	T(0)   U(0)	V(0)	W(0)   X(0)	Y(0)	Z(0) */
	18,	24,	26,	30,	30,	30,	36,	36,	36,	36,	36,	36,	36,
	36
};
#else
const static readonly unsigned char job_param_index[27] =
{
	 0,  2,  2,  2,  6,  8,  8, 10, 10, 14, 14, 14, 16,
	16, 22, 24, 28, 28, 28, 34, 34, 34, 34, 34, 34, 34,
	34
};
#endif

#undef JPDEF
#define JPDEF(a,b) b
LITDEF jp_datatype	job_param_datatypes[] =
{
#include "jobparams.h"
};

/* Maximum length string for any JOB parameter. Length limit
 * dictated by having only one byte to represent the string
 * length. That translates to 255 characters*/
#define MAXJOBPARSTRLEN 255

error_def(ERR_JOBPARNOVAL);
error_def(ERR_JOBPARNUM);
error_def(ERR_JOBPARSTR);
error_def(ERR_JOBPARTOOLONG);
error_def(ERR_JOBPARUNK);
error_def(ERR_JOBPARVALREQ);

int one_job_param (char **parptr)
{
	boolean_t	neg;
	int		x, num;
	int		len;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if ((TK_IDENT != TREF(window_token))
	     || (0 > (x = (namelook(job_param_index, job_param_names, (TREF(window_ident)).addr, (TREF(window_ident)).len)))))
	{	/* NOTE assigment above */
		stx_error (ERR_JOBPARUNK);
		return FALSE;
	}
	advancewindow();
	*(*parptr)++ = job_param_data[x];
	if (job_param_datatypes[job_param_data[x]] != jpdt_nul)
	{
		if (TK_EQUAL != TREF(window_token))
		{
			stx_error (ERR_JOBPARVALREQ);
			return FALSE;
		}
		advancewindow ();
		switch (job_param_datatypes[job_param_data[x]])
		{
		case jpdt_num:
			neg = FALSE;
			if ((TK_MINUS == TREF(window_token)) && (TK_INTLIT == TREF(director_token)))
			{
				advancewindow();
				neg = TRUE;
			}
			if (TK_INTLIT != TREF(window_token))
			{
				stx_error (ERR_JOBPARNUM);
				return FALSE;
			}
			num = MV_FORCE_INTD(&(TREF(window_mval)));
			*((int4 *)(*parptr)) = (neg ? -num : num);
			*parptr += SIZEOF(int4);
			break;
		case jpdt_str:
			if (TK_STRLIT != TREF(window_token))
			{
				stx_error (ERR_JOBPARSTR);
				return FALSE;
			}
			len = (TREF(window_mval)).str.len;
			if (MAXJOBPARSTRLEN < len)
			{
				stx_error (ERR_JOBPARTOOLONG);
				return FALSE;
			}
			*(*parptr)++ = len;
			memcpy(*parptr, (TREF(window_mval)).str.addr, len);
			*parptr += len;
			break;
		default:
			GTMASSERT;
		}
		advancewindow ();
	} else if (TK_EQUAL == TREF(window_token))
	{
		stx_error (ERR_JOBPARNOVAL);
		return FALSE;
	}
	return TRUE;
}
