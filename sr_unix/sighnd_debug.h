/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2019-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef SIGHND_DEBUG_H_INCLUDED
#define SIGHND_DEBUG_H_INCLUDED

/* Define debugging macros for signal handling - uncomment the define below to enable but note it emits
 * output to stderr.
 */
//#define DEBUG_SIGNAL_HANDLING
#ifdef DEBUG_SIGNAL_HANDLING
# define DBGSIGHND(x) DBGFPF(x)
# define DBGSIGHND_ONLY(x) x
GBLREF int forced_exit_sig;
# include "gtm_stdio.h"
# include "gtmio.h"
# include "io.h"
#else
# define DBGSIGHND(x)
# define DBGSIGHND_ONLY(x)
#endif

#endif
