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

/* ydb_decode_s.c internal forward declarations */
int	decode_object(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, int max_subs,
		json_t *jansson_object, ydb_var_types decode_type, int decode_svn_index);
int	decode_array(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, int max_subs,
		json_t *jansson_object, ydb_var_types decode_type, int decode_svn_index);
int	decode_string(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, json_t *jansson_object,
		ydb_var_types decode_type, int decode_svn_index);
int	decode_integer(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, json_t *jansson_object,
		ydb_var_types decode_type, int decode_svn_index);
int	decode_real(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, json_t *jansson_object,
		ydb_var_types decode_type, int decode_svn_index);
int	decode_bool(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, ydb_var_types decode_type,
		int decode_svn_index, int bool_type);

/* ydb_encode_s.c internal forward declarations */
int	encode_tree(const ydb_buffer_t *varname, int subs_used, const ydb_buffer_t *subsarray, ydb_var_types encode_type,
		int encode_svn_index, unsigned int data_value, json_t *obj, int *ret_subs_used, ydb_buffer_t *ret_subsarray);

boolean_t	yed_is_integer(ydb_buffer_t buff, long long *value);
boolean_t	yed_is_real(ydb_buffer_t buff, double *value);
boolean_t	yed_is_true(ydb_buffer_t buff);
boolean_t	yed_is_false(ydb_buffer_t buff);
boolean_t	yed_is_null(ydb_buffer_t buff);
boolean_t	yed_is_direct_child_of(int subs_used, const ydb_buffer_t *subsarray,
			int next_subs_used, ydb_buffer_t *next_subsarray);
boolean_t	yed_is_descendant_of(int subs_used, const ydb_buffer_t *subsarray,
			int next_subs_used, ydb_buffer_t *next_subsarray);

/* ydb_encode_s.c and ydb_decode_s.c internal forward declarations */
void		yed_dl_load(char *ydb_caller_fn);

/* ydb_encode_s.c and ydb_decode_s.c external declarations */
GBLREF	volatile int4	outofband;
GBLREF	boolean_t	yed_lydb_rtn;
GBLREF	boolean_t	yed_dl_complete;

/* Jansson external function pointer declarations */
GBLREF	void		(*object_delete)(json_t *);
GBLREF	json_t		*(*new_object)(void), *(*new_string)(const char *, size_t), *(*new_integer)(long long),
			*(*new_real)(double), *(*new_true)(void), *(*new_false)(void), *(*new_null)(void);
GBLREF	size_t		(*output_json)(const json_t *, char *, size_t, size_t);
GBLREF	int		(*set_object)(json_t *, const char *, json_t *);

#endif /* YDB_ENCODE_DECODE_INCLUDED */
