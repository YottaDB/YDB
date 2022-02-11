/****************************************************************
 *								*
 * Copyright (c) 2018-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 * Below is a list of all environment variable names recognized by YottaDB and their corresponding GT.M env var names.
 *
 * 1) All YottaDB environment variables start with a ydb_ prefix and are lower-case (even if their corresponding
 *    GT.M env var name is upper case).
 * 2) The ydbenvindx column defines an enum which is an index into the ydbenvname[] and gtmenvname[] tables that
 *    in turn record the corresponding ydb* and gtm* environment variables names (ydbenvname and gtmenvname columns below).
 * 3) The following rules apply
 *      a) If the ydb* env var is not defined, but the gtm* env var is defined, the ydb* env var is also defined
 *          to have the same value as the gtm* env var the first time the gtm* env var is read in the code flow.
 *       b) Note though that if the gtm* env var is not defined, but the ydb* env var is defined,
 *          the gtm* env var is NOT defined.
 *       c) If the ydb* env var and the gtm* env vars are both defined, the ydb* env var value takes precedence.
 *          The gtm* env var value is ignored.
 *
 * YDBENVINDX_TABLE_ENTRY(ydbenvindx,                          ydbenvname,                        gtmenvname)
 */

YDBENVINDX_TABLE_ENTRY (YDBENVINDX_MIN_INDEX,                  "",                                "")

