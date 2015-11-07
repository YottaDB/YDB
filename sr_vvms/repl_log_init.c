/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_stdio.h"

GBLDEF int		gtmsource_log_fd = FD_INVALID;
GBLDEF int		gtmrecv_log_fd = FD_INVALID;
GBLDEF int		updproc_log_fd = FD_INVALID;
GBLDEF int		updhelper_log_fd = FD_INVALID;

GBLDEF FILE		*gtmsource_log_fp = NULL;
GBLDEF FILE		*gtmrecv_log_fp = NULL;
GBLDEF FILE		*updproc_log_fp = NULL;
GBLDEF FILE		*updhelper_log_fp = NULL;
