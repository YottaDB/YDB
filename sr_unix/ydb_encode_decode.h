/****************************************************************
 *								*
 * Copyright (c) 2019-2026 YottaDB LLC and/or its subsidiaries.	*
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

/*
 * YED_NUL is a convenience for the ASCII NUL byte, which is used to end C
 * strings and to encode JSON strings that would otherwise be coerced in to
 * canonical numbers.
 *
 * The YED_JSON_FALSE, YED_JSON_TRUE, and YED_JSON_NULL constants represent the
 * JSON value types: false, true, and null. They are prefaced by a \0 byte
 * (which is not allowed in JSON that Jansson decodes by default) to
 * disambiguate them from their string counterparts when stored in M arrays.
 *
 * Jansson uses a default precision of 17 for reals, while YottaDB uses a
 * precision of 18, however, YED_PRECISION is set to 15, so that common reals
 * that are hard for IEEE 754 to store precisely, like 0.1, 0.2, or 9.2 will
 * not be extended to use their precise values, which happens at 17 digits of
 * precision, but not at 15. It's a compromise, but seems to provide the best
 * behaviors for encoding and decoding, in the widest number of cases.
 *
 * YED_INTEGER_MAX_LEN and YED_REAL_MAX_LEN are used to create the buffers that
 * hold the string equivalent of the long long or double numbers in
 * ydb_decode_int() and ydb_decode_real() respectively. They have extra space
 * to accommodate things like `.`, `-`, `e+???`, or `e-???` after conversion to
 * a string.
 *
 * See note at yed_decode_bool() in ydb_decode_s.c for more details.
 */
#define YED_NUL			'\0'
#define YED_JSON_FALSE		"\0false"
#define YED_JSON_TRUE		"\0true"
#define YED_JSON_NULL		"\0null"
#define YED_PRECISION		15
#define YED_INTEGER_MAX_LEN	(YED_PRECISION + 5)
#define YED_REAL_MAX_LEN	(YED_PRECISION + 9)

/* ydb_decode_s.c internal forward declarations */
int	yed_decode_object(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, json_t *jansson_object);
int	yed_decode_array(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, json_t *jansson_object);
int	yed_decode_string(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, json_t *jansson_object);
int	yed_decode_integer(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, json_t *jansson_object);
int	yed_decode_real(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, json_t *jansson_object);
int	yed_decode_bool(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, int bool_type);

/* ydb_encode_s.c internal forward declarations */
int	yed_encode_object(const ydb_buffer_t *varname, int subs_used, const ydb_buffer_t *subsarray,
		unsigned int data_value, json_t *obj, int *ret_subs_used, ydb_buffer_t *ret_subsarray);
int	yed_encode_array(const ydb_buffer_t *varname, int subs_used, const ydb_buffer_t *subsarray,
		json_t *obj, int *ret_subs_used, ydb_buffer_t *ret_subsarray);
int	yed_array_test(const ydb_buffer_t *varname, int order_subs_used, ydb_buffer_t *order_subsarray);

boolean_t	yed_is_integer(ydb_buffer_t buff, long long *value);
boolean_t	yed_is_real(ydb_buffer_t buff, double *value);
boolean_t	yed_is_true(ydb_buffer_t buff);
boolean_t	yed_is_false(ydb_buffer_t buff);
boolean_t	yed_is_null(ydb_buffer_t buff);
boolean_t	yed_is_descendant_of(int subs_used, const ydb_buffer_t *subsarray,
			int next_subs_used, ydb_buffer_t *next_subsarray);
boolean_t	yed_same_node_next(int cur_subs_used, const ydb_buffer_t *cur_subsarray,
			int next_subs_used, ydb_buffer_t *next_subsarray);

/* ydb_encode_s.c and ydb_decode_s.c internal forward declarations */
void	yed_dl_load(char *ydb_caller_fn);
void	yed_object_decref(json_t *json);

/* ydb_encode_s.c and ydb_decode_s.c external declarations */
GBLREF	boolean_t	yed_lydb_rtn;
GBLREF	boolean_t	yed_dl_complete;

/* Jansson external function pointer declarations */
GBLREF	json_t	*(*yed_new_object)(void),
		*(*yed_new_array)(void),
		*(*yed_new_string)(const char *, size_t),
		*(*yed_new_integer)(long long),
		*(*yed_new_real)(double),
		*(*yed_new_true)(void),
		*(*yed_new_false)(void),
		*(*yed_new_null)(void);
GBLREF	void	(*yed_object_delete)(json_t *);
GBLREF	char	*(*yed_encode_json)(const json_t *, size_t);
GBLREF	int	(*yed_set_object)(json_t *, const char *, json_t *),
		(*yed_set_array)(json_t *, json_t *);

#endif /* YDB_ENCODE_DECODE_INCLUDED */
