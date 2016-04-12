/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "cli.h"

/*************************************************************
 * NOTE
 * This file might have lines longer than 132 characters
 * since the command tables are being initialized.
 *
 * Entries need to be made in sorted order (lexicographic) within
 * each table.
 ************************************************************/

static readonly CLI_ENTRY gtcm_gnp_qual[] = {
{ "LOG", 0, 0, 0, 0, 0, 0, VAL_REQ, 1, NON_NEG, VAL_STR, 0},
{ "SERVICE", 0, 0, 0, 0, 0, 0, VAL_REQ, 1, NON_NEG, VAL_STR, 0},
{ "TIMEOUT", 0, 0, 0, 0, 0, 0, VAL_REQ, 1, NON_NEG, VAL_NUM, 0},
{ "TRACE", 0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NON_NEG, VAL_N_A, 0},
{ 0 }
};

GBLDEF CLI_ENTRY gtcm_gnp_cmd_ary[] = {
{ "GTCM_GNP_SERVER", 0, gtcm_gnp_qual, 0, 0, 0, 0, VAL_DISALLOWED, 1, NON_NEG, VAL_N_A, 0},
{ 0 }
};
