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

#ifndef PROBE_included
#define PROBE_included

#ifdef UNIX
boolean_t	probe(uint4 len, void *addr, boolean_t write);
#elif VMS
boolean_t	probe(uint4 len, sm_uc_ptr_t addr, boolean_t write);
#endif

#define 	GTM_PROBE(X, Y, Z) 	(probe((X), (Y), (Z)))
#define 	WRITE			TRUE
#define 	READ			FALSE

#define		PROBE_EVEN(X)	(!((unsigned int)(X) & 1))
#define		PROBE_ODD(X)	(!PROBE_EVEN(X))

#define		CAREFUL_DECR_CNT(X,Y)	if (PROBE_EVEN(X)) DECR_CNT((X),(Y))

#endif
