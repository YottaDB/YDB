/****************************************************************
 *								*
 *	Copyright 2002, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __MUPIP_CMD_DISALLOW_H__
#define __MUPIP_CMD_DISALLOW_H__

boolean_t cli_disallow_mupip_backup(void);
boolean_t cli_disallow_mupip_freeze(void);
boolean_t cli_disallow_mupip_integ(void);
boolean_t cli_disallow_mupip_journal(void);
boolean_t cli_disallow_mupip_reorg(void);
boolean_t cli_disallow_mupip_replicate(void);
boolean_t cli_disallow_mupip_replic_editinst(void);
boolean_t cli_disallow_mupip_replic_receive(void);
boolean_t cli_disallow_mupip_replic_source(void);
boolean_t cli_disallow_mupip_replic_updhelper(void);
boolean_t cli_disallow_mupip_rundown(void);
boolean_t cli_disallow_mupip_set(void);
boolean_t cli_disallow_mupip_trigger(void);
boolean_t cli_disallow_mupip_size(void);
boolean_t cli_disallow_mupip_size_heuristic(void);
#endif
