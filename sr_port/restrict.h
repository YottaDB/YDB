/****************************************************************
 *								*
 * Copyright (c) 2017-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#define RESTRICTED(FACILITY)								\
		(!restrict_initialized ? (restrict_init(), restrictions.FACILITY) : restrictions.FACILITY)
#define COMM_FILTER_FILENAME     	"filter_commands.tab"
#define PIPE_C_CALL_NAME		"gtmpipeopen"
#define ZSY_C_CALL_NAME			"gtmzsystem"
/* Bit definitions for the dm_audit_enable restriction facility */
#define	AUDIT_DISABLE			0x0000		/* Disables Direct Mode Auditing */
#define	AUDIT_ENABLE_DMODE		0x0001		/* Enables auditing for direct mode */
#define	AUDIT_ENABLE_RDMODE		0x0002		/* Enables auditing for all READ (op_read)*/
#define AUDIT_ENABLE_DMRDMODE		0x0003		/* (AUDIT_ENABLE_DMODE | AUDIT_ENABLE_RDMODE) */

struct restrict_facilities
{						/* Restriction(s) added in version */
	boolean_t	break_op;		/* V6.3-002 */
	boolean_t	zbreak_op;
	boolean_t	zedit_op;
	boolean_t	zsystem_op;
	boolean_t	pipe_open;
	boolean_t	trigger_mod;
	boolean_t	cenable;
	boolean_t	dse;
	boolean_t	lkeclear;
	boolean_t	lke;
	boolean_t	dmode;
	boolean_t	zcmdline;
	boolean_t	halt_op;		/* V6.3-003 */
	boolean_t	zhalt_op;
	boolean_t	zsy_filter;		/* V6.3-006 */
	boolean_t	pipe_filter;
	boolean_t	library_load_path;	/* V6.3-007 */
	boolean_t	mupip_enable;		/* V7.0-004 */
	uint4		dm_audit_enable;
	boolean_t	logdenials;		/* V6.3-012 */
};
GBLREF	struct restrict_facilities	restrictions;
GBLREF	boolean_t			restrict_initialized;
void restrict_init(void);
