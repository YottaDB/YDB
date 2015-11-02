/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef INVOCATION_MODE_H
#define INVOCATION_MODE_H

GBLREF unsigned int     invocation_mode;

/* flags for invocation_mode */
#define MUMPS_COMPILE	(1 << 0) /* compile-only invocation - mumps <file.m> */
#define MUMPS_RUN	(1 << 1) /* mumps -run file */
#define MUMPS_DIRECT	(1 << 2) /* mumps -direct */
#define MUMPS_CALLIN	(1 << 3) /* libgtmci.so linked with call-in user */
#define MUMPS_GTMCI	(1 << 4) /* current environment is created by gtm_ci */
#define MUMPS_UTILTRIGR	(1 << 5) /* One of the utilities needs to run triggers */
#define MUMPS_GTMCI_OFF	(~MUMPS_GTMCI) /* to turn-off MUMPS_GTMCI */

#endif