YDBENVINDX_TABLE_ENTRY (YDBENVINDX_AIO_NR_EVENTS,              "$ydb_aio_nr_events",              "$gtm_aio_nr_events")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_APP_ENSURES_ISOLATION,      "$ydb_app_ensures_isolation",      "")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_AUTORELINK_CTLMAX,          "$ydb_autorelink_ctlmax",          "$gtm_autorelink_ctlmax")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_AUTORELINK_KEEPRTN,         "$ydb_autorelink_keeprtn",         "$gtm_autorelink_keeprtn")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_AUTORELINK_SHM,             "$ydb_autorelink_shm",             "$gtm_autorelink_shm")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_BADCHAR,                    "$ydb_badchar",                    "$gtm_badchar")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_BAKTMPDIR,                  "$ydb_baktmpdir",                  "$gtm_baktmpdir")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_BAKTMPDIR_UC,               "$ydb_baktmpdir",                  "$GTM_BAKTMPDIR")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_BLKUPGRADE_FLAG,            "$ydb_blkupgrade_flag",            "$gtm_blkupgrade_flag")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_BOOLEAN,                    "$ydb_boolean",                    "$gtm_boolean")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_CALLIN_START,               "$ydb_callin_start",               "$GTM_CALLIN_START")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_CHSET,                      "$ydb_chset",                      "$gtm_chset")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_CHSET_LOCALE,               "$ydb_chset_locale",               "$gtm_chset_locale")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_CI,                         "$ydb_ci",                         "$GTMCI")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_CM_PREFIX,                  "$ydb_cm_",                        "$GTCM_")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_COLLATE_PREFIX,             "$ydb_collate_",                   "$gtm_collate_")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_COMPILE,                    "$ydb_compile",                    "$gtmcompile")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_COREDUMP_FILTER,            "$ydb_coredump_filter",            "$gtm_coredump_filter")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_CRYPT_CONFIG,               "$ydb_crypt_config",               "$gtmcrypt_config")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_CRYPT_FIPS,                 "$ydb_crypt_fips",                 "$gtmcrypt_FIPS")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_CRYPT_PLUGIN,               "$ydb_crypt_plugin",               "$gtm_crypt_plugin")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_CUSTOM_ERRORS,              "$ydb_custom_errors",              "$gtm_custom_errors")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_DBFILEXT_SYSLOG_DISABLE,    "$ydb_dbfilext_syslog_disable",    "$gtm_dbfilext_syslog_disable")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_DBGLVL,                     "$ydb_dbglvl",                     "$gtmdbglvl")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_DB_STARTUP_MAX_WAIT,        "$ydb_db_startup_max_wait",        "$gtm_db_startup_max_wait")
/* Note there are 2 lines for "$ydb_dist" below. One for callers which are guaranteed that "$ydb_dist" is set at
 * image startup. These are almost all the executables in YottaDB (mumps, mupip, dse, etc.) all of which go through
 * "dlopen_libyottadb" at startup. They need to only look at "ydb_dist" and so they will use the YDBENVINDX_DIST_ONLY
 * entry. There are a few exceptions though (e.g. encryption plugin) that are built separately and don't go through
 * the "dlopen_libyottadb" initialization. Those need to look at "$ydb_dist" and "$gtm_dist" in that order and will
 * therefore use the YDBENVINDX_DIST entry.
 */
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_DIST,                       "$ydb_dist",                       "$gtm_dist")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_DIST_ONLY,                  "$ydb_dist",                       "")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_DMTERM,                     "$ydb_dmterm",                     "$gtm_dmterm")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_DONT_TAG_UTF8_ASCII,        "$ydb_dont_tag_utf8_ascii",        "$gtm_dont_tag_UTF8_ASCII")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_DTNDBD,                     "$ydb_dtndbd",                     "$gtmdtndbd")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_ENVIRONMENT_INIT,           "$ydb_environment_init",           "$gtm_environment_init")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_ENV_TRANSLATE,              "$ydb_env_translate",              "$gtm_env_translate")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_ERROR_ON_JNL_FILE_LOST,     "$ydb_error_on_jnl_file_lost",     "$gtm_error_on_jnl_file_lost")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_ETRAP,                      "$ydb_etrap",                      "$gtm_etrap")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_EXTRACT_NOCOL,              "$ydb_extract_nocol",              "$gtm_extract_nocol")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_GBLDIR,                     "$ydb_gbldir",                     "$gtmgbldir")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_GBLDIR_TRANSLATE,           "$ydb_gbldir_translate",           "")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_GDSCERT,                    "$ydb_gdscert",                    "$gtm_gdscert")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_HUPENABLE,		       "$ydb_hupenable",		  "$gtm_hupenable")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_ICU_VERSION,                "$ydb_icu_version",                "$gtm_icu_version")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_IPV4_ONLY,                  "$ydb_ipv4_only",                  "$gtm_ipv4_only")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_JNL_RELEASE_TIMEOUT,        "$ydb_jnl_release_timeout",        "$gtm_jnl_release_timeout")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_LCT_STDNULL,                "$ydb_lct_stdnull",                "$gtm_lct_stdnull")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_LINK,                       "$ydb_link",                       "$gtm_link")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_LINKTMPDIR,                 "$ydb_linktmpdir",                 "$gtm_linktmpdir")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_LOCALE,                     "$ydb_locale",                     "$gtm_locale")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_LOCAL_COLLATE,              "$ydb_local_collate",              "$gtm_local_collate")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_LOG,                        "$ydb_log",                        "$gtm_log")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_LVNULLSUBS,                 "$ydb_lvnullsubs",                 "$gtm_lvnullsubs")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_MAXTPTIME,                  "$ydb_maxtptime",                  "$gtm_zmaxtptime")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_MAX_INDRCACHE_COUNT,        "$ydb_max_indrcache_count",        "$gtm_max_indrcache_count")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_MAX_INDRCACHE_MEMORY,       "$ydb_max_indrcache_memory",       "$gtm_max_indrcache_memory")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_MAX_SOCKETS,                "$ydb_max_sockets",                "$gtm_max_sockets")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_MAX_STORALLOC,              "$ydb_max_storalloc",              "$gtm_max_storalloc")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_MEMORY_RESERVE,             "$ydb_memory_reserve",             "$gtm_memory_reserve")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_MSGPREFIX,                  "$ydb_msgprefix",                  "")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_MSTACK_CRIT_THRESHOLD,      "$ydb_mstack_crit_threshold",      "$gtm_mstack_crit_threshold")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_MSTACK_SIZE,                "$ydb_mstack_size",                "$gtm_mstack_size")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_MUPJNL_PARALLEL,            "$ydb_mupjnl_parallel",            "$gtm_mupjnl_parallel")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_NOCENABLE,                  "$ydb_nocenable",                  "$gtm_nocenable")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_NOFFLF,		       "$ydb_nofflf",			  "$gtm_nofflf")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_NONTPRESTART_LOG_DELTA,     "$ydb_nontprestart_log_delta",     "$gtm_nontprestart_log_delta")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_NONTPRESTART_LOG_FIRST,     "$ydb_nontprestart_log_first",     "$gtm_nontprestart_log_first")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_NON_BLOCKED_WRITE_RETRIES,  "$ydb_non_blocked_write_retries",  "$gtm_non_blocked_write_retries")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_NOUNDEF,                    "$ydb_noundef",                    "$gtm_noundef")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_OBFUSCATION_KEY,            "$ydb_obfuscation_key",            "$gtm_obfuscation_key")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_PASSWD,                     "$ydb_passwd",                     "$gtm_passwd")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_PATNUMERIC,                 "$ydb_patnumeric",                 "$gtm_patnumeric")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_PATTERN_FILE,               "$ydb_pattern_file",               "$gtm_pattern_file")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_PATTERN_TABLE,              "$ydb_pattern_table",              "$gtm_pattern_table")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_POOLLIMIT,                  "$ydb_poollimit",                  "$gtm_poollimit")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_PRINCIPAL,                  "$ydb_principal",                  "$gtm_principal")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_PRINCIPAL_EDITING,          "$ydb_principal_editing",          "$gtm_principal_editing")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_PROCSTUCKEXEC,              "$ydb_procstuckexec",              "$gtm_procstuckexec")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_PROMPT,                     "$ydb_prompt",                     "$gtm_prompt")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_QUIET_HALT,                 "$ydb_quiet_halt",                 "$gtm_quiet_halt")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_RECOMPILE_NEWER_SRC,        "$ydb_recompile_newer_src",        "")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_REPL_FILTER_TIMEOUT,        "$ydb_repl_filter_timeout",        "$gtm_repl_filter_timeout")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_REPL_INSTANCE,              "$ydb_repl_instance",              "$gtm_repl_instance")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_REPL_INSTNAME,              "$ydb_repl_instname",              "$gtm_repl_instname")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_REPL_INSTSECONDARY,         "$ydb_repl_instsecondary",         "$gtm_repl_instsecondary")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_ROUTINES,                   "$ydb_routines",                   "$gtmroutines")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_SIDE_EFFECTS,               "$ydb_side_effects",               "$gtm_side_effects")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_SOCKET_KEEPALIVE_IDLE,      "$ydb_socket_keepalive_idle",      "$gtm_socket_keepalive_idle")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_SNAPTMPDIR,                 "$ydb_snaptmpdir",                 "$gtm_snaptmpdir")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_STATSDIR,                   "$ydb_statsdir",                   "$gtm_statsdir")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_STATSHARE,                  "$ydb_statshare",                  "$gtm_statshare")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_STDXKILL,                   "$ydb_stdxkill",                   "$gtm_stdxkill")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_STRING_POOL_LIMIT,          "$ydb_string_pool_limit",          "$gtm_string_pool_limit")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_SYSID,                      "$ydb_sysid",                      "$gtm_sysid")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_TREAT_SIGUSR2_LIKE_SIGUSR1, "$ydb_treat_sigusr2_like_sigusr1", "")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_TLS_PASSWD_PREFIX,          "$ydb_tls_passwd_",                "$gtmtls_passwd_")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_DOLLAR_TEST,                "$ydb_dollartest",                 "")
/* Like YDBENVINDX_DIST and YDBENVINDX_DIST_ONLY, we have the below two lines.
 * YDBENVINDX_TMP is used by the wrapper gtmsecshr ($ydb_dist/gtmsecshr). But since that sets ydb_tmp in the environment,
 * the gtmsecshr process that is forked off ($ydb_dist/gtmsecshrdir/gtmsecshr) inherits this env var and so does not need
 * to look at gtm_tmp env var at all. Therefore it uses the YDBENVINDX_TMP_ONLY entry (which has no "$gtm_tmp").
 */
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_TMP,                        "$ydb_tmp",                        "$gtm_tmp")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_TMP_ONLY,                   "$ydb_tmp",                        "")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_TPNOTACIDTIME,              "$ydb_tpnotacidtime",              "$gtm_tpnotacidtime")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_TPRESTART_LOG_DELTA,        "$ydb_tprestart_log_delta",        "$gtm_tprestart_log_delta")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_TPRESTART_LOG_FIRST,        "$ydb_tprestart_log_first",        "$gtm_tprestart_log_first")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_TRACE_GBL_NAME,             "$ydb_trace_gbl_name",             "$gtm_trace_gbl_name")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_TRACE_GROUPS,               "$ydb_trace_groups",               "$gtm_trace_groups")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_TRACE_TABLE_SIZE,           "$ydb_trace_table_size",           "$gtm_trace_table_size")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_TRIGGER_ETRAP,              "$ydb_trigger_etrap",              "$gtm_trigger_etrap")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_UTFCGR_STRINGS,             "$ydb_utfcgr_strings",             "$gtm_utfcgr_strings")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_UTFCGR_STRING_GROUPS,       "$ydb_utfcgr_string_groups",       "$gtm_utfcgr_string_groups")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_XC,                         "$ydb_xc",                         "$GTMXC")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_XC_PREFIX,                  "$ydb_xc_",                        "$GTMXC_")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_ZDATE_FORM,                 "$ydb_zdate_form",                 "$gtm_zdate_form")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_ZINTERRUPT,                 "$ydb_zinterrupt",                 "$gtm_zinterrupt")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_ZLIB_CMP_LEVEL,             "$ydb_zlib_cmp_level",             "$gtm_zlib_cmp_level")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_ZQUIT_ANYWAY,               "$ydb_zquit_anyway",               "$gtm_zquit_anyway")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_ZSTEP,                      "$ydb_zstep",                      "$gtm_zstep")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_ZTRAP_FORM,                 "$ydb_ztrap_form",                 "$gtm_ztrap_form")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_ZTRAP_NEW,                  "$ydb_ztrap_new",                  "$gtm_ztrap_new")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_ZYERROR,                    "$ydb_zyerror",                    "$gtm_zyerror")

