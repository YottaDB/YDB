/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include <efndef.h>

enum efn_names
{
	efn_bg_qio_read = 3,	/* According to the Programming Concepts Manual section on "Using Event Flags",
				 *    event flag 0 is the default flag used by system routines.
				 *    event flag 1 to 23 may be used in system routines.
				 *    event flag 24 to 31 is reserved for Compaq/HP use only.
				 *    event flags in cluster #1 i.e. flag # 32 to 63 are Local and Available for general use.
				 * However, we have not (yet) noticed any issues with our event flag usage. We'll stick with
				 * cluster 0 usage until we've had a chance to cleanup event flag usage
				 */
	efn_bg_qio_write,
	efn_ignore_obsolete,
	efn_immed_wait,		/* to be used whenever we are NOT in an AST and are going to WAIT */
	efn_op_job,
	efn_outofband,
	efn_timer,
	efn_sys_wait,
        efn_2timer,             /* used by gt.cm cmi */
	efn_jnl,
        efn_iott_write,
        efn_cmi_immed_wait,
        efn_cmi_mbx,
	efn_timer_ast,		/* to be used whenever we are in an AST and are going to WAIT */
	efn_hiber_start,	/* used by hiber_start when not in an AST */
	efn_ignore = EFN$C_ENF
};
