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

#include "mdef.h"

#include <dlfcn.h>

#include "stringpool.h"
#include "libyottadb_int.h"
#include "namelook.h"
#include "outofband.h"
#include "dlopen_handle_array.h"
#include "ydb_encode_decode.h"
#include "real_len.h"
#include "fgncal.h"		/* Needed for MAX_ERRSTR_LEN */


GBLREF	volatile int4	outofband;

int	ydb_decode_s(const ydb_buffer_t *varname, int subs_used, const ydb_buffer_t *subsarray, int max_subs_usable,
	const char *format, const char *value)
{
	ydb_buffer_t	cur_subsarray[MAX_GVSUBSCRIPTS];
	boolean_t	error_encountered;
	ydb_var_types	decode_type;
	void		*handle;
	json_t		*jansson_object;
	json_error_t	jansson_error;
	json_t		*(*decode)(const char *, size_t, json_error_t *);
	int		decode_svn_index, loop_iterator, json_type;
	char err_msg[MAX_ERRSTR_LEN];
	char_ptr_t	err_str;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	VERIFY_NON_THREADED_API;	/* clears a global variable "caller_func_is_stapi" set by SimpleThreadAPI caller
					 * so needs to be first invocation after SETUP_THREADGBL_ACCESS to avoid any error
					 * scenarios from not resetting this global variable even though this function returns.
					 */
	/* Verify entry conditions, make sure YDB CI environment is up etc. */
	LIBYOTTADB_INIT(LYDB_RTN_DECODE, (int));		/* Note: macro could return from this function in case of errors */
	assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* previously unused entries should have been cleared by that
							 * corresponding ydb_*_s() call.
							 */
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{
		assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* Should have been cleared by "ydb_simpleapi_ch" */
		REVERT;
		return ((ERR_TPRETRY == SIGNAL) ? YDB_TP_RESTART : -(TREF(ydb_error_code)));
	}
	/* Check if an outofband action that might care about has popped up */
	if (outofband)
		outofband_action(FALSE);
	/* Do some validation */
	VALIDATE_VARNAME(varname, decode_type, decode_svn_index, FALSE);
	if (0 > subs_used)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MINNRSUBSCRIPTS);
	if (YDB_MAX_SUBS < subs_used || YDB_MAX_SUBS < max_subs_usable)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
	if (max_subs_usable < subs_used)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
			LEN_AND_LIT("max_subs_usable < subs_used"),
			LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
		return YDB_ERR_PARAMINVALID;
	}
	if (NULL == value)
		return YDB_OK;
	for (loop_iterator = 0; loop_iterator < subs_used; loop_iterator++)
	{
		if (subsarray[loop_iterator].len_alloc < subsarray[loop_iterator].len_used)
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
				LEN_AND_LIT("len_alloc < len_used for at least 1 subscript in subsarray"),
				LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
			return YDB_ERR_PARAMINVALID;
		}
		if ((0 != subsarray[loop_iterator].len_used) && (NULL == subsarray[loop_iterator].buf_addr))
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
				LEN_AND_LIT("len_used is non-zero and buf_addr is NULL for at least 1 subscript in subsarray"),
				LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
			return YDB_ERR_PARAMINVALID;
		}
	}
	handle = dlopen("libjansson.so", RTLD_LAZY);
	if (NULL == handle)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JANSSONDLNOOPEN, 4,
			LEN_AND_LIT("attempt to open jansson failed"),
			LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
		return YDB_ERR_JANSSONDLNOOPEN;
	}
	dlopen_handle_array_add(handle);
	/* Get the function pointers from the library */
	decode = dlsym(handle, "json_loads");
	jansson_object = decode(value, 0, &jansson_error);
	if (NULL == jansson_object)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
			LEN_AND_LIT("Invalid JSON"),
			LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
		return YDB_ERR_PARAMINVALID;
	}
	for (loop_iterator = 0; loop_iterator < MAX_GVSUBSCRIPTS; loop_iterator++)
	{
		cur_subsarray[loop_iterator].len_used = subsarray[loop_iterator].len_used;
		cur_subsarray[loop_iterator].len_alloc = MAX_LVSUBSCRIPTS;
		ENSURE_STP_FREE_SPACE(MAX_LVSUBSCRIPTS);
		cur_subsarray[loop_iterator].buf_addr = (char *)stringpool.free;
		memcpy(cur_subsarray[loop_iterator].buf_addr, subsarray[loop_iterator].buf_addr, cur_subsarray[loop_iterator].len_used);
		stringpool.free += MAX_LVSUBSCRIPTS;
		assert(stringpool.free <= stringpool.top);
	}
	json_type = jansson_object->type;
	if (JSON_OBJECT == json_type) /* object */
	{
		decode_json_object(varname, subs_used, cur_subsarray, max_subs_usable, jansson_object, decode_type, decode_svn_index);
	}
	else if (JSON_ARRAY == json_type) /* array */
	{
		decode_json_array(varname, subs_used, cur_subsarray, max_subs_usable, jansson_object, decode_type, decode_svn_index);
	}
	else
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
			LEN_AND_LIT("Invalid JSON"),
			LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
		return YDB_ERR_PARAMINVALID;
	}
	TREF(sapi_mstrs_for_gc_indx) = 0;
	LIBYOTTADB_DONE;
	REVERT;
	return YDB_OK;
}

