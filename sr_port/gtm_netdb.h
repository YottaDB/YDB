/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* gtm_netdb.h - interlude to <netdb.h> system header file.  */
#ifndef GTM_NETDBH
#define GTM_NETDBH

#include <netdb.h>

#define MAX_GETHOST_TRIES	8
#define GETHOSTBYNAME		gethostbyname
#define GETHOSTBYADDR		gethostbyaddr
#define HSTRERROR		hstrerror

#endif
