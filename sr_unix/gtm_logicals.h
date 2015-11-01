/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* gtm_logicals.h - Environment variables. */
/* Needs gtm_stdio.h */

#define	DEFAULT_GTM_TMP			P_tmpdir
#define	GTM_DEBUG_LEVEL_ENVLOG		"$gtmdbglvl"
#define	GTM_FULLBLOCKWRITES		"$gtm_fullblockwrites"
#define	GTM_LOG_ENV			"$gtm_log"
#define	GTM_PRINCIPAL			"$gtm_principal"
#define	GTM_PRINCIPAL_EDITING		"$gtm_principal_editing"
#define	GTM_SHMFLAGS			"$gtm_shmatflags"
#define	GTM_TMP_ENV			"$gtm_tmp"
#define	GTM_TPRESTART_LOG_DELTA		"$gtm_tprestart_log_delta"
#define	GTM_TPRESTART_LOG_LIMIT		"$gtm_tprestart_log_first"
#define	GTM_ZINTERRUPT			"$gtm_zinterrupt"
#define	SYSID				"$gtm_sysid"
#define	ZCOMPILE			"$gtmcompile"
#define	ZDATE_FORM			"$gtm_zdate_form"
#define	ZGBLDIR				"$gtmgbldir"
#define	ZGTMENVXLATE			"$gtm_env_translate"
#define	ZREPLINSTANCE			"$gtm_repl_instance"
#define	ZROUTINES			"$gtmroutines"
#define	ZTRAP_FORM			"$gtm_ztrap_form"
#define	ZTRAP_NEW			"$gtm_ztrap_new"
#define	ZYERROR				"$gtm_zyerror"
#define	DISABLE_ALIGN_STRINGS		"$gtm_disable_alignstr"
#define GTM_MAX_SOCKETS			"$gtm_max_sockets"
#define GTM_QUIET_HALT			"$gtm_quiet_halt"

/* Environment variables for Unicode functionality */
#define GTM_CHSET_ENV			"$gtm_chset"
#define GTM_PATNUMERIC_ENV		"$gtm_patnumeric"
#define GTM_BADCHAR_ENV			"$gtm_badchar"
#define GTM_ICU_MINOR_ENV		"$gtm_icu_minorver"

#define	GTM_GVDUPSETNOOP		"$gtm_gvdupsetnoop"
#define	GTM_GDSCERT			"$gtm_gdscert"
#define	GTM_BLKUPGRADE_FLAG		"$gtm_blkupgrade_flag"

#define	GTM_WHITE_BOX_TEST_CASE_ENABLE	"$gtm_white_box_test_case_enable"
#define	GTM_WHITE_BOX_TEST_CASE_NUMBER	"$gtm_white_box_test_case_number"
#define	GTM_WHITE_BOX_TEST_CASE_COUNT	"$gtm_white_box_test_case_count"

#define	GTM_DBFILEXT_SYSLOG_DISABLE	"$gtm_dbfilext_syslog_disable"

#define	GTM_REPL_INSTNAME		"$gtm_repl_instname"
#define	GTM_REPL_INSTSECONDARY		"$gtm_repl_instsecondary"
