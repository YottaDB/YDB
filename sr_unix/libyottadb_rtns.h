/****************************************************************
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

LYDBRTN(LYDB_RTN_NONE,			LYDB_NONE,		"none"),			/* No libyottadb routine is running */
LYDBRTN(LYDB_RTN_CHILDINIT,		LYDB_UTILITY,		"ydb_child_init()"),		/* "ydb_child_init" is running */
LYDBRTN(LYDB_RTN_DATA, 			LYDB_SIMPLEAPI,		"ydb_data_s()"),		/* "ydb_data_s" is running */
LYDBRTN(LYDB_RTN_DELETE, 		LYDB_SIMPLEAPI,		"ydb_delete_s()"),		/* "ydb_delete_s" is running */
LYDBRTN(LYDB_RTN_DELETE_EXCL, 		LYDB_SIMPLEAPI,		"ydb_delete_excl_s()"),		/* "ydb_delete_excl_s" is running */
LYDBRTN(LYDB_RTN_GET, 			LYDB_SIMPLEAPI,		"ydb_get_s()"),			/* "ydb_get_s" is running */
LYDBRTN(LYDB_RTN_INCR,			LYDB_SIMPLEAPI,		"ydb_incr_s()"),		/* "ydb_incr_s" is running */
LYDBRTN(LYDB_RTN_LOCK, 			LYDB_SIMPLEAPI,		"ydb_lock_s()"),		/* "ydb_lock_s" is running */
LYDBRTN(LYDB_RTN_LOCK_DECR, 		LYDB_SIMPLEAPI,		"ydb_lock_decr_s()"),		/* "ydb_lock_decr_s" is running */
LYDBRTN(LYDB_RTN_LOCK_INCR, 		LYDB_SIMPLEAPI,		"ydb_lock_incr_s()"),		/* "ydb_lock_incr_s" is running */
LYDBRTN(LYDB_RTN_NODE_NEXT, 		LYDB_SIMPLEAPI,		"ydb_node_next_s()"),		/* "ydb_node_next_s" is running */
LYDBRTN(LYDB_RTN_NODE_PREVIOUS, 	LYDB_SIMPLEAPI,		"ydb_node_previous_s()"),	/* "ydb_node_previous_s" is running */
LYDBRTN(LYDB_RTN_SET, 			LYDB_SIMPLEAPI,		"ydb_set_s()"),			/* "ydb_set_s" is running */
LYDBRTN(LYDB_RTN_STR2ZWR,		LYDB_SIMPLEAPI,		"ydb_str2zwr_s()"),		/* "ydb_str2zwr_s" is running */
LYDBRTN(LYDB_RTN_SUBSCRIPT_NEXT, 	LYDB_SIMPLEAPI,		"ydb_subscript_next_s()"),	/* "ydb_subscript_next_s" is running */
LYDBRTN(LYDB_RTN_SUBSCRIPT_PREVIOUS, 	LYDB_SIMPLEAPI,		"ydb_subscript_previous_s()"),	/* "ydb_subscript_previous_s" is running */
LYDBRTN(LYDB_RTN_TP,			LYDB_SIMPLEAPI,		"ydb_tp_s()"),			/* "ydb_tp_s" is running */
LYDBRTN(LYDB_RTN_ZWR2STR,		LYDB_SIMPLEAPI,		"ydb_zwr2str_s()")		/* "ydb_zwr2str_s" is running */
