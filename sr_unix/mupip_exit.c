/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdlib.h"

#include "iosp.h"
#include "error.h"
#include "mupip_exit.h"
#include "gtmmsg.h"
#include "readline.h"

GBLREF	boolean_t	mupip_exit_status_displayed;

void mupip_exit(int4 stat)
{
	int4	tmp_severity;
	if (stat != SS_NORMAL)
	{
		if (error_condition != stat)		/* If message not already put out.. */
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) stat);
			tmp_severity =  SEVMASK(stat);
		} else
			tmp_severity = severity;
		/* Make sure give an rc when we exit */
		if (SUCCESS != tmp_severity && INFO != tmp_severity)
			stat = (((stat & UNIX_EXIT_STATUS_MASK) != 0) ? stat : -1);
		else
			stat = 0;
	}
	mupip_exit_status_displayed = TRUE;
	readline_write_history();
	EXIT(stat);
}
