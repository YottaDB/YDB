/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef REPL_DBG_H
#define REPL_DBG_H

#ifdef REPL_DEBUG
#include "gtm_stdio.h"

#define REPL_DEBUG_ONLY(stmt)		stmt

#define REPL_DPRINT1(p)			{ \
						FPRINTF(stderr, p); \
				         	FFLUSH(stderr); \
					}
#define REPL_DPRINT2(p, q)		{ \
						FPRINTF(stderr, p, q); \
						FFLUSH(stderr); \
					}
#define REPL_DPRINT3(p, q, r)		{ \
						FPRINTF(stderr, p, q, r); \
						FFLUSH(stderr); \
					}
#define REPL_DPRINT4(p, q, r, s)	{ \
						FPRINTF(stderr, p, q, r, s); \
						FFLUSH(stderr); \
					}
#define REPL_DPRINT5(p, q, r, s, t)	{ \
						FPRINTF(stderr, p, q, r, s, t);\
						FFLUSH(stderr); \
					}
#define REPL_DPRINT6(p, q, r, s, t, u)	{ \
						FPRINTF(stderr, p, q, r, s, t, u);\
						FFLUSH(stderr); \
					}
#ifdef REPL_EXTRA_DEBUG

#define REPL_EXTRA_DEBUG_ONLY(stmt)		stmt
#define REPL_EXTRA_DPRINT1(p)			REPL_DPRINT1(p)
#define REPL_EXTRA_DPRINT2(p, q)		REPL_DPRINT2(p, q)
#define REPL_EXTRA_DPRINT3(p, q, r)		REPL_DPRINT3(p, q, r)
#define REPL_EXTRA_DPRINT4(p, q, r, s)		REPL_DPRINT4(p, q, r, s)
#define REPL_EXTRA_DPRINT5(p, q, r, s, t)	REPL_DPRINT5(p, q, r, s, t)
#define REPL_EXTRA_DPRINT6(p, q, r, s, t, u)	REPL_DPRINT6(p, q, r, s, t, u)

#endif /* REPL_EXTRA_DEBUG */

#else /* ! REPL_DEBUG */

#define REPL_DEBUG_ONLY(stmt)
#define REPL_DPRINT1(p)
#define REPL_DPRINT2(p, q)
#define REPL_DPRINT3(p, q, r)
#define REPL_DPRINT4(p, q, r, s)
#define REPL_DPRINT5(p, q, r, s, t)
#define REPL_DPRINT6(p, q, r, s, t, u)

#endif /* REPL_DEBUG */

#if !defined(REPL_DEBUG) || !defined(REPL_EXTRA_DEBUG)

#define REPL_EXTRA_DEBUG_ONLY(stmt)
#define REPL_EXTRA_DPRINT1(p)
#define REPL_EXTRA_DPRINT2(p, q)
#define REPL_EXTRA_DPRINT3(p, q, r)
#define REPL_EXTRA_DPRINT4(p, q, r, s)
#define REPL_EXTRA_DPRINT5(p, q, r, s, t)
#define REPL_EXTRA_DPRINT6(p, q, r, s, t, u)

#endif /* !REPL_DEBUG || !REPL_EXTRA_DEBUG */

#endif /* REPL_DBG_H */
