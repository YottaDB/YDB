/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef _REPL_DBG_H
#define _REPL_DBG_H

#ifdef REPL_DEBUG
#include "gtm_stdio.h"
#define REPL_DPRINT1(p)			{ \
						FPRINTF(stderr, p); \
				         	fflush(stderr); \
					}
#define REPL_DPRINT2(p, q)		{ \
						FPRINTF(stderr, p, q); \
						fflush(stderr); \
					}
#define REPL_DPRINT3(p, q, r)		{ \
						FPRINTF(stderr, p, q, r); \
						fflush(stderr); \
					}
#define REPL_DPRINT4(p, q, r, s)	{ \
						FPRINTF(stderr, p, q, r, s); \
						fflush(stderr); \
					}
#define REPL_DPRINT5(p, q, r, s, t)	{ \
						FPRINTF(stderr, p, q, r, s, t);\
						fflush(stderr); \
					}
#define REPL_DPRINT6(p, q, r, s, t, u)	{ \
						FPRINTF(stderr, p, q, r, s, t, u);\
						fflush(stderr); \
					}
#else

#define REPL_DPRINT1(p)
#define REPL_DPRINT2(p, q)
#define REPL_DPRINT3(p, q, r)
#define REPL_DPRINT4(p, q, r, s)
#define REPL_DPRINT5(p, q, r, s, t)
#define REPL_DPRINT6(p, q, r, s, t, u)

#endif

#endif