int decode_json_object(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, int max_subs_usable,
	json_t *jansson_object, ydb_var_types decode_type, int decode_svn_index)
{
	void		*handle, *iterator;
	void		*(*get_obj_iter)(json_t *object), *(*obj_iter_next)(json_t *, void *);
	json_t		*(*obj_next_value)(void *);
	json_t		*value;
	int		cur_subs_used, i, str_size;
	const char	*(*obj_next_key)(void *);
	const char	*key;

	handle = dlopen("libjansson.so", RTLD_LAZY);
	dlopen_handle_array_add(handle);

	obj_iter_next = dlsym(handle, "json_object_iter_next");
	obj_next_key = dlsym(handle, "json_object_iter_key");
	obj_next_value = dlsym(handle, "json_object_iter_value");
	get_obj_iter = dlsym(handle, "json_object_iter");
	iterator = get_obj_iter(jansson_object);
	while (NULL != iterator)
	{
		cur_subs_used = subs_used;
		key = obj_next_key(iterator);
		if (cur_subs_used == max_subs_usable)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
		else if (0 != strcmp(key, "_root")) 	/* If this is the value that shares a variable name
		 					 * and subscripts with the subtree, we don't want to
							 * add a subscript called "_root"
							 */
		{
			str_size = strlen(key);
			if (subsarray[cur_subs_used].len_alloc > str_size)
			{
				strcpy(subsarray[cur_subs_used].buf_addr, key);
				subsarray[cur_subs_used].len_used = str_size;
				cur_subs_used++;
			}
			else
			{
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
					LEN_AND_LIT("buf_addr is too small for at least 1 key in JSON input"),
					LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
				return YDB_ERR_PARAMINVALID;
			}
		}
		value = obj_next_value(iterator);
		switch (value->type)
		{
			case JSON_OBJECT: /* object */
				decode_json_object(varname, cur_subs_used, subsarray, max_subs_usable, value, decode_type, decode_svn_index);
				break;
			case JSON_ARRAY: /* array */
				decode_json_array(varname, cur_subs_used, subsarray, max_subs_usable, value, decode_type, decode_svn_index);
				break;
			case JSON_STRING: /* string */
				decode_json_string(varname, cur_subs_used, subsarray, value, decode_type, decode_svn_index);
				break;
			case JSON_INTEGER: /* integer */
				decode_json_integer(varname, cur_subs_used, subsarray, value, decode_type, decode_svn_index);
				break;
			case JSON_REAL: /* real */
				decode_json_real(varname, cur_subs_used, subsarray, value, decode_type, decode_svn_index);
				break;
			case JSON_TRUE: /* TRUE */
				decode_json_bool(varname, cur_subs_used, subsarray, value, decode_type, decode_svn_index, 1);
				break;
			case JSON_FALSE: /* FALSE */
				decode_json_bool(varname, cur_subs_used, subsarray, value, decode_type, decode_svn_index, 0);
				break;
			default: /* NULL */
				decode_json_bool(varname, cur_subs_used, subsarray, value, decode_type, decode_svn_index, 2);
		}
		iterator = obj_iter_next(jansson_object, iterator);
	}
	return YDB_OK;
}

