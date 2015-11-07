/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#if defined(__alpha) && defined(__vms)
#define memmove gtm_memmove
#endif
#define GTMSECSHR_SET_PRIV(X, Y)	\
{				\
	prvadr[1] = 0;		\
	prvadr[0] = (X);	\
	Y = sys$setprv(TRUE, prvadr, FALSE, prvprv);	\
}
#define GTMSECSHR_REL_PRIV	\
if (0 != (prvadr[0] &= ~prvprv[0]))	\
{				\
	sys$setprv(FALSE, prvadr, FALSE, NULL);	\
}
#ifdef DEBUG
#define GTMSECSHR_SET_DBG_PRIV(X, Y)	GTMSECSHR_SET_PRIV((X), (Y))
#define GTMSECSHR_REL_DBG_PRIV		GTMSECSHR_REL_PRIV
#else
/* in kernel mode - can do whatever anyway */
#define GTMSECSHR_SET_DBG_PRIV(X, Y)	Y = TRUE
#define GTMSECSHR_REL_DBG_PRIV
#endif

