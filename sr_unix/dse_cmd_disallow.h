/****************************************************************
 *								*
 *	Copyright 2003, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __DSE_CMD_DISALLOW_H__
#define __DSE_CMD_DISALLOW_H__

boolean_t cli_disallow_dse_add(void);
boolean_t cli_disallow_dse_all(void);
boolean_t cli_disallow_dse_cache(void);
boolean_t cli_disallow_dse_change(void);
boolean_t cli_disallow_dse_chng_fhead(void);
boolean_t cli_disallow_dse_crit(void);
boolean_t cli_disallow_dse_dump(void);
boolean_t cli_disallow_dse_find(void);
boolean_t cli_disallow_dse_maps(void);
boolean_t cli_disallow_dse_remove(void);
boolean_t cli_disallow_dse_save(void);
boolean_t cli_disallow_dse_shift(void);

#endif