int decode_json_array(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, int max_subs_usable,
	json_t *jansson_object, ydb_var_types decode_type, int decode_svn_index)
{
	void		*handle;
	json_t		*value;
	json_t		*(*get_value)(const json_t *, size_t);
	int		value_type, i, cur_subs_used;
	size_t		array_size;
	size_t		(*get_size)(const json_t *);

	handle = dlopen("libjansson.so", RTLD_LAZY);
	dlopen_handle_array_add(handle);
	get_size = dlsym(handle, "json_array_size");
	get_value = dlsym(handle, "json_array_get");
	array_size = get_size(jansson_object);
	for (i = 0; i < array_size; i++)
	{
		cur_subs_used = subs_used;
		if (max_subs_usable > cur_subs_used)
		{
			subsarray[cur_subs_used].len_used = snprintf(subsarray[cur_subs_used].buf_addr,
				subsarray[cur_subs_used].len_alloc, "%d", i);
			if ((subsarray[cur_subs_used].len_alloc < subsarray[cur_subs_used].len_used) ||
				(subsarray[cur_subs_used].len_used < 0))
			{
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
					LEN_AND_LIT("buf_addr is too small for at least 1 key in JSON input"),
					LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
				return YDB_ERR_PARAMINVALID;
			}
			else
			{
				cur_subs_used++;
			}
		}
		else
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
		value = get_value(jansson_object, (size_t)i);
		switch (value->type)
		{
			case JSON_OBJECT: /* object */
				decode_json_object(varname, cur_subs_used, subsarray, max_subs_usable, value, decode_type, decode_svn_index);
				break;
			case JSON_ARRAY: /* array */
				decode_json_array(varname, cur_subs_used, subsarray, max_subs_usable, value, decode_type, decode_svn_index);
				break;
			case JSON_STRING: /* string */
				decode_json_string(varname, cur_subs_used, subsarray, value, decode_type, decode_svn_index);
				break;
			case JSON_INTEGER: /* integer */
				decode_json_integer(varname, cur_subs_used, subsarray, value, decode_type, decode_svn_index);
				break;
			case JSON_REAL: /* real */
				decode_json_real(varname, cur_subs_used, subsarray, value, decode_type, decode_svn_index);
				break;
			case JSON_TRUE: /* TRUE */
				decode_json_bool(varname, cur_subs_used, subsarray, value, decode_type, decode_svn_index, 1);
				break;
			case JSON_FALSE: /* FALSE */
				decode_json_bool(varname, cur_subs_used, subsarray, value, decode_type, decode_svn_index, 0);
				break;
			default: /* NULL */
				decode_json_bool(varname, cur_subs_used, subsarray, value, decode_type, decode_svn_index, 2);
		}
	}
	return YDB_OK;
}

/* 	This function handles JSON keys with a value of TRUE, FALSE or NULL.
 *	bool_types are:
 *	0 for FALSE
 *	1 for TRUE
 *	2 for NULL
 */
int decode_json_bool(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, json_t *jansson_object,
	ydb_var_types decode_type, int decode_svn_index, int bool_type)
{
	ydb_buffer_t	value_buffer;

	switch(bool_type)
	{
		case 0:
			value_buffer.len_alloc = 5;
			value_buffer.len_used = 5;
			value_buffer.buf_addr = "FALSE";
			ydb_set_value(varname, subs_used, subsarray, &value_buffer, decode_type, decode_svn_index, (char *)LYDBRTNNAME(LYDB_RTN_DECODE));
			break;
		case 1:
			value_buffer.len_alloc = 4;
			value_buffer.len_used = 4;
			value_buffer.buf_addr = "TRUE";
			ydb_set_value(varname, subs_used, subsarray, &value_buffer, decode_type, decode_svn_index, (char *)LYDBRTNNAME(LYDB_RTN_DECODE));
			break;
		default:
			value_buffer.len_alloc = 4;
			value_buffer.len_used = 4;
			value_buffer.buf_addr = "NULL";
			ydb_set_value(varname, subs_used, subsarray, &value_buffer, decode_type, decode_svn_index, (char *)LYDBRTNNAME(LYDB_RTN_DECODE));
			break;
	}
	return YDB_OK;
}

