/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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

static readonly CLI_PARM mumps_parm[] = {
	{"INFILE", "What file: ", PARM_REQ}
};

static readonly CLI_ENTRY mumps_qual[] = {
{ "ALIGN_STRINGS",	0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NEG, 	VAL_N_A, 0},
{ "CROSS_REFERENCE", 	0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NON_NEG, 	VAL_N_A, 0},
{ "DEBUG", 		0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NON_NEG, 	VAL_N_A, 0},
{ "DIRECT_MODE", 	0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NON_NEG, 	VAL_N_A, 0},
{ "DYNAMIC_LITERALS", 	0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NEG, 	VAL_N_A, 0},
{ "IGNORE", 		0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NEG, 	VAL_N_A, 0},
{ "INLINE_LITERALS",	0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NEG, 	VAL_N_A, 0},
{ "LABELS", 		0, 0, 0, 0, 0, 0, VAL_REQ, 	  1, NON_NEG, 	VAL_STR, 0},
{ "LENGTH", 		0, 0, 0, 0, 0, 0, VAL_REQ, 	  1, NON_NEG, 	VAL_NUM, 0},
{ "LINE_ENTRY", 	0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NEG, 	VAL_N_A, 0},
{ "LIST", 		0, 0, 0, 0, 0, 0, VAL_NOT_REQ, 	  1, NEG, 	VAL_STR, 0},
{ "MACHINE_CODE", 	0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NON_NEG, 	VAL_N_A, 0},
{ "NAMEOFRTN",		0, 0, 0, 0, 0, 0, VAL_REQ,	  1, NON_NEG,	VAL_STR, 0},
{ "OBJECT", 		0, 0, 0, 0, 0, 0, VAL_NOT_REQ, 	  1, NEG, 	VAL_STR, 0},
{ "RUN", 		0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NON_NEG, 	VAL_N_A, 0},
{ "SPACE", 		0, 0, 0, 0, 0, 0, VAL_REQ, 	  1, NON_NEG, 	VAL_NUM, 0},
{ "WARNINGS", 		0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NEG, 	VAL_N_A, 0},
{ 0 }
};

GBLDEF CLI_ENTRY mumps_cmd_ary[] = {
{ "MUMPS", 0, mumps_qual, mumps_parm, 0, 0, 0, VAL_DISALLOWED, 1, 0, VAL_LIST, 0},
{ 0 }
};
