/****************************************************************
 *								*
 * Copyright (c) 2001-2025 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 * --------------------------------------------------------------
 * Parser tables.
 * Entries need to be made in sorted order (lexicographic) within
 * each table.
 *
 * This file might have lines longer than 132 characters
 * since the command tables are being initialized.
 * --------------------------------------------------------------
 */

#include "mdef.h"
#include "mlkdef.h"
#include "cmidef.h"
#include "cli.h"
#include "lke.h"
#include "util_spawn.h"
#include "util_help.h"
#include "lke_cmd_disallow.h"

static readonly CLI_ENTRY clear_qual[] = {
{ "ALL",         0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NON_NEG, VAL_N_A, 0       },
{ "EXACT",       0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NEG,     VAL_N_A, 0       },
{ "INTERACTIVE", 0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NEG,     VAL_N_A, 0       },
{ "LOCK",        0, 0, 0, 0, 0, 0, VAL_REQ,        1, NON_NEG, VAL_STR, 0       },
{ "OUTPUT",      0, 0, 0, 0, 0, 0, VAL_REQ,        1, NON_NEG, VAL_STR, 0       },
{ "PID",         0, 0, 0, 0, 0, 0, VAL_REQ,        1, NON_NEG, VAL_NUM, VAL_DCM },
{ "REGION",      0, 0, 0, 0, 0, 0, VAL_REQ,        0, NON_NEG, VAL_STR, 0       },
{ "" }
};

static readonly CLI_ENTRY show_qual[] = {
{ "ALL",      0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NON_NEG, VAL_N_A, 0       },
{ "CRITICAL", 0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NEG,     VAL_N_A, 0       },
{ "LOCK",     0, 0, 0, 0, 0, 0, VAL_REQ,        1, NON_NEG, VAL_STR, 0       },
{ "MEMORY",   0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NON_NEG, VAL_N_A, 0       },
{ "OUTPUT",   0, 0, 0, 0, 0, 0, VAL_REQ,        1, NON_NEG, VAL_STR, 0       },
{ "PID",      0, 0, 0, 0, 0, 0, VAL_REQ,        1, NON_NEG, VAL_NUM, VAL_DCM },
{ "REGION",   0, 0, 0, 0, 0, 0, VAL_REQ,        0, NON_NEG, VAL_STR, 0       },
{ "WAIT",     0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NON_NEG, VAL_STR, 0       },
{ "" }
};

/* These must be alphabetized */
static readonly CLI_ENTRY clnup_qual[] = {
{ "ALL",      0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NON_NEG, VAL_N_A, 0 },
{ "INTEG",    0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NON_NEG, VAL_N_A, 0 },
{ "PERIODIC", 0, 0, 0, 0, 0, 0, VAL_REQ,        0, NON_NEG, VAL_NUM, 0 },
{ "REGION",   0, 0, 0, 0, 0, 0, VAL_REQ,        0, NON_NEG, VAL_STR, 0 },
{ "" }
};

static readonly CLI_ENTRY growhash_qual[] = {
{ "ALL",    0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NON_NEG, VAL_N_A, 0 },
{ "REGION", 0, 0, 0, 0, 0, 0, VAL_REQ,        0, NON_NEG, VAL_STR, 0 },
{ "" }
};

static readonly CLI_ENTRY rehash_qual[] = {
{ "ALL",    0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NON_NEG, VAL_N_A, 0 },
{ "REGION", 0, 0, 0, 0, 0, 0, VAL_REQ,        0, NON_NEG, VAL_STR, 0 },
{ "" }
};

/* Main command table (cmd_ary is defined to this array in lke.c)
 * This table contains the names of all commands, and corresponding functions to be
 * dispatched to, and the qualifier sub-tables, containing all legal qualifiers..
 */
GBLDEF CLI_ENTRY lke_cmd_ary[] = {
{ "CLEAR",    lke_clear,    clear_qual,    0, 0, cli_disallow_lke_clear, 0, VAL_NOT_REQ,    0, 0, VAL_STR, 0},
{ "CLNUP",    lke_clean,    clnup_qual,    0, 0, 0,                      0, VAL_NOT_REQ,    0, 0, 0,       0},
{ "EXIT",     lke_exit,     0,             0, 0, 0,                      0, VAL_DISALLOWED, 0, 0, 0,       0},
{ "GROWHASH", lke_growhash, growhash_qual, 0, 0, 0,                      0, VAL_NOT_REQ,    0, 0, 0,       0},
{ "HELP",     util_help,    0,             0, 0, 0,                      0, VAL_NOT_REQ,    1, 0, 0,       0},
{ "REHASH",   lke_rehash,   rehash_qual,   0, 0, 0,                      0, VAL_NOT_REQ,    0, 0, 0,       0},
{ "SHOW",     lke_show,     show_qual,     0, 0, 0,                      0, VAL_NOT_REQ,    0, 0, VAL_STR, 0},
{ "SPAWN",    util_spawn,   0,             0, 0, 0,                      0, VAL_DISALLOWED, 0, 0, 0,       0},
{ "" }
};
