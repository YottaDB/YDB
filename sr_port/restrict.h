/****************************************************************
 *								*
 * Copyright (c) 2017-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#define RESTRICTED(FACILITY)											\
		(!restrict_initialized ? (restrict_init(), restrictions.FACILITY) : restrictions.FACILITY)

#define COMM_FILTER_FILENAME     	"filter_commands.tab"
#define PIPE_C_CALL_NAME		"gtmpipeopen"
#define ZSY_C_CALL_NAME			"gtmzsystem"

struct restrict_facilities
{
	boolean_t	break_op;
	boolean_t	zbreak_op;
	boolean_t	zedit_op;
	boolean_t	zsystem_op;
	boolean_t	pipe_open;
	boolean_t	trigger_mod;
	boolean_t	cenable;
	boolean_t	dse;
	boolean_t	dmode;
	boolean_t	zcmdline;
	boolean_t	halt_op;
	boolean_t	zhalt_op;
	boolean_t	zsy_filter;
	boolean_t	pipe_filter;
};

GBLREF	struct restrict_facilities	restrictions;
GBLREF	boolean_t			restrict_initialized;

void restrict_init(void);
