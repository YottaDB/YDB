/****************************************************************
 *								*
 * Copyright (c) 2018-2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
Each line in this file is of the following form

LYDBRTN(lydbtype,			simpleapi_rtnname,			simplethreadapi_rtnname			)

where
	lydbtype          -> Type of call to libyottadb (e.g. LYDB_RTN_GET implies a ydb_get_s() or ydb_get_st() call
	simpleapi_rtnname -> Name of routine that uses this lydbtype in SimpleAPI mode (i.e. when noThreadAPI_active == TRUE),
			  -> "********" if no routine uses this lydbtype in SimpleAPI mode
	simplethreadapi_rtnname -> Name of routine that uses this lydbtype in SimpleThreadAPI mode (i.e. when simpleThreadAPI_active == TRUE),
			  -> "********" if no routine uses this lydbtype in SimpleThreadAPI mode
*/

LYDBRTN(LYDB_RTN_NONE,			"********",				"********"				),	/* No libyottadb routine is running */
LYDBRTN(LYDB_RTN_DATA, 			"ydb_data_s()",				"ydb_data_st()"				),	/* "ydb_data_s" is running */
LYDBRTN(LYDB_RTN_DECODE,		"ydb_decode_s()",			"ydb_decode_st()"			),	/* "ydb_decode_s" is running */
LYDBRTN(LYDB_RTN_DELETE_EXCL, 		"ydb_delete_excl_s()",			"ydb_delete_excl_st()"			),	/* "ydb_delete_excl_s" is running */
LYDBRTN(LYDB_RTN_DELETE, 		"ydb_delete_s()",			"ydb_delete_st()"			),	/* "ydb_delete_s" is running */
LYDBRTN(LYDB_RTN_ENCODE,		"ydb_encode_s()",			"ydb_encode_st()"			),	/* "ydb_encode_s" is running */
LYDBRTN(LYDB_RTN_GET, 			"ydb_get_s()",				"ydb_get_st()"				),	/* "ydb_get_s" is running */
LYDBRTN(LYDB_RTN_INCR,			"ydb_incr_s()",				"ydb_incr_st()"				),	/* "ydb_incr_s" is running */
LYDBRTN(LYDB_RTN_LOCK, 			"ydb_lock_s()",				"ydb_lock_st()"				),	/* "ydb_lock_s" is running */
LYDBRTN(LYDB_RTN_LOCK_DECR, 		"ydb_lock_decr_s()",			"ydb_lock_decr_st()"			),	/* "ydb_lock_decr_s" is running */
LYDBRTN(LYDB_RTN_LOCK_INCR, 		"ydb_lock_incr_s()",			"ydb_lock_incr_st()"			),	/* "ydb_lock_incr_s" is running */
LYDBRTN(LYDB_RTN_NODE_NEXT, 		"ydb_node_next_s()",			"ydb_node_next_st()"			),	/* "ydb_node_next_s" is running */
LYDBRTN(LYDB_RTN_NODE_PREVIOUS, 	"ydb_node_previous_s()",		"ydb_node_previous_st()"		),	/* "ydb_node_previous_s" is running */
LYDBRTN(LYDB_RTN_SET, 			"ydb_set_s()",				"ydb_set_st()"				),	/* "ydb_set_s" is running */
LYDBRTN(LYDB_RTN_STR2ZWR,		"ydb_str2zwr_s()",			"ydb_str2zwr_st()"			),	/* "ydb_str2zwr_s" is running */
LYDBRTN(LYDB_RTN_SUBSCRIPT_NEXT, 	"ydb_subscript_next_s()",		"ydb_subscript_next_st()"		),	/* "ydb_subscript_next_s" is running */
LYDBRTN(LYDB_RTN_SUBSCRIPT_PREVIOUS, 	"ydb_subscript_previous_s()",		"ydb_subscript_previous_st()"		),	/* "ydb_subscript_previous_s" is running */
LYDBRTN(LYDB_RTN_TP,			"ydb_tp_s()",				"ydb_tp_st()"				),	/* "ydb_tp_s"  in SimpleAPI to start/commit TP; in SimpleThreadAPI starts TP */
LYDBRTN(LYDB_RTN_TP_START,		"********",				"ydb_tp_st()"				),	/* "ydb_tp_st" to do "op_tstart"    in SimpleThreadAPI for nested TP */
LYDBRTN(LYDB_RTN_TP_COMMIT,		"********",				"ydb_tp_st()"				),	/* "ydb_tp_st" to do "op_tcommit"   in SimpleThreadAPI for nested TP */
LYDBRTN(LYDB_RTN_TP_RESTART,		"********",				"ydb_tp_st()"				),	/* "ydb_tp_st" to do "tp_restart"   in SimpleThreadAPI for nested TP */
LYDBRTN(LYDB_RTN_TP_ROLLBACK,		"********",				"********"				),	/* Not needed but there as a counterpart for LYDB_RTN_TP_ROLLBACK_TLVL0 */
LYDBRTN(LYDB_RTN_TP_TLVL0,		"********",				"ydb_tp_st()"				),	/* "ydb_tp_st"                      in SimpleThreadAPI for outermost TP */
LYDBRTN(LYDB_RTN_TP_START_TLVL0,	"********",				"ydb_tp_st()"				),	/* "ydb_tp_st" to do "op_tstart"    in SimpleThreadAPI for outermost TP */
LYDBRTN(LYDB_RTN_TP_COMMIT_TLVL0,	"********",				"ydb_tp_st()"				),	/* "ydb_tp_st" to do "op_tcommit"   in SimpleThreadAPI for outermost TP */
LYDBRTN(LYDB_RTN_TP_RESTART_TLVL0,	"********",				"ydb_tp_st()"				),	/* "ydb_tp_st" to do "tp_restart"   in SimpleThreadAPI for outermost TP */
LYDBRTN(LYDB_RTN_TP_ROLLBACK_TLVL0,	"********",				"ydb_tp_st()"				),	/* "ydb_tp_st" to do "op_trollback" in SimpleThreadAPI for outermost TP */
LYDBRTN(LYDB_RTN_ZWR2STR,		"ydb_zwr2str_s()",			"ydb_zwr2str_st()"			),	/* "ydb_zwr2str_s" is running */
LYDBRTN(LYDB_RTN_CHILDINIT,		"ydb_child_init()",			"ydb_child_init()"			),	/* "ydb_child_init" is running */
LYDBRTN(LYDB_RTN_FILE_ID_FREE,		"ydb_file_id_free()",			"ydb_file_id_free_t()"			),	/* "ydb_file_id_free" is running */
LYDBRTN(LYDB_RTN_FILE_IS_IDENTICAL,	"ydb_file_is_identical()",		"ydb_file_is_identical_t()"		),	/* "ydb_file_is_identical is running */
LYDBRTN(LYDB_RTN_FILE_NAME_TO_ID,	"ydb_file_name_to_id()",		"ydb_file_name_to_id_t()"		),	/* "ydb_file_name_to_id" is running */
LYDBRTN(LYDB_RTN_FREE,			"ydb_free()",				"********"				),	/* "ydb_free" is running */
LYDBRTN(LYDB_RTN_HIBER_START,		"ydb_hiber_start()",			"********"				),	/* "ydb_hiber_start" is running */
LYDBRTN(LYDB_RTN_HIBER_START_ANY,	"ydb_hiber_start_any()",		"********"				),	/* "ydb_hiber_start_any" is running */
LYDBRTN(LYDB_RTN_MALLOC,		"ydb_malloc()",				"********"				),	/* "ydb_malloc" is running */
LYDBRTN(LYDB_RTN_MESSAGE,		"ydb_message()",			"ydb_message_t()"			),	/* "ydb_message" is running */
LYDBRTN(LYDB_RTN_STDIO_ADJUST,		"ydb_stdout_stderr_adjust()",		"ydb_stdout_stderr_adjust_t()"		),	/* "ydb_stdout_stderr_adjust" is running */
LYDBRTN(LYDB_RTN_TIMER_CANCEL,		"ydb_timer_cancel()",			"ydb_timer_cancel_t()"			),	/* "ydb_timer_cancel" is running */
LYDBRTN(LYDB_RTN_TIMER_START,		"ydb_timer_start()",			"ydb_timer_start_t()"			),	/* "ydb_timer_start" is running */
LYDBRTN(LYDB_RTN_YDB_CI,		"********",				"ydb_ci_t()"				),	/* "ydb_cip_helper" is running */
LYDBRTN(LYDB_RTN_YDB_CIP,		"********",				"ydb_cip_t()"				),	/* "ydb_cip_helper" is running */
LYDBRTN(LYDB_RTN_YDB_CI_TAB_OPEN,	"ydb_ci_tab_open()",			"ydb_ci_tab_open_t()"			),	/* "ydb_ci_tab_open" is running */
LYDBRTN(LYDB_RTN_YDB_CI_TAB_SWITCH,	"ydb_ci_tab_switch()",			"ydb_ci_tab_switch_t()"			),	/* "ydb_ci_tab_switch" is running */
LYDBRTN(LYDB_RTN_YDB_CI_GET_INFO,	"ydb_ci_get_info()",			"ydb_ci_get_info_t()"			),	/* "ydb_ci_get_info" is running */
LYDBRTN(LYDB_RTN_EINTR_HANDLER,		"ydb_eintr_handler()",			"ydb_eintr_handler_t()"			),	/* "ydb_eintr_handler" is running */
/* ydb_sig_dispatch() is called from (currently only) the Go wrapper but if this changes, this may need to be uncommented */
/*LYDBRTN(LYDB_RTN_YDB_SIG_DISPATCH,	"ydb_sig_dispatch()",			"ydb_sig_dispatch()"			),*/	/* "ydb_sig_dispatch() and signal handler are running */
