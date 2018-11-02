/****************************************************************
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Include to define some debugging macros for libyottadb */
#ifndef LIBYOTTADB_DBG_H
#define LIBYOTTADB_DBG_H

//#define YDB_TRACE_API
//#define YDB_TRACE_APITP

/* Macros to assist in tracing calls to console */
#ifdef YDB_TRACE_API
# include "gtm_stdio.h"
# include "io.h"		/* For flush_pio */
# include "gtmio.h"		/* For FFLUSH macro */
# define DBGAPI(x) DBGFPF(x)
# define DBGAPI_ONLY(x) x
#else
# define DBGAPI(x)
# define DBGAPI(x)
#endif

/* Macros to help debug TP multi-threading locks */
#ifdef YDB_TRACE_APITP
# include "gtm_stdio.h"
# include "io.h"		/* For flush_pio */
# include "gtmio.h"		/* For FFLUSH macro */
# define DBGAPITP(x) DBGFPF(x)
# define DBGAPITP_ONLY(x) x
#else
# define DBGAPITP(x)
# define DBGAPITP_ONLY(x)
#endif

#endif
