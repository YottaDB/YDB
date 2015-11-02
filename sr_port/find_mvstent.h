/****************************************************************
 *                                                              *
 *      Copyright 2011 Fidelity Information Services, Inc 	*
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#ifndef FIND_MVSTENT_H
#define FIND_MVSTENT_H
mv_stent *find_mvstent_cmd(zintcmd_ops match_command, unsigned char *match_restart_pc, unsigned char *match_restart_ctxt,
	boolean_t clear_mvstent);

/* Keep track of active interrupted commands in global to avoid unneeded searches of stack */
typedef struct
{
        int     count;          /* number of active MVST_ZINTCMD entries for this command */
        unsigned char   *restart_pc_last;       /* most recent on MVST stack */
        unsigned char   *restart_ctxt_last;
} zintcmd_active_info;
#endif
