/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*	File modified by Hallgarth on 28-APR-1986 08:39:49.98    */

/* Define some macros to allow conditional commas at the end of statements useful for when the last entry in the
 * xfer table is surrounded by #ifdef. This is sort of like UNIX_ONLY_COMMA() except that macro drops the entire
 * argument if not UNIX, not just the comma.
 */
#ifdef UNIX
# define UNIX_COMMA(x) x,
# define VMS_COMMA(x) x
#else
# define UNIX_COMMA(x) x
# define VMS_COMMA(x) x,
#endif

/* Declare all xf_* enumerators */
#ifndef XFER_ENUM_H
#define XFER_ENUM_H

#define XFER(a,b) a
enum
{
#include "xfer.h"
};
#undef XFER

typedef int (* volatile xfer_entry_t)();

#ifndef UNICODE_SUPPORTED
/* Call "z" counterparts for non-unicode flavor of these functions on VMS. */
#define op_fnascii op_fnzascii
#define op_fnchar op_fnzchar
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
#define op_fnreverse op_fnzreverse
#endif /* UNICODE_SUPPORTED */
#endif /* XFER_ENUM_H */
