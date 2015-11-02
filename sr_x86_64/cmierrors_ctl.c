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

LITDEF	err_msg cmierrors[] = {
	"DCNINPROG", "Attempt to initiate operation while disconnect was in progress", 0,
	"LNKNOTIDLE", "Attempt to initiate operation before previous operation completed", 0,
	"ASSERT", "Assert failed !AD line !UL", 3,
	"CMICHECK", "Internal CMI error. Report to your GT.M Support Channel.", 0,
	"NETFAIL", "Failure of Net operation", 0,
	"BADPORT", "Environment variable GTCM_TCP_PORT is not an integer", 0,
	"NOTND", "tnd argument to cmi_init is NULL", 0,
	"OVERRUN", "mbf argument in CLB is not large enough for packet", 0,
	"NOSERVENT", "Sevices data lookup failure", 0,
	"BADIPADDRPORT", "Bad specification of [ip address:port] in tnd", 0,
	"REASON_CONNECT", "Incoming connection", 0,
	"REASON_INTMSG", "Incoming urgent data", 0,
	"REASON_DISCON", "Disconnect encountered", 0,
	"REASON_ABORT", "Link aborted", 0,
	"REASON_EXIT", "Exit", 0,
	"REASON_PATHLOST", "Network path lost", 0,
	"REASON_PROTOCOL", "Protocol error", 0,
	"REASON_THIRDPARTY", "Thirdparty error", 0,
	"REASON_TIMEOUT", "Network timeout", 0,
	"REASON_NETSHUT", "Shutdown received", 0,
	"REASON_REJECT", "Connection rejected", 0,
	"REASON_IODONE", "I/O done", 0,
	"REASON_OVERRUN", "Input overran buffer", 0,
	"REASON_STATUS", "Status", 0,
	"REASON_CONFIRM", "Confirm", 0,
};

LITDEF	int CMI_DCNINPROG = 150634508;
LITDEF	int CMI_LNKNOTIDLE = 150634516;
LITDEF	int CMI_ASSERT = 150634522;
LITDEF	int CMI_CMICHECK = 150634532;
LITDEF	int CMI_NETFAIL = 150634538;
LITDEF	int CMI_BADPORT = 150634546;
LITDEF	int CMI_NOTND = 150634556;
LITDEF	int CMI_OVERRUN = 150634562;
LITDEF	int CMI_NOSERVENT = 150634570;
LITDEF	int CMI_BADIPADDRPORT = 150634578;
LITDEF	int CMI_REASON_CONNECT = 150634586;
LITDEF	int CMI_REASON_INTMSG = 150634594;
LITDEF	int CMI_REASON_DISCON = 150634602;
LITDEF	int CMI_REASON_ABORT = 150634610;
LITDEF	int CMI_REASON_EXIT = 150634618;
LITDEF	int CMI_REASON_PATHLOST = 150634626;
LITDEF	int CMI_REASON_PROTOCOL = 150634634;
LITDEF	int CMI_REASON_THIRDPARTY = 150634642;
LITDEF	int CMI_REASON_TIMEOUT = 150634650;
LITDEF	int CMI_REASON_NETSHUT = 150634658;
LITDEF	int CMI_REASON_REJECT = 150634666;
LITDEF	int CMI_REASON_IODONE = 150634674;
LITDEF	int CMI_REASON_OVERRUN = 150634682;
LITDEF	int CMI_REASON_STATUS = 150634690;
LITDEF	int CMI_REASON_CONFIRM = 150634698;

GBLDEF	err_ctl cmierrors_ctl = {
	250,
	"CMI",
	&cmierrors[0],
	25};
