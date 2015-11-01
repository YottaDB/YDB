/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* gtm_logicals.h - Environment variables. */
/* Needs gtm_stdio.h */

#define ZCOMPILE 	"$gtmcompile"
#define ZROUTINES 	"$gtmroutines"
#define ZGBLDIR 	"$gtmgbldir"
#define ZYERROR		"$gtm_zyerror"
#define ZTRAP_FORM	"$gtm_ztrap_form"
#define ZTRAP_NEW	"$gtm_ztrap_new"
#define ZREPLINSTANCE	"$gtm_repl_instance"
#define ZGTMENVXLATE	"$gtm_env_translate"

#define GTM_TMP_ENV		"$gtm_tmp"
#define GTM_LOG_ENV		"$gtm_log"
#define DEFAULT_GTM_TMP		P_tmpdir

#define	GTM_TPRESTART_LOG_LIMIT		"$gtm_tprestart_log_first"
#define	GTM_TPRESTART_LOG_DELTA		"$gtm_tprestart_log_delta"
