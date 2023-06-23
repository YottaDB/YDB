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

#define MAX_US_READ 128

/* ***************************************************** */
/* ********* structure for user device driver ********** */
/* ***************************************************** */

typedef struct
{
	dev_dispatch_struct *disp;
}d_us_struct;
