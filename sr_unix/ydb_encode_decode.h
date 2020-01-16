/****************************************************************
*								*
* Copyright (c) 2019-2020 YottaDB LLC and/or its subsidiaries.	*
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

typedef enum {
    JSON_OBJECT,
    JSON_ARRAY,
    JSON_STRING,
    JSON_INTEGER,
    JSON_REAL,
    JSON_TRUE,
    JSON_FALSE,
    JSON_NULL
} json_type;

typedef struct json_t {
    json_type type;
    volatile size_t refcount;
} json_t;

#define JSON_ERROR_TEXT_LENGTH   160
#define JSON_ERROR_SOURCE_LENGTH 80

typedef struct json_error_t {
    int line;
    int column;
    int position;
    char source[JSON_ERROR_SOURCE_LENGTH];
    char text[JSON_ERROR_TEXT_LENGTH];
} json_error_t;

int decode_json_object(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, int max_subs_usable,
	json_t *jansson_object, ydb_var_types decode_type, int decode_svn_index);
int decode_json_array(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, int max_subs_usable,
	json_t *jansson_object, ydb_var_types decode_type, int decode_svn_index);
int decode_json_string(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, json_t *jansson_object,
	ydb_var_types decode_type, int decode_svn_index);
int decode_json_integer(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, json_t *jansson_object,
	ydb_var_types decode_type, int decode_svn_index);
int decode_json_real(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, json_t *jansson_object,
	ydb_var_types decode_type, int decode_svn_index);
int decode_json_bool(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, json_t *jansson_object,
	ydb_var_types decode_type, int decode_svn_index, int bool_type);
boolean_t is_integer(ydb_buffer_t buff, long long *value);
boolean_t is_real(ydb_buffer_t buff, double *value);
boolean_t is_true(ydb_buffer_t buff);
boolean_t is_false(ydb_buffer_t buff);
boolean_t is_null(ydb_buffer_t buff);
boolean_t is_direct_child_of(int subs_used, const ydb_buffer_t *subsarray, int next_subs_used, ydb_buffer_t *next_subsarray);
boolean_t is_descendent_of(int subs_used, const ydb_buffer_t *subsarray, int next_subs_used, ydb_buffer_t *next_subsarray);
char *get_key(ydb_buffer_t buffer);
int encode_tree(const ydb_buffer_t *varname, int subs_used, const ydb_buffer_t *subsarray, ydb_var_types encode_type,
	int encode_svn_index, int value_and_subtree, json_t *obj, int *ret_subs_used, ydb_buffer_t *ret_subsarray);

#endif
