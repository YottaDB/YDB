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

/*
 *  omi_extstr.c ---
 *
 *
 *
 */

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

#include "mdef.h"
#include "omi.h"


GBLDEF char	*omi_oprlist[] = {
    0,
    "Connect",
    "Status",
    "Disconnect",
    0,    0,    0,    0,    0,    0,
    "Set",
    "*Set Piece",
    "*Set Extract",
    "Kill",
    "*Increment",
    0,    0,    0,    0,    0,
    "Get",
    "Define",
    "Order",
    "Next",
    "Query",
    "Reverse Order",
    "*Reverse Query",
    0,    0,    0,
    "Lock",
    "Unlock",
    "Unlock Client",
    "Unlock All"
};

GBLDEF char	*omi_errlist[] = {
    "No Error",
    "User not authorized",
    "No such environment",
    "Global Reference content not valid",
    "Global Reference too long",
    "Global Value too long",
    "Unrecoverable error while performing operation",
    0,    0,    0,
    "Global Reference format not valid",
    "*Message format not valid",
    "Operation Type not valid",
    "Service temporarily suspended",
    "*Sequence number error",
    0,    0,    0,    0,    0,
    "OMI Version not supported",
    "*Length negotiation - Agent minimum greater than server maximum",
    "*Length negotiation - Agent maximum less than server minimum",
    "*Connect Request received during session",
    "OMI Session not established"
};
