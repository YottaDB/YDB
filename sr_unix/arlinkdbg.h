/****************************************************************
 *								*
 * Copyright (c) 2014-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Header file to control debugging for autorelink */
#ifndef ARLINKDBG_H
#define ARLINKDBG_H

/* To enable debugging macros (output to console) uncomment the following #define */
/*#define DEBUG_ARLINK*/
#ifdef DEBUG_ARLINK
# define DBGARLNK(x) DBGFPF(x)
# define DBGARLNK_ONLY(x) x
# include "gtm_stdio.h"
# include "gtmio.h"
# include "io.h"
#else
# define DBGARLNK(x)
# define DBGARLNK_ONLY(x)
#endif

#endif