int decode_json_string(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, json_t *jansson_object,
	ydb_var_types decode_type, int decode_svn_index)
{
	void		*handle;
	const char	*value;
	const char	*(*get_string_value)(const json_t *);
	ydb_buffer_t	value_buffer;

	handle = dlopen("libjansson.so", RTLD_LAZY);
	dlopen_handle_array_add(handle);

	get_string_value = dlsym(handle, "json_string_value");
	value = get_string_value(jansson_object);
	assert(NULL != value); /* This function should only be called if jansson_object was previously confirmed to be a string */
	value_buffer.len_alloc = strlen(value) + 1; /* add 1 for the null terminator */
	value_buffer.len_used = value_buffer.len_alloc - 1;
	ENSURE_STP_FREE_SPACE(value_buffer.len_alloc);
	value_buffer.buf_addr = (char *)stringpool.free;
	stringpool.free += value_buffer.len_alloc;
	assert(stringpool.free <= stringpool.top);
	strcpy(value_buffer.buf_addr, value);
	ydb_set_value(varname, subs_used, subsarray, &value_buffer, decode_type, decode_svn_index, (char *)LYDBRTNNAME(LYDB_RTN_DECODE));
	return YDB_OK;
}

int decode_json_integer(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, json_t *jansson_object,
	ydb_var_types decode_type, int decode_svn_index)
{
	void		*handle;
	long long	value;
	long long	(*get_int_value)(const json_t *);
	ydb_buffer_t	value_buffer;

	handle = dlopen("libjansson.so", RTLD_LAZY);
	dlopen_handle_array_add(handle);
	get_int_value = dlsym(handle, "json_integer_value");
	value = get_int_value(jansson_object);
	value_buffer.len_alloc = 21;
	ENSURE_STP_FREE_SPACE(value_buffer.len_alloc);
	value_buffer.buf_addr = (char *)stringpool.free;
	stringpool.free += value_buffer.len_alloc;
	assert(stringpool.free <= stringpool.top);
	value_buffer.len_used = snprintf(value_buffer.buf_addr, value_buffer.len_alloc, "%lld", value);
	ydb_set_value(varname, subs_used, subsarray, &value_buffer, decode_type, decode_svn_index, (char *)LYDBRTNNAME(LYDB_RTN_DECODE));
	return YDB_OK;
}

int decode_json_real(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, json_t *jansson_object,
	ydb_var_types decode_type, int decode_svn_index)
{
	void		*handle;
	double		value;
	double		(*get_real_value)(const json_t *);
	ydb_buffer_t	value_buffer;

	handle = dlopen("libjansson.so", RTLD_LAZY);
	dlopen_handle_array_add(handle);

	get_real_value = dlsym(handle, "json_real_value");
	value = get_real_value(jansson_object);
	value_buffer.len_alloc = 0;
	value_buffer.len_used = snprintf(value_buffer.buf_addr, value_buffer.len_alloc, "%f", value);
	value_buffer.len_alloc = value_buffer.len_used + 1; /* Add 1 for the null terminator */
	ENSURE_STP_FREE_SPACE(value_buffer.len_alloc);
	value_buffer.buf_addr = (char *)stringpool.free;
	stringpool.free += value_buffer.len_alloc;
	assert(stringpool.free <= stringpool.top);
	value_buffer.len_used = snprintf(value_buffer.buf_addr, value_buffer.len_alloc, "%f", value);
	ydb_set_value(varname, subs_used, subsarray, &value_buffer, decode_type, decode_svn_index, (char *)LYDBRTNNAME(LYDB_RTN_DECODE));
	return YDB_OK;
}
