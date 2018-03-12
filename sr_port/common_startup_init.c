/****************************************************************
 *								*
 * Copyright (c) 2014-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2018 YottaDB LLC. and/or its subsidiaries.*
 * All rights reserved.						*
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

#include <dlfcn.h>
#include <errno.h>

#include "gt_timer.h"
#include "gtm_env_init.h"
#include "get_page_size.h"
#include "getjobnum.h"
#include "gtmimagename.h"
#include "gtm_utf8.h"
#include "min_max.h"
#include "common_startup_init.h"
#include "cli.h"
#include "gdsroot.h"
#include "is_file_identical.h"
#include "gtm_logicals.h"

GBLREF	boolean_t		skip_dbtriggers;
GBLREF	boolean_t		is_replicator;
GBLREF	boolean_t		run_time;
GBLREF	boolean_t		write_after_image;
GBLREF	boolean_t		dse_running;
GBLREF	enum gtmImageTypes	image_type;
GBLREF	boolean_t		jnlpool_init_needed;
GBLREF	boolean_t 		span_nodes_disallowed;
GBLREF	char			ydb_dist[YDB_PATH_MAX];
GBLREF	CLI_ENTRY		*cmd_ary;	/* Pointer to command table for MUMPS/DSE/LKE etc. */

error_def(ERR_MIXIMAGE);

void	common_startup_init(enum gtmImageTypes img_type, CLI_ENTRY *image_cmd_ary)
{
	boolean_t	is_gtcm;
	boolean_t	status;
	char		*dist;
	int		len;
	int		nbytes;
	int		ret_value;
	int		save_errno;
	Dl_info		shlib_info;
	char		comparison[YDB_PATH_MAX], *envptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (image_type)
	{	/* An image is already loaded. This is possible if a C function that did call-ins
		 * (and hence has access to the symbols that libyottadb.so exposes) invokes "dlopen_libyottadb"
		 * more than once for different image types. Do not allow multiple images to be loaded.
		 */
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MIXIMAGE, 4, GTMIMAGENAMETXT(img_type), GTMIMAGENAMETXT(image_type));
		assert(FALSE);
	}
	/* At this point, $ydb_dist should be set by "dlopen_libyottadb" (mumps/mupip/dse etc.) or "ydb_init" (call_ins).
	 * Read the env var "ydb_dist" and set the "ydb_dist" buffer based on that.
	 */
	if (NULL == (dist = GETENV(YDB_DIST)))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_YDBDISTUNDEF);
	len = STRLEN(dist);
	len = (YDB_PATH_MAX < len) ? YDB_PATH_MAX : len;
	memcpy(ydb_dist, dist, len);
	len = MIN(len, PATH_MAX);
	ydb_dist[len] = '\0';
	if (GTMSECSHR_IMAGE != img_type)
	{
		/* Check that the currently running libyottadb.so points to $ydb_dist. If not, it is a possible
		 * rogue C program that directly invoked one of "gtm_main","mupip_main","dse_main" etc. for
		 * more than one libyottadb.so (potentially different builds/releases of YottaDB) in the same
		 * process, a situation we do not want to get into. Issue an error in that case.
		 */
		if (0 == dladdr(&common_startup_init, &shlib_info))
		{	/* Could not find "common_startup_init" symbol in a shared library. Issue error. */
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("dladdr"), CALLFROM);
		}
		nbytes = SNPRINTF(comparison, YDB_PATH_MAX, LIBYOTTADBDOTSO, ydb_dist);
		if ((0 > nbytes) || (nbytes >= YDB_PATH_MAX))
		{	/* Error return from SNPRINTF */
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_DISTPATHMAX, 1, YDB_DIST_PATH_MAX);
		}
		status = is_file_identical((char *)shlib_info.dli_fname, comparison);
		if (!status)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_LIBYOTTAMISMTCH, 4,
				LEN_AND_STR(comparison), LEN_AND_STR(shlib_info.dli_fname));
	}
	/* else : In gtmsecshr, there is no dlopen of "libyottadb.so" hence above check is skipped.
	 *        Also, it has its own "ydb_dist" checks.
	 */
	cmd_ary = image_cmd_ary;	/* Define cmd_ary to point to IMAGE-specific cmd table */
	/* First set the global variable image_type. */
	image_type = img_type;
	/* Get the process ID. */
	getjobnum();
	/* Get the OS page size. */
	get_page_size();
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
	gtm_wcswidth_fnptr = (GTMSECSHR_IMAGE == img_type) ? NULL : gtm_wcswidth;
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
	/* Check if ydb_gbldir env var is set.  If so set gtmgbldir env var to the same value.
	 * If ydb_gbldir env var is not set and if gtmgbldir env var is set, set ydb_gbldir to gtmgbldir.
	 * That way
	 *	a) ydb_gbldir overrides gtmgbldir
	 *	b) "dpzgbini" and "zgbldir" functions in this process can use YDB_GBLDIR (instead of GTM_GBLDIR) AND
	 *	c) Child processes see both env vars set to the same value too.
	 */
	if (NULL != (envptr = GETENV(YDB_GBLDIR + 1)))	/* + 1 to get past '$' this call doesn't use */
	{	/* ydb_gbldir env var is set. Set gtmgbldir env var to same value. */
		ret_value = setenv(GTM_GBLDIR + 1, envptr, TRUE);
		if (ret_value)
		{
			assert(-1 == ret_value);
			save_errno = errno;
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
				RTS_ERROR_LITERAL("setenv(gtmgbldir)"), CALLFROM, save_errno);
		}
	} else if (NULL != (envptr = GETENV(GTM_GBLDIR + 1)))
	{	/* gtmgbldir env var is set. Set ydb_gbldir env var to same value. */
		ret_value = setenv(YDB_GBLDIR + 1, envptr, TRUE);
		if (ret_value)
		{
			assert(-1 == ret_value);
			save_errno = errno;
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
				RTS_ERROR_LITERAL("setenv(ydb_gbldir)"), CALLFROM, save_errno);
		}
	}
	/* else: YDB_GBLDIR and GTM_GBLDIR are both undefined (and therefore in sync with each other) */
	/* At this point, the env vars YDB_GBLDIR and GTM_GBLDIR are guaranteed to be equal to each other */
	return;
}

