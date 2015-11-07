/****************************************************************
 *								*
 * Copyright (c) 2015 Fidelity National Information 		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef INDRCOMPTRC_H
#define INDRCOMPTRC_H

/* Provide some tracing for indirect compiles but due to the multiple rtnhdr.h files, these macros have
 * been moved out to this supplemental header file so can be included where needed.
 *
 * Uncomment below #define or use -DDEBUG_INDRCOMP compilation option to enable debugging macros.
 */
/* #define DEBUG_INDRCOMP */
#if defined(DEBUG_INDRCOMP)
# define DBGINDCOMP(x) DBGFPF(x)
# define DBGINDCOMP_ONLY(x) x
# include "gtm_stdio.h"
# include "gtmio.h"
# include "io.h"
#else
# define DBGINDCOMP(x)
# define DBGINDCOMP_ONLY(x)
#endif

#endif
