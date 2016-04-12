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

#ifndef __GTM_TPUTS_H__
#define __GTM_TPUTS_H__

#ifdef __sparc
int gtm_tputs(char *, int, int (*)(char));
#else
int gtm_tputs(char *, int, int (*)(int));
#endif

#endif
