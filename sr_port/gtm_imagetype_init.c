/****************************************************************
 *								*
 *	Copyright 2010, 2013 Fidelity Information Services, Inc	*
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

#include "gtmimagename.h"
#include "gtm_imagetype_init.h"

GBLREF	boolean_t		skip_dbtriggers;
GBLREF	boolean_t		is_replicator;
GBLREF	boolean_t		run_time;
GBLREF	boolean_t		write_after_image;
GBLREF	boolean_t		dse_running;
GBLREF	enum gtmImageTypes	image_type;
#ifdef UNIX
GBLREF	boolean_t		jnlpool_init_needed;
GBLREF	boolean_t 		span_nodes_disallowed;
GBLREF	char			gtm_dist[GTM_PATH_MAX];
#endif

void	gtm_imagetype_init(enum gtmImageTypes img_type)
{
	boolean_t		is_svc_or_gtcm;
	char			*dist;
	int			len = 0;

	NON_GTMTRIG_ONLY(skip_dbtriggers = TRUE;) /* Do not invoke triggers for trigger non-supporting platforms. */
	UNIX_ONLY(span_nodes_disallowed = (GTCM_GNP_SERVER_IMAGE == img_type) || (GTCM_SERVER_IMAGE == img_type);)
	is_svc_or_gtcm = ((GTM_SVC_DAL_IMAGE == img_type)
				|| (GTCM_GNP_SERVER_IMAGE == img_type)
				|| (GTCM_SERVER_IMAGE == img_type));
	if (is_svc_or_gtcm)
		skip_dbtriggers = TRUE; /* SUN RPC DAL server and GT.CM OMI and GNP servers do not invoke triggers */
	if (is_svc_or_gtcm || (GTM_IMAGE == img_type))
	{
		is_replicator = TRUE; /* can go through t_end() and write jnl records to the jnlpool for replicated db */
		run_time = TRUE;
	} else if (DSE_IMAGE == img_type)
	{
		dse_running = TRUE;
		write_after_image = TRUE; /* if block change is done, after image of the block needs to be written */
	}
#	ifdef UNIX
	else if (MUPIP_IMAGE == img_type)
		run_time = FALSE;
	/* GT.M typically opens journal pool during the first update (in gvcst_put, gvcst_kill or op_ztrigger). But, if
	 * anticipatory freeze is enabled, we want to open journal pool for any reads done by GT.M as well (basically at the time
	 * of first database open (in gvcst_init). So, set jnlpool_init_needed to TRUE if this is GTM_IMAGE.
	 */
	jnlpool_init_needed = (GTM_IMAGE == img_type);
	/* Read gtm_dist here and use this value everywhere else */
	dist = (char *)GETENV(GTM_DIST);
	if (dist)
		len = STRLEN(dist);
	if(len)
	{
		memcpy(gtm_dist, dist, ((len > GTM_PATH_MAX) ? GTM_PATH_MAX : len));
		gtm_dist[GTM_PATH_MAX - 1] = '\0';
	}
	else
		gtm_dist[0] = '\0';
#	endif
	image_type = img_type;
	return;
}
