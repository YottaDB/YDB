/****************************************************************
 *								*
 * Copyright (c) 2019-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef YDB_ENCODE_DECODE_INCLUDED
#define YDB_ENCODE_DECODE_INCLUDED

#include <jansson.h>

/* ydb_decode_s.c internal declarations */
int	decode_json_object(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, int max_subs,
			json_t *jansson_object, ydb_var_types decode_type, int decode_svn_index, void *handle);
int	decode_json_array(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, int max_subs,
			json_t *jansson_object, ydb_var_types decode_type, int decode_svn_index, void *handle);
int	decode_json_string(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, json_t *jansson_object,
			ydb_var_types decode_type, int decode_svn_index, void *handle);
int	decode_json_integer(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, json_t *jansson_object,
			ydb_var_types decode_type, int decode_svn_index, void *handle);
int	decode_json_real(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, json_t *jansson_object,
			ydb_var_types decode_type, int decode_svn_index, void *handle);
int	decode_json_bool(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, ydb_var_types decode_type,
			int decode_svn_index, int bool_type);

/* ydb_encode_s.c internal declarations */
int	encode_tree(const ydb_buffer_t *varname, int subs_used, const ydb_buffer_t *subsarray, ydb_var_types encode_type,
			int encode_svn_index, unsigned int data_value, json_t *obj, int *ret_subs_used,
			ydb_buffer_t *ret_subsarray, void *handle);

boolean_t	is_integer(ydb_buffer_t buff, long long *value);
boolean_t	is_real(ydb_buffer_t buff, double *value);
boolean_t	is_true(ydb_buffer_t buff);
boolean_t	is_false(ydb_buffer_t buff);
boolean_t	is_null(ydb_buffer_t buff);
boolean_t	is_direct_child_of(int subs_used, const ydb_buffer_t *subsarray, int next_subs_used, ydb_buffer_t *next_subsarray);
boolean_t	is_descendant_of(int subs_used, const ydb_buffer_t *subsarray, int next_subs_used, ydb_buffer_t *next_subsarray);

#endif /* YDB_ENCODE_DECODE_INCLUDED */
