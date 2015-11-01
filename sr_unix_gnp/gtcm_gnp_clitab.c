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
#include "cli.h"

static readonly CLI_ENTRY gtcm_gnp_qual[] = {
{ "LOG", 0, 0, 0, VAL_REQ, 1, NON_NEG, VAL_STR, 0},
{ "SERVICE", 0, 0, 0, VAL_REQ, 1, NON_NEG, VAL_STR, 0},
{ "TIMEOUT", 0, 0, 0, VAL_REQ, 1, NON_NEG, VAL_NUM, 0},
{ "TRACE", 0, 0, 0, VAL_DISALLOWED, 0, NON_NEG, VAL_N_A, 0},
{ 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

GBLDEF CLI_ENTRY cmd_ary[] = {
{ "GTCM_GNP_SERVER", 0, gtcm_gnp_qual, 0, VAL_DISALLOWED, 1, NON_NEG, VAL_LIST, 0},
{ 0, 0, 0, 0, 0, 0, 0, 0, 0}
};
