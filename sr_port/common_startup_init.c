/****************************************************************
 *								*
 * Copyright (c) 2014-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_limits.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"

#include "gt_timer.h"
#include "gtm_env_init.h"
#include "get_page_size.h"
#include "getjobnum.h"
#include "gtmimagename.h"
#include "gtm_utf8.h"
#include "min_max.h"
#include "common_startup_init.h"
#include "deferred_events.h"

GBLREF	boolean_t		skip_dbtriggers;
GBLREF	boolean_t		is_replicator;
GBLREF	boolean_t		run_time;
GBLREF	boolean_t		write_after_image;
GBLREF	boolean_t		dse_running;
GBLREF	enum gtmImageTypes	image_type;
GBLREF	boolean_t		jnlpool_init_needed;
GBLREF	boolean_t 		span_nodes_disallowed;
GBLREF	char			gtm_dist[GTM_PATH_MAX];
GBLREF	unsigned int		gtm_dist_len;

void	common_startup_init(enum gtmImageTypes img_type)
{
	boolean_t		is_gtcm;
	char			*dist;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* First set the global variable image_type. */
	image_type = img_type;
	/* Get the process ID. */
	getjobnum();
	/* Get the OS page size. */
	get_page_size();
	/* Read gtm_dist. */
	if (NULL != (dist = GETENV(GTM_DIST)))
	{
		gtm_dist_len = strnlen(dist, PATH_MAX);
		assert(GTM_PATH_MAX > gtm_dist_len);
		memcpy(gtm_dist, dist, gtm_dist_len);
		gtm_dist[gtm_dist_len] = '\0';
	} else
		gtm_dist[0] = '\0';
	/* Setup global variables corresponding to signal blocks. */
	set_blocksig();
	/* Do common environment initialization. */
	gtm_env_init();
	/* GT.M typically opens journal pool during the first update (in gvcst_put, gvcst_kill or op_ztrigger). But, if
	 * anticipatory freeze is enabled, we want to open journal pool for any reads done by GT.M as well (basically at the time
	 * of first database open (in gvcst_init). So, set jnlpool_init_needed to TRUE if this is GTM_IMAGE.
	 */
	jnlpool_init_needed = (GTM_IMAGE == img_type);
	/* Set gtm_wcswidth_fnptr for util_output to work correctly in UTF-8 mode. This is needed only for utilities that can
	 * operate on UTF-8 mode.
	 */
	/* these attempts to keep the gtmsecshr link from dragging lots of stuff in need investigation */
	gtm_wcswidth_fnptr = (GTMSECSHR_IMAGE == img_type) ? NULL : gtm_wcswidth;
	xfer_set_handlers_fnptr =  (GTMSECSHR_IMAGE == img_type) ? NULL : xfer_set_handlers;
	NON_GTMTRIG_ONLY(skip_dbtriggers = TRUE;) /* Do not invoke triggers for trigger non-supporting platforms. */
	span_nodes_disallowed = (GTCM_GNP_SERVER_IMAGE == img_type) || (GTCM_SERVER_IMAGE == img_type);
	is_gtcm = ((GTCM_GNP_SERVER_IMAGE == img_type) || (GTCM_SERVER_IMAGE == img_type));
	if (is_gtcm)
		skip_dbtriggers = TRUE; /* GT.CM OMI and GNP servers do not invoke triggers */
	if (is_gtcm || (GTM_IMAGE == img_type))
	{
		is_replicator = TRUE; /* can go through t_end() and write jnl records to the jnlpool for replicated db */
		TREF(ok_to_see_statsdb_regs) = TRUE;
		run_time = TRUE;
	} else if (DSE_IMAGE == img_type)
	{
		dse_running = TRUE;
		write_after_image = TRUE; /* if block change is done, after image of the block needs to be written */
		TREF(ok_to_see_statsdb_regs) = TRUE;
	} else if (MUPIP_IMAGE == img_type)
	{
		run_time = FALSE;
		TREF(ok_to_see_statsdb_regs) = FALSE;	/* In general, MUPIP commands should not even see the statsdb regions.
							 * Specific MUPIP commands will override this later.
							 */
	}
	return;
}
