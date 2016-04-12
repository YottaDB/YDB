/****************************************************************
 *								*
 *	Copyright 2005, 2011 Fidelity Information Services, LLC.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "cli.h"

/* Below are defined in dbcertify.h but that requires SO many includes prior this seemed easier */
void dbcertify_scan_phase(void);
void dbcertify_certify_phase(void);

/*********************************************************************
 * Parameters must be defined in the order they are to be specified
 * AND must be alphabetical
 *
 * This file might have lines longer than 132 characters
 * since the command tables are being initialized.
 *********************************************************************/

static readonly CLI_PARM dbscan_parm[] = {
	{ "REGION", "Region:", PARM_REQ},
	{ "", "", PARM_REQ}
};

static	CLI_ENTRY	dbscan_qual[] = {
	{ "BSU_KEYS",      0, 0, 0,            0,                  0, 0, VAL_DISALLOWED,  1, NEG,     VAL_N_A, 0       },
	{ "DEBUG",         0, 0, 0,            0,                  0, 0, VAL_DISALLOWED,  1, NEG,     VAL_N_A, 0       },
	{ "DETAIL",        0, 0, 0,            0,                  0, 0, VAL_DISALLOWED,  1, NEG,     VAL_N_A, 0       },
	{ "KEEP_TEMPS",    0, 0, 0,            0,                  0, 0, VAL_DISALLOWED,  1, NEG,     VAL_N_A, 0       },
	{ "OUTFILE",       0, 0, 0,            0,                  0, 0, VAL_REQ,         1, NON_NEG, VAL_STR, 0       },
	{ "REPORT_ONLY",   0, 0, 0,            0,                  0, 0, VAL_DISALLOWED,  1, NEG,     VAL_N_A, 0       },
	{ "TEMPFILE_DIR",  0, 0, 0,            0,                  0, 0, VAL_REQ,         1, NON_NEG, VAL_STR, 0       },
	{ 0 }
};

static readonly CLI_PARM dbcertify_parm[] = {
	{ "P1OUTFILE", "SCAN output file:", PARM_REQ},
	{ "", "", PARM_REQ}
};

static	CLI_ENTRY	dbcertify_qual[] = {
	{ "BLOCKS",	   0, 0, 0,	       0,		   0, 0, VAL_REQ,	  1, NON_NEG, VAL_HEX, 0       },
	{ "DEBUG",         0, 0, 0,            0,                  0, 0, VAL_DISALLOWED,  1, NEG,     VAL_N_A, 0       },
	{ "KEEP_TEMPS",    0, 0, 0,            0,                  0, 0, VAL_DISALLOWED,  1, NEG,     VAL_N_A, 0       },
	{ "TEMPFILE_DIR",  0, 0, 0,            0,                  0, 0, VAL_REQ,         1, NON_NEG, VAL_STR, 0       },
	{ 0 }
};


GBLDEF	CLI_ENTRY	dbcertify_cmd_ary[] = {
	{ "CERTIFY",   dbcertify_certify_phase, dbcertify_qual,   dbcertify_parm,   0, 0,    0, VAL_DISALLOWED, 1, 0, 0, 0 },
	{ "SCAN",      dbcertify_scan_phase,	dbscan_qual,	  dbscan_parm,      0, 0,    0, VAL_DISALLOWED, 1, 0, 0, 0 },
	{ 0 }
};
