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

LITDEF	err_msg cmierrors[] = {
	{ "DCNINPROG", "Attempt to initiate operation while disconnect was in progress", 0 },
	{ "LNKNOTIDLE", "Attempt to initiate operation before previous operation completed", 0 },
	{ "ASSERT", "Assert failed !AD line !UL", 3 },
	{ "CMICHECK", "Internal CMI error. Report to your GT.M Support Channel.", 0 },
	{ "NETFAIL", "Failure of Net operation", 0 },
	{ "BADPORT", "Environment variable GTCM_TCP_PORT is not an integer", 0 },
	{ "NOTND", "tnd argument to cmi_init is NULL", 0 },
	{ "OVERRUN", "mbf argument in CLB is not large enough for packet", 0 },
	{ "NOSERVENT", "Sevices data lookup failure", 0 },
	{ "BADIPADDRPORT", "Bad specification of [ip address:port] in tnd", 0 },
	{ "REASON_CONNECT", "Incoming connection", 0 },
	{ "REASON_INTMSG", "Incoming urgent data", 0 },
	{ "REASON_DISCON", "Disconnect encountered", 0 },
	{ "REASON_ABORT", "Link aborted", 0 },
	{ "REASON_EXIT", "Exit", 0 },
	{ "REASON_PATHLOST", "Network path lost", 0 },
	{ "REASON_PROTOCOL", "Protocol error", 0 },
	{ "REASON_THIRDPARTY", "Thirdparty error", 0 },
	{ "REASON_TIMEOUT", "Network timeout", 0 },
	{ "REASON_NETSHUT", "Shutdown received", 0 },
	{ "REASON_REJECT", "Connection rejected", 0 },
	{ "REASON_IODONE", "I/O done", 0 },
	{ "REASON_OVERRUN", "Input overran buffer", 0 },
	{ "REASON_STATUS", "Status", 0 },
	{ "REASON_CONFIRM", "Confirm", 0 },
};


GBLDEF	err_ctl cmierrors_ctl = {
	250,
	"CMI",
	&cmierrors[0],
	25};
