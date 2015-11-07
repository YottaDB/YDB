/****************************************************************
 *								*
 *	Copyright 2010, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef FULLBOOL_H_INCLUDED
#define FULLBOOL_H_INCLUDED

enum gtm_bool_type
{
	GTM_BOOL = 0,		/* original GT.M short-circuit Boolean evaluation with naked maintenance */
	FULL_BOOL,		/* standard behavior - evaluate everything with a side effect */
	FULL_BOOL_WARN		/* like FULL_BOOL but give compiler warnings when it makes a difference */
};

enum gtm_se_type
{
	OLD_SE = 0,		/* ignore side effect implications */
	STD_SE,			/* reorder argument processing for left-to-right side effects */
	SE_WARN			/* like STD but give compiler warnings when it makes a difference */
};

#endif /* FULLBOOL_H_INCLUDED */
