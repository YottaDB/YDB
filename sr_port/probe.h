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

#ifndef PROBE_included
#define PROBE_included

/* Only VAX has a non-boolean return code due to the secvector mechanism. */
#if defined(VAX)
uint4		probe(uint4 len, sm_uc_ptr_t addr, boolean_t write);
#elif defined(UNIX)
boolean_t	probe(uint4 len, void *addr, boolean_t write);
#else
boolean_t	probe(uint4 len, sm_uc_ptr_t addr, boolean_t write);
#endif

/* GTM_PROBE is defined to call the probe() function. In VAX, probe() resides in GTMSECSHR
 * and might return a status code (actually from change_mode.mar) which should not be confused
 * with the output of the probe() call. Hence the check for TRUE == probe().
 */

#ifdef VAX
#define 	GTM_PROBE(X, Y, Z) 	(TRUE == probe((X), (Y), (Z)))
#else
#define 	GTM_PROBE(X, Y, Z) 	(probe((uint4)(X), (Y), (Z)))
#endif

#define 	WRITE			TRUE
#define 	READ			FALSE

#define		PROBE_EVEN(X)	(!((UINTPTR_T)(X) & 1))
#define		PROBE_ODD(X)	(!PROBE_EVEN(X))

#define		CAREFUL_DECR_CNT(X,Y)	if (PROBE_EVEN(X)) DECR_CNT((X),(Y))
#define		CAREFUL_INCR_CNT(X,Y)	if (PROBE_EVEN(X)) INCR_CNT((X),(Y))

#endif
