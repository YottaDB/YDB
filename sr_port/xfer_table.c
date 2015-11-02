/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "xfer_enum.h"

/* Declare all prototypes with same signature as xfer_entry_t */
#define XFER(a,b) b()
int
#include "xfer.h"
;
#undef XFER

#ifndef UNICODE_SUPPORTED
/* Call "z" counterparts for non-unicode flavor of these functions on unsupported platforms */

#define op_fnascii op_fnzascii
#define op_fnchar op_fnzchar
#define op_fnzechar op_fnzchar
#define op_fnextract op_fnzextract
#define op_setextract op_setzextract
#define op_fnfind op_fnzfind
#define op_fnj2 op_fnzj2
#define op_fnlength op_fnzlength
#define op_fnpopulation op_fnzpopulation
#define op_fnpiece op_fnzpiece
#define op_fnp1 op_fnzp1
#define op_setpiece op_setzpiece
#define op_setp1 op_setzp1
#define op_fntranslate op_fnztranslate

#endif

/* Initialize the table with the runtime routine functions */

#define XFER(a,b) b

GBLDEF xfer_entry_t xfer_table[] =
{
#include "xfer.h"
};
#undef XFER


