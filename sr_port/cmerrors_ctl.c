/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "error.h"

LITDEF	err_msg cmerrors[] = {
	{ "INVPROT", "Invalid protocol specified by remote partner", 0 },
	{ "REGNTFND", "Region referenced not initialized", 0 },
	{ "CMINTQUE", "Interlock failure accessing GT.CM server queue", 0 },
	{ "INVINTMSG", "Invalid interrupt message received.", 0 },
	{ "CMEXCDASTLM", "Exceeded AST limit. Cannot open database.", 0 },
	{ "CMSYSSRV", "Error doing system service, status:", 0 },
};


GBLDEF	err_ctl cmerrors_ctl = {
	249,
	"GTCM",
	&cmerrors[0],
	6};
