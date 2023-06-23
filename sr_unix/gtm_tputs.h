/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTM_TPUTS_H_INCLUDED
#define GTM_TPUTS_H_INCLUDED

#ifdef __sparc
int gtm_tputs(char *, int, int (*)(char));
#else
int gtm_tputs(char *, int, int (*)(int));
#endif

#endif
