/****************************************************************
 *								*
 *	Copyright 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "cli.h"

static CLI_ENTRY mup_trig_cmd_qual[] = {
	{ "KILL",             0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NON_NEG, VAL_N_A, 0 },
	{ "SET",              0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NON_NEG, VAL_N_A, 0 },
	{ "ZKILL",            0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NON_NEG, VAL_N_A, 0 },
	{ "ZTKILL",           0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NON_NEG, VAL_N_A, 0 },
	{ "ZTRIGGER",         0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NON_NEG, VAL_N_A, 0 },
	{ "ZWITHDRAW",        0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NON_NEG, VAL_N_A, 0 },
	{ 0 }
};


static CLI_ENTRY mup_trig_option_qual[] = {
	{ "CONSISTENCYCHECK", 0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NEG,     VAL_N_A, 0 },
	{ "ISOLATION",        0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NEG,     VAL_N_A, 0 },
	{ 0 }
};

static 	CLI_ENTRY	triggerfile_ary[] = {
{ "COMMANDS",             0, 0, 0, mup_trig_cmd_qual,    0,                    0, VAL_REQ,	  0, NON_NEG, VAL_STR,  0 },
{ "DELIM",                0, 0, 0, 0,                    0,                    0, VAL_REQ,	  0, NON_NEG, VAL_STR,  0 },
{ "NAME",     	          0, 0, 0, 0,			 0,		       0, VAL_REQ,	  0, NON_NEG, VAL_STR,  0 },
{ "OPTIONS",              0, 0, 0, mup_trig_option_qual, 0,		       0, VAL_REQ,	  0, NON_NEG, VAL_STR,  0 },
{ "PIECES",               0, 0, 0, 0,                    0,                    0, VAL_REQ,	  0, NON_NEG, VAL_STR,  0 },
{ "XECUTE",               0, 0, 0, 0,                    0,                    0, VAL_REQ,	  0, NON_NEG, VAL_STR,  0 },
{ "ZDELIM",               0, 0, 0, 0,                    0,                    0, VAL_REQ,	  0, NON_NEG, VAL_STR,  0 },
{ 0 }
};

GBLDEF 	CLI_ENTRY	trigger_cmd_ary[] = {
{ "TRIGGER",   0, 0,   0,      triggerfile_ary, 0,   0, VAL_DISALLOWED, 0, 0, 0, 0 },
{ 0 }
};
