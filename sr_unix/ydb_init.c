/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2019 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stat.h"

#include <dlfcn.h>

#include "mv_stent.h"
#include "gtmci.h"
#include "gtmimagename.h"
#include "gtm_threadgbl_init.h"
#include "common_startup_init.h"
#include "invocation_mode.h"
#include "startup.h"
#include "gtm_startup.h"
#include "libyottadb_int.h"
#include "ydb_getenv.h"
#include "gtmsecshr.h"
#include "gtm_savetraps.h"
#include "gtm_permissions.h"
#ifdef UTF8_SUPPORTED
# include "gtm_icu_api.h"
# include "gtm_utf8.h"
# include "gtm_conv.h"
GBLREF	u_casemap_t 		gtm_strToTitle_ptr;		/* Function pointer for gtm_strToTitle */
#endif

GBLREF  stack_frame     	*frame_pointer;
GBLREF  unsigned char		*fgncal_stack;
GBLREF	int			fork_after_ydb_init;
GBLREF	boolean_t		noThreadAPI_active;
GBLREF	CLI_ENTRY		mumps_cmd_ary[];
GBLREF	int			mumps_status;
GBLREF	char			ydb_dist[YDB_PATH_MAX];
GBLREF	boolean_t		ydb_dist_ok_to_use;
GTMTRIG_DBG_ONLY(GBLREF ch_ret_type (*ch_at_trigger_init)();)

/* Initialization routine - can be called directly by call-in caller or can be driven by ydb_ci*() implicitly. But
 * if other YottaDB services are to be used prior to a ydb_ci*() call (like timers, gtm_malloc/free, etc), this routine
 * should be called first.
 */
int ydb_init()

