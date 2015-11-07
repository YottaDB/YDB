/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Phases the GT.M compiler can be in */
enum CGP_PHASE
{
	CGP_NOSTATE = 0,	/* 0 - compiler not running */
	CGP_PARSE,		/* 1 - compiler initialized - parsing into triples */
	CGP_RESOLVE,		/* 2 - resolve triple references to each other */
	CGP_APPROX_ADDR,	/* 3 - approximate addresses with pseudo-code-gen */
	CGP_ADDR_OPT,		/* 4 - address optimization and triple reduction */
	CGP_ASSEMBLY,		/* 5 - generate assembler listing */
	CGP_MACHINE,		/* 6 - generate machine code */
	CGP_FINI,		/* 7 - compile complete - cleanup */
	CGP_MAXSTATE
};
