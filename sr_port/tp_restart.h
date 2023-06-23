/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef TP_RESTART_H_INCLUDED
#define TP_RESTART_H_INCLUDED

/* The following codes define the states "tp_restart" can be run in */
#define TPRESTART_STATE_NORMAL	0	/* This is the normal way tp_restart is entered when not in a trigger */
#define TPRESTART_STATE_TPUNW	1	/* A trigger base frame was detected in tp_unwind */
#define TPRESTART_STATE_MSTKUNW	2	/* A trigger or callin base frame was detected in tp_restart's stack frame unwind code */

#define	TP_RESTART_HANDLES_ERRORS	TRUE

#define	TPWRAP_HELPER_MAX_ATTEMPTS	16	/* maximum # of iterations allowed to avoid indefinite tp restart loop */
#define RESTART_CODE_EXPANSION_FACTOR	(CDB_MAX_TRIES * SIZEOF("0x5;"))	/* need 0xn: where n is <CTRL> of 1 or 2 digits, get
										 *  5 from"0x5:" because of <NUL> term
										 */

/* Helper function */
void	op_trestart_set_cdb_code(void);

int tp_restart(int newlevel, boolean_t handle_errors_internally);

#endif