{
	unsigned char   	*transfer_addr;
	char			*dist, *tmp_ptr;
	int			dist_len, tmp_len;
	char			path[YDB_PATH_MAX];
	int			path_len;
	int			save_errno;
	int			status;
	struct stat		stat_buf;
	char			file_perm[MAX_PERM_LEN];
	Dl_info			shlib_info;
	libyottadb_routines	save_active_stapi_rtn;
	ydb_buffer_t		*save_errstr;
	boolean_t		get_lock;
	boolean_t		error_encountered;
	DCL_THREADGBL_ACCESS;

	/* If "ydb_init" has already been done then we can skip the "ydb_init" call altogether. Return right away. */
	if (ydb_init_complete)
		return YDB_OK;
	SETUP_THREADGBL_ACCESS;	/* needed at least by SETUP_GENERIC_ERROR macro in case we go below that code path */
	/* Single thread the rest of initialization so all of the various not-thread-safe things this routine does in
	 * addition to initializing both memory and work thread mutexes in gtm_startup() are all completed without race
	 * conditions. Note our condition handler coverage differs depending on whether ydb has been initialized or not.
	 * In the most typical case, YDB will not yet be initialized so the gtmci_ch handler is not established until
	 * after the condition handling stack is setup below. But if YDB is initialized, we establish the handler before
	 * much of anything happens.
	 */
	THREADED_API_YDB_ENGINE_LOCK(YDB_NOTTP, NULL, LYDB_RTN_NONE, save_active_stapi_rtn, save_errstr, get_lock, status);
	if (0 != status)
	{	/* If not initialized yet, can't rts_error so just return our error code */
		assert(0 < status);	/* i.e. can only be a system error, not a YDB_ERR_* error code */
		if (ydb_init_complete)
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("pthread_mutex_lock()"),
				      CALLFROM, status);
		else
			FPRINTF(stderr, "%%YDB-E-SYSCALL, Error received from system call pthread_mutex_lock()"
				"-- called from module %s at line %d\n%%SYSTEM-E-ENO%d, %s\n",
				__FILE__, __LINE__, status, STRERROR(status));
		return ERR_SYSCALL;
	}
	if (NULL == lcl_gtm_threadgbl)
	{	/* This means the SETUP_THREADGBL_ACCESS done at the beginning of this function (before getting the
		 * "THREADED_API_YDB_ENGINE_LOCK" multi-thread lock) saw the global variable "gtm_threadgbl" as NULL.
		 * But it is possible we reach here after one thread has done the initialization and released the
		 * multi-thread lock. So redo the SETUP_THREADGBL_ACCESS.
		 */
		SETUP_THREADGBL_ACCESS;
	}
	if (NULL == lcl_gtm_threadgbl)
	{	/* A NULL value of "lcl_gtm_threagbl" here means "gtm_threadgbl" is still NULL.
		 * This means no GTM_THREADGBL_INIT or "ydb_init" has happened yet in this process. Take care of both below.
		 * Note: This will likely need some attention before going to a fully threaded model
		 */
		assert(!ydb_init_complete);
		GTM_THREADGBL_INIT;
	}
	if (fork_after_ydb_init)
	{	/* This process was created by a "fork" from a parent process that had done YottaDB engine calls.
		 * Do some checks for error scenarios.
		 */
		assert(!ydb_init_complete);	/* should have been cleared by "ydb_stm_atfork_child" */
		assert(simpleThreadAPI_active || noThreadAPI_active);
		if (simpleThreadAPI_active)
		{	/* SimpleThreadAPI was active in the parent process before the "fork". This means the parent
			 * was multi-threaded and so after a "fork", the YottaDB state of the child (current) process
			 * is not consistent (inherent issue with fork in a multi-threaded process) as a concurrently running
			 * thread could be in the middle of YottaDB runtime logic at the time of the "fork" which means the
			 * global variables capturing YottaDB state are not in a clean state in the child process.
			 * It is a mess to clean up this and start afresh in the child process so we disallow YottaDB engine
			 * calls after a "fork". We therefore expected an "exec" to be done in the child process before any
			 * YottaDB calls are made. That clearly did not happen. So issue an error.
			 *
			 * Note that SETUP_GENERIC_ERROR macro is usable here even though "ydb_init_complete" is FALSE
			 * because "ydb_init" was already run in this process.
			 */
			SETUP_GENERIC_ERROR(ERR_STAPIFORKEXEC);	/* ensure a later call to "ydb_zstatus" returns full error string */
			THREADED_API_YDB_ENGINE_UNLOCK(YDB_NOTTP, NULL, save_active_stapi_rtn, save_errstr, get_lock);
			/* "ydb_init" returns positive error code so return ERR_STAPIFORKEXEC, not YDB_ERR_STAPIFORKEXEC. */
			return ERR_STAPIFORKEXEC;
		} else
		{	/* SimpleAPI was active in the parent process before the "fork". We expect the first YottaDB call in
			 * the child process to be "ydb_child_init". But the fact that "fork_after_ydb_init" is non-zero
			 * implies that call did not happen. Handle that first before proceeding. "ydb_stm_atfork_child" would
			 * have reset "ydb_init_complete" to FALSE even though YottaDB engine has been initialized. Undo that
			 * change before calling "ydb_child_init".
			 */
			assert(!ydb_init_complete);
			ydb_init_complete = TRUE;
			status = ydb_child_init(NULL);
			if (YDB_OK != status)
			{
				assert(0 > status);	/* "ydb_child_init" returns negated error status. */
				ydb_init_complete = FALSE;	/* Force "ydb_init" to be invoked again in case any more
								 * YottaDB calls happen since "ydb_child_init" did not run clean.
								 */
				THREADED_API_YDB_ENGINE_UNLOCK(YDB_NOTTP, NULL, save_active_stapi_rtn, save_errstr, get_lock);
				/* "ydb_init" returns positive error code hence the negate below */
				return -status;		/* Negate it back before returning from "ydb_init". */
			}
		}
	}
	if (process_exiting)
	{	/* A prior invocation of ydb_exit() would have set process_exiting = TRUE.
		 * Use this to disallow any more YottaDB calls (including "ydb_init").
		 * Note that because "ydb_init" was already run (or else "process_exiting" can never be set to TRUE),
		 * SETUP_GENERIC_ERROR is usable here even though "ydb_init_complete" is FALSE.
		 */
		assert(!ydb_init_complete);	/* should have been cleared by "ydb_exit" as part of calling "gtm_exit_handler" */
		SETUP_GENERIC_ERROR(ERR_CALLINAFTERXIT); /* ensure a later call to "ydb_zstatus" returns full error string */
		THREADED_API_YDB_ENGINE_UNLOCK(YDB_NOTTP, NULL, save_active_stapi_rtn, save_errstr, get_lock);
		 /* "ydb_init" returns positive error code so return ERR_CALLINAFTERXIT, not YDB_ERR_CALLINAFTERXIT. */
		return ERR_CALLINAFTERXIT;
	}
	if (!ydb_init_complete)
	{	/* Call-in or SimpleAPI or SimpleThreadAPI invoked from C as base.
		 * YottaDB hasn't been started up yet. Start it now.
		 */
		if (NULL == (dist = ydb_getenv(YDBENVINDX_DIST_ONLY, NULL_SUFFIX, NULL_IS_YDB_ENV_MATCH)))
		{	/* In a call-in and "ydb_dist" env var is not defined. Set it to full path of libyottadb.so
			 * that contains the currently invoked "ydb_init" function.
			 */
			if (0 == dladdr(&ydb_init, &shlib_info))
			{	/* Could not find "ydb_init" symbol (current running function) in any loaded shared libraries.
				 * Issue error.
				 */
				FPRINTF(stderr, "%%YDB-E-SYSCALL, Error received from system call dladdr()"
					"-- called from module %s at line %d\n%s\n", __FILE__, __LINE__, dlerror());
				THREADED_API_YDB_ENGINE_UNLOCK(YDB_NOTTP, NULL, save_active_stapi_rtn, save_errstr, get_lock);
				return ERR_SYSCALL;
			}
			/* Temporarily copy shlib_info.dli_fname onto a local buffer as we cannot modify the former
			 * and we need to do that to remove the "/libyottadb.so" trailer before setting "$ydb_dist".
			 */
			tmp_ptr = path;
			tmp_len = STRLEN(shlib_info.dli_fname);
			assert(tmp_len);
			if (tmp_len >= SIZEOF(path))
			{
				FPRINTF(stderr, "%%YDB-E-DISTPATHMAX, Executable path length is greater than maximum (%d)\n",
													YDB_DIST_PATH_MAX);
				THREADED_API_YDB_ENGINE_UNLOCK(YDB_NOTTP, NULL, save_active_stapi_rtn, save_errstr, get_lock);
				/* "ydb_init" returns positive error code so return ERR_DISTPATHMAX, not YDB_ERR_DISTPATHMAX. */
				return ERR_DISTPATHMAX;
			}
			memcpy(tmp_ptr, shlib_info.dli_fname, tmp_len);
			tmp_ptr += tmp_len - 1;
			/* Remove trailing "/libyottadb.so" from filename before setting $ydb_dist.
			 * Note: The later call to "common_startup_init" will ensure the filename is libyottadb.so.
			 */
			for (; tmp_ptr >= path; tmp_ptr--)
			{
				if ('/' == *tmp_ptr)
					break;
			}
			*tmp_ptr = '\0';
			/* Note that we still have not checked if the executable name is libyottadb.so.
			 * We will do that a little later in "common_startup_init" below (after setting <ydb_dist>
			 * env var) and issue a LIBYOTTAMISMTCH error if needed.
			 */
			status = setenv(ydbenvname[YDBENVINDX_DIST] + 1, path, TRUE);	/* + 1 to skip leading $ */
			if (status)
			{
				assert(-1 == status);
				save_errno = errno;
				FPRINTF(stderr, "%%YDB-E-SYSCALL, Error received from system call setenv(ydb_dist)"
					"-- called from module %s at line %d\n%%SYSTEM-E-ENO%d, %s\n",
					__FILE__, __LINE__, save_errno, STRERROR(save_errno));
				THREADED_API_YDB_ENGINE_UNLOCK(YDB_NOTTP, NULL, save_active_stapi_rtn, save_errstr, get_lock);
				return ERR_SYSCALL;
			}
			status = setenv(gtmenvname[YDBENVINDX_DIST] + 1, path, TRUE);	/* + 1 to skip leading $ */
			if (status)
			{
				assert(-1 == status);
				save_errno = errno;
				FPRINTF(stderr, "%%YDB-E-SYSCALL, Error received from system call setenv(gtm_dist)"
					"-- called from module %s at line %d\n%%SYSTEM-E-ENO%d, %s\n",
					__FILE__, __LINE__, save_errno, STRERROR(save_errno));
				THREADED_API_YDB_ENGINE_UNLOCK(YDB_NOTTP, NULL, save_active_stapi_rtn, save_errstr, get_lock);
				return ERR_SYSCALL;
			}
		} /* else : "ydb_dist" env var is defined. Use that for later verification done inside "common_startup_init" */
		common_startup_init(GTM_IMAGE, &mumps_cmd_ary[0]);
		err_init(stop_image_conditional_core);
		ESTABLISH_NORET(gtmci_ch, error_encountered);
		if (error_encountered)
		{	/* "gtmci_ch" encountered an error and transferred control back here. Return after mutex lock cleanup. */
			THREADED_API_YDB_ENGINE_UNLOCK(YDB_NOTTP, NULL, save_active_stapi_rtn, save_errstr, get_lock);
			REVERT;
			/* "ydb_init" returns positive error code so return mumps_status as is (i.e. no negation for YDB_ERR_*) */
			assert(0 < mumps_status);
			return mumps_status;
		}
		/* Now that a condition handler has been established, it is safe to use "rts_error_csa" going forward.
		 * Note that any errors below would invoke "rts_error_csa" which will transfer control to "gtmci_ch" which
		 * will return through the ESTABLISH_NORET macro above with "error_encountered" set to a non-zero value
		 * and so it is enough to do the ydb_engine mutex unlock there instead of before each "rts_error_csa" below.
		 */
		UTF8_ONLY(gtm_strToTitle_ptr = &gtm_strToTitle);
		GTM_ICU_INIT_IF_NEEDED;	/* Note: should be invoked after err_init (since it may error out) and before CLI parsing */
		/* Ensure that $ydb_dist exists */
		if (NULL == (dist = ydb_getenv(YDBENVINDX_DIST_ONLY, NULL_SUFFIX, NULL_IS_YDB_ENV_MATCH)))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_YDBDISTUNDEF);
		/* Ensure that $ydb_dist is non-zero and does not exceed YDB_DIST_PATH_MAX */
		dist_len = STRLEN(dist);
		if (!dist_len)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_YDBDISTUNDEF);
		else if (YDB_DIST_PATH_MAX <= dist_len)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_DISTPATHMAX, 1, YDB_DIST_PATH_MAX);
		/* Verify that $ydb_dist/gtmsecshr is available with setuid root */
		memcpy(path, ydb_dist, dist_len);
		path[dist_len] =  '/';
		memcpy(path + dist_len + 1, GTMSECSHR_EXECUTABLE, STRLEN(GTMSECSHR_EXECUTABLE));
		path_len = dist_len + 1 + STRLEN(GTMSECSHR_EXECUTABLE);
		assertpro(YDB_PATH_MAX > path_len);
		path[path_len] = '\0';
		if (-1 == Stat(path, &stat_buf))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
				      LEN_AND_LIT("stat for $ydb_dist/gtmsecshr"), CALLFROM, errno);
		/* Ensure that the call-in can execute $ydb_dist/gtmsecshr. This not sufficient for security purposes */
		if ((ROOTUID != stat_buf.st_uid) || !(stat_buf.st_mode & S_ISUID))
		{
			SNPRINTF(file_perm, SIZEOF(file_perm), "%04o", stat_buf.st_mode & PERMALL);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_GTMSECSHRPERM, 5,
					path_len, path,
					RTS_ERROR_STRING(file_perm), stat_buf.st_uid);
		} else
		{	/* $ydb_dist validated */
			ydb_dist_ok_to_use = TRUE;
			memcpy(ydb_dist, dist, dist_len);
		}
		cli_lex_setup(0, NULL);
		/* Initialize msp to the maximum so if errors occur during YottaDB startup below,
		 * the unwind logic in "gtmci_ch" will get rid of the whole stack.
		 */
		msp = (unsigned char *)-1L;
		GTMTRIG_DBG_ONLY(ch_at_trigger_init = &mdb_condition_handler);
		invocation_mode = MUMPS_CALLIN;
		init_gtm();			/* Note - this initializes fgncal_stackbase and creates the call-in
						 * base-frame for the initial level.
						 */
		assert(ydb_init_complete);
		gtm_savetraps(); 		/* Nullify default $ZTRAP handling */
		assert(IS_VALID_IMAGE && (n_image_types > image_type));	/* assert image_type is initialized */
		assert(frame_pointer->type & SFT_CI);
		TREF(gtmci_nested_level) = 1;
		TREF(libyottadb_active_rtn) = LYDB_RTN_NONE;
		/* Now that YottaDB is initialized. Mark the new stack pointer (msp) so that errors
		 * while executing an M routine do not unwind stack below this mark. It important that
		 * the call-in frames (SFT_CI) that hold nesting information (eg. $ECODE/$STACK data
		 * of the previous stack) are kept from being unwound.
		 */
		SAVE_FGNCAL_STACK;
		REVERT;
	}
	assert(NULL == TREF(temp_fgncal_stack));
	THREADED_API_YDB_ENGINE_UNLOCK(YDB_NOTTP, NULL, save_active_stapi_rtn, save_errstr, get_lock);
	return YDB_OK;
}
