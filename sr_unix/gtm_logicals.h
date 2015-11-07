/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* gtm_logicals.h - Environment variables used by GT.M. */
/* within each group, the entries are in alpha order of the third column */
/* -------------------------- Common to Unix and VMS --------------------------  */

#define	GTM_DIST_LOG			"$gtm_dist"

/* Database */
#define	GTM_GBLDIR			"$gtmgbldir"
#define	GTM_BLKUPGRADE_FLAG		"$gtm_blkupgrade_flag"
#define	GTM_DBFILEXT_SYSLOG_DISABLE	"$gtm_dbfilext_syslog_disable"
#define	GTM_ENV_XLATE			"$gtm_env_translate"
#define	GTM_FULLBLOCKWRITES		"$gtm_fullblockwrites"
#define	GTM_GDSCERT			"$gtm_gdscert"
#define	GTM_GVDUPSETNOOP		"$gtm_gvdupsetnoop"
#define	GTM_GVUNDEF_FATAL		"$gtm_gvundef_fatal"
#define	GTM_TP_ALLOCATION_CLUE		"$gtm_tp_allocation_clue"
#define	GTM_TPNOTACIDTIME		"$gtm_tpnotacidtime"
#define	GTM_TPRESTART_LOG_DELTA		"$gtm_tprestart_log_delta"
#define	GTM_TPRESTART_LOG_LIMIT		"$gtm_tprestart_log_first"
#define	GTM_ZMAXTPTIME			"$gtm_zmaxtptime"

/* White-box testing */
#define	GTM_WHITE_BOX_TEST_CASE_COUNT	"$gtm_white_box_test_case_count"
#define	GTM_WHITE_BOX_TEST_CASE_ENABLE	"$gtm_white_box_test_case_enable"
#define	GTM_WHITE_BOX_TEST_CASE_NUMBER	"$gtm_white_box_test_case_number"

/* Indirection-cache */
#define	GTM_MAX_INDRCACHE_COUNT		"$gtm_max_indrcache_count"
#define	GTM_MAX_INDRCACHE_MEMORY	"$gtm_max_indrcache_memory"

/* MUPIP BACKUP */
#define	GTM_BAK_TEMPDIR_LOG_NAME	"$gtm_baktmpdir"
#define	GTM_BAK_TEMPDIR_LOG_NAME_UC	"$GTM_BAKTMPDIR"

/* Pattern match operator */
#define	PAT_FILE			"$gtm_pattern_file"
#define	PAT_TABLE			"$gtm_pattern_table"

/* Alternative Collation */
#define	CT_PREFIX			"$gtm_collate_"
#define	LCT_PREFIX			"$gtm_local_collate"
#define	LCT_STDNULL			"$gtm_lct_stdnull"

/* GTM processing versus M standard */
/* (see gtm_local_collate above) */
#define GTM_STDXKILL			"$gtm_stdxkill"

/* Miscellaneous */
#define	ZCOMPILE			"$gtmcompile"
#define	GTM_DEBUG_LEVEL_ENVLOG		"$gtmdbglvl"
#define	GTM_ZROUTINES			"$gtmroutines"
#define	GTM_BOOLEAN			"$gtm_boolean"
#define	DISABLE_ALIGN_STRINGS		"$gtm_disable_alignstr"
#define	GTM_MAX_SOCKETS			"$gtm_max_sockets"
#define	GTM_MEMORY_RESERVE		"$gtm_memory_reserve"
#define	GTM_NOUNDEF			"$gtm_noundef"
#define	GTM_PRINCIPAL			"$gtm_principal"
#define	GTM_PROMPT			"$gtm_prompt"
#define	GTM_SIDE_EFFECT			"$gtm_side_effects"
#define	SYSID				"$gtm_sysid"
#define	GTM_MPROF_TESTING		"$gtm_trace_gbl_name"
#define	GTM_TRACE_GROUPS		"$gtm_trace_groups"
#define	GTM_TRACE_TABLE_SIZE		"$gtm_trace_table_size"
#define	ZDATE_FORM			"$gtm_zdate_form"
#define	GTM_ZINTERRUPT			"$gtm_zinterrupt"
#define	GTM_ZQUIT_ANYWAY		"$gtm_zquit_anyway"
#define	ZTRAP_FORM			"$gtm_ztrap_form"
#define	ZTRAP_NEW			"$gtm_ztrap_new"
#define	ZYERROR				"$gtm_zyerror"

/* -------------------------- Unix only --------------------------  */

/* Database */
#define	GTM_TMP_ENV			"$gtm_tmp"
#define	GTM_SHMFLAGS			"$gtm_shmatflags"
#define	GTM_TRIGGER_ETRAP		"$gtm_trigger_etrap"
#define	GTM_SNAPTMPDIR			"$gtm_snaptmpdir"
#define	GTM_DB_STARTUP_MAX_WAIT		"$gtm_db_startup_max_wait"

/* Replication */
#define	GTM_REPL_INSTANCE		"$gtm_repl_instance"
#define	GTM_REPL_INSTNAME		"$gtm_repl_instname"
#define	GTM_REPL_INSTSECONDARY		"$gtm_repl_instsecondary"
#define	GTM_ZLIB_CMP_LEVEL		"$gtm_zlib_cmp_level"
#define	GTM_EVENT_LOG_LIB_ENV		"$gtm_event_log_libpath"
#define	GTM_EVENT_LOG_RTN_ENV		"$gtm_event_log_rtn"
#define	GTM_JNL_RELEASE_TIMEOUT		"$gtm_jnl_release_timeout"
#define	GTM_CUSTOM_ERRORS		"$gtm_custom_errors"

/* Unicode */
#define	GTM_CHSET_ENV			"$gtm_chset"
#ifdef __MVS__
#define	GTM_CHSET_LOCALE_ENV		"$gtm_chset_locale"
#define	GTM_TAG_UTF8_AS_ASCII		"$gtm_dont_tag_UTF8_ASCII"
#endif
#define	GTM_PATNUMERIC_ENV		"$gtm_patnumeric"
#define	GTM_BADCHAR_ENV			"$gtm_badchar"
#define	GTM_ICU_VERSION			"$gtm_icu_version"

/* Miscellaneous */
#define GTM_ERROR_ON_JNL_FILE_LOST	"$gtm_error_on_jnl_file_lost"
#define GTM_ETRAP			"$gtm_etrap"
#define	GTM_LOG_ENV			"$gtm_log"
#define	GTM_LVNULLSUBS			"$gtm_lvnullsubs"
#define	GTM_NOCENABLE			"$gtm_nocenable"
#define	GTM_NON_BLOCKED_WRITE_RETRIES	"$gtm_non_blocked_write_retries"
#define	GTM_PRINCIPAL_EDITING		"$gtm_principal_editing"
#define	GTM_PROCSTUCKEXEC		"$gtm_procstuckexec"
#define	GTM_QUIET_HALT			"$gtm_quiet_halt"
#define	GTM_EXTRACT_NOCOL		"$gtm_extract_nocol"
#define	GTMDBGFLAGS			"$gtmdbgflags"
#define	GTMDBGFLAGS_FREQ		"$gtmdbgflags_freq"
#define GTM_MAX_STORALLOC		"$gtm_max_storalloc"
#define GTM_IPV4_ONLY			"$gtm_ipv4_only"
