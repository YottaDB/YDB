/****************************************************************
 *								*
 *	Copyright 2001,2013 Fidelity Information Services, Inc	*
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
	"INVPROT", "Invalid protocol specified by remote partner", 0,
	"REGNTFND", "Region referenced not initialized", 0,
	"CMINTQUE", "Interlock failure accessing GT.CM server queue", 0,
	"INVINTMSG", "Invalid interrupt message received.", 0,
	"CMEXCDASTLM", "Exceeded AST limit. Cannot open database.", 0,
	"CMSYSSRV", "Error doing system service, status:", 0,
};

LITDEF	int CMERR_INVPROT = 150568970;
LITDEF	int CMERR_REGNTFND = 150568978;
LITDEF	int CMERR_CMINTQUE = 150568988;
LITDEF	int CMERR_INVINTMSG = 150568994;
LITDEF	int CMERR_CMEXCDASTLM = 150569002;
LITDEF	int CMERR_CMSYSSRV = 150569010;

GBLDEF	err_ctl cmerrors_ctl = {
	249,
	"GTCM",
	&cmerrors[0],
	6};
