/****************************************************************
 *								*
 *	Copyright 2002, 2003 Sanchez Computer Associates, Inc.	*
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
boolean_t cli_disallow_mupip_replicate(void);
boolean_t cli_disallow_mupip_replic_receive(void);
boolean_t cli_disallow_mupip_replic_source(void);
boolean_t cli_disallow_mupip_rundown(void);
boolean_t cli_disallow_mupip_set(void);

#endif