/* The below are currently unused/commented-out in the code. */
/* YDBENVINDX_TABLE_ENTRY (YDBENVINDX_DISABLE_ALIGNSTR,           "$ydb_disable_alignstr",           "$gtm_disable_alignstr") */

/* The below is a list of non-YottaDB-specific environment variables that are relied upon by YottaDB */
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_GENERIC_EDITOR,             "$EDITOR",                         "")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_GENERIC_GNUPGHOME,          "$GNUPGHOME",                      "")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_GENERIC_HOME,               "$HOME",                           "")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_GENERIC_PATH,               "$PATH",                           "")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_GENERIC_SHELL,              "$SHELL",                          "")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_GENERIC_TERM,               "$TERM",                           "")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_GENERIC_USER,               "$USER",                           "")

/* The below is a list of environment variable names that are recognized only by debug builds of YottaDB */
#ifdef DEBUG
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_DBGFLAGS,                   "$ydb_dbgflags",                   "$gtmdbgflags")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_DBGFLAGS_FREQ,              "$ydb_dbgflags_freq",              "$gtmdbgflags_freq")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_DB_COUNTER_SEM_INCR,        "$ydb_db_counter_sem_incr",        "$gtm_db_counter_sem_incr")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_DIRTREE_COLLHDR_ALWAYS,     "$ydb_dirtree_collhdr_always",     "$gtm_dirtree_collhdr_always")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_GVUNDEF_FATAL,              "$ydb_gvundef_fatal",              "$gtm_gvundef_fatal")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_LOCKHASH_N_BITS,            "$ydb_lockhash_n_bits",            "")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_TEST_AUTORELINK_ALWAYS,     "$ydb_test_autorelink_always",     "$gtm_test_autorelink_always")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_TEST_FAKE_ENOSPC,           "$ydb_test_fake_enospc",           "$gtm_test_fake_enospc")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_TEST_JNLPOOL_SYNC,          "$ydb_test_jnlpool_sync",          "$gtm_test_jnlpool_sync")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_USESECSHR,                  "$ydb_usesecshr",                  "$gtm_usesecshr")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_WHITE_BOX_TEST_CASE_COUNT,  "$ydb_white_box_test_case_count",  "$gtm_white_box_test_case_count")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_WHITE_BOX_TEST_CASE_ENABLE, "$ydb_white_box_test_case_enable", "$gtm_white_box_test_case_enable")
YDBENVINDX_TABLE_ENTRY (YDBENVINDX_WHITE_BOX_TEST_CASE_NUMBER, "$ydb_white_box_test_case_number", "$gtm_white_box_test_case_number")
#endif
