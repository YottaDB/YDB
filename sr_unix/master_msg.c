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

#include "mdef.h"
#include "merror.h"

LITREF	err_ctl	gdemsg_ctl;
LITREF	err_ctl cmierrors_ctl;
LITREF	err_ctl cmerrors_ctl;

LITDEF	err_ctl *master_msg[] = {
	&gdemsg_ctl,
	&cmierrors_ctl,
	&cmerrors_ctl,
	0};
