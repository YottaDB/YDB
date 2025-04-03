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

#include "mdef.h"

#include <dlfcn.h>

#include "libyottadb_int.h"
#include "ydb_encode_decode.h"
#include "namelook.h"
#include "dlopen_handle_array.h"
#include "deferred_events_queue.h"

GBLREF	volatile int4	outofband;

/* Routine to decode a formatted string in to a local or global
 *
 * Parameters:
 *   varname    - Gives name of local or global variable
 *   subs_used	- Count of subscripts already setup for destination array (subtree root)
 *   subsarray	- an array of "max_subs" subscripts
 *   max_subs	- the max number of subscripts allocated for decoding the value
 *   format	- Format of string to be decoded (currently always "JSON" and ignored - for future use)
 *   value	- Value to be decoded and set into local/global
 */
int ydb_decode_s(const ydb_buffer_t *varname, int subs_used, const ydb_buffer_t *subsarray,
			int max_subs, const char *format, const char *value)
{
	ydb_buffer_t	cur_subsarray[YDB_MAX_SUBS] = {0};
	boolean_t	error_encountered;
	ydb_var_types	decode_type;
	void		*handle;
	json_t		*(*decode)(const char *, size_t, json_error_t *), *jansson_object;
	json_error_t	jansson_error;
	int		decode_svn_index, json_type, i, string_size;
	char		err_msg[YDB_MAX_ERRORMSG], *curpool;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	VERIFY_NON_THREADED_API;	/* clears a global variable "caller_func_is_stapi" set by SimpleThreadAPI caller
					 * so needs to be first invocation after SETUP_THREADGBL_ACCESS to avoid any error
					 * scenarios from not resetting this global variable even though this function returns.
					 */
	/* Verify entry conditions, make sure YDB CI environment is up etc. */
	LIBYOTTADB_INIT(LYDB_RTN_DECODE, (int));	/* Note: macro could return from this function in case of errors */
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
	VALIDATE_VARNAME(varname, subs_used, FALSE, LYDB_RTN_DECODE, -1, decode_type, decode_svn_index);
	if (0 > subs_used)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MINNRSUBSCRIPTS);
	if (YDB_MAX_SUBS < subs_used || YDB_MAX_SUBS < max_subs)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
	if (max_subs < subs_used)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
			LEN_AND_LIT("max_subs < subs_used"), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
	if (NULL == value)
		return YDB_OK;
	for (i = 0; i < subs_used; i++)
	{
		if (subsarray[i].len_alloc < subsarray[i].len_used)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
				LEN_AND_LIT("len_alloc < len_used for at least 1 subscript in subsarray"),
				LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
		if ((0 != subsarray[i].len_used) && (NULL == subsarray[i].buf_addr))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
				LEN_AND_LIT("len_used is non-zero and buf_addr is NULL for at least 1 subscript in subsarray"),
				LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
	}
	handle = dlopen("libjansson.so", RTLD_LAZY);
	if (NULL == handle)
	{
		strncpy(err_msg, dlerror(), YDB_MAX_ERRORMSG);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JANSSONDLERROR, 4,
			LEN_AND_STR(err_msg), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
	}
	dlopen_handle_array_add(handle);
	/* Get the function pointers from the library */
	decode = dlsym(handle, "json_loads");
	if (NULL == decode)
	{
		strncpy(err_msg, dlerror(), YDB_MAX_ERRORMSG);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JANSSONDLERROR, 4,
			LEN_AND_STR(err_msg), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
	}
	jansson_object = decode(value, 0, &jansson_error);
	if (NULL == jansson_object)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JANSSONINVALIDJSON, 4,
			LEN_AND_STR(jansson_error.text), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
	if (LYDB_VARREF_GLOBAL == decode_type)
		string_size = MAX_KEY_SZ;
	else
		string_size = YDB_MAX_STR;
	curpool = calloc(YDB_MAX_SUBS, string_size);
	for (i = 0; YDB_MAX_SUBS > i; i++)
	{
		cur_subsarray[i].len_alloc = string_size;
		cur_subsarray[i].buf_addr = &curpool[string_size * i];
		if (i < subs_used)
		{
			cur_subsarray[i].len_used = subsarray[i].len_used;
			memcpy(cur_subsarray[i].buf_addr, subsarray[i].buf_addr, cur_subsarray[i].len_used);
		}
	}
	json_type = jansson_object->type;
	if (JSON_OBJECT == json_type)		/* object */
		decode_json_object(varname, subs_used, cur_subsarray, max_subs,
			jansson_object, decode_type, decode_svn_index, handle);
	else if (JSON_ARRAY == json_type)	/* array */
		decode_json_array(varname, subs_used, cur_subsarray, max_subs,
			jansson_object, decode_type, decode_svn_index, handle);
	else
	{
		system_free(curpool);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JANSSONINVALIDJSON, 4,
			LEN_AND_LIT("Invalid JSON"), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
	}
	system_free(curpool);
	assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* the counter should have never become non-zero in this function */
	LIBYOTTADB_DONE;
	REVERT;
	return YDB_OK;
}

int decode_json_object(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, int max_subs,
			json_t *jansson_object, ydb_var_types decode_type, int decode_svn_index, void *handle)
{
	void		*iterator, *(*get_obj_iter)(json_t *object), *(*obj_iter_next)(json_t *, void *);
	json_t		*(*obj_next_value)(void *), *value;
	int		cur_subs_used, str_size;
	const char	*(*obj_next_key)(void *), *root, *key;
	char		err_msg[YDB_MAX_ERRORMSG];

	root = "";	/* Used as a special JSON key to hold values at M array nodes that also have children at higher levels.
			 * An empty string is allowed as a key in JSON, but not as a subscript in M (by default).
			 */
	obj_iter_next = dlsym(handle, "json_object_iter_next");
	if (NULL == obj_iter_next)
	{
		strncpy(err_msg, dlerror(), YDB_MAX_ERRORMSG);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JANSSONDLERROR, 4,
			LEN_AND_STR(err_msg), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
	}
	obj_next_key = dlsym(handle, "json_object_iter_key");
	if (NULL == obj_next_key)
	{
		strncpy(err_msg, dlerror(), YDB_MAX_ERRORMSG);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JANSSONDLERROR, 4,
			LEN_AND_STR(err_msg), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
	}
	obj_next_value = dlsym(handle, "json_object_iter_value");
	if (NULL == obj_next_value)
	{
		strncpy(err_msg, dlerror(), YDB_MAX_ERRORMSG);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JANSSONDLERROR, 4,
			LEN_AND_STR(err_msg), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
	}
	get_obj_iter = dlsym(handle, "json_object_iter");
	if (NULL == get_obj_iter)
	{
		strncpy(err_msg, dlerror(), YDB_MAX_ERRORMSG);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JANSSONDLERROR, 4,
			LEN_AND_STR(err_msg), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
	}
	iterator = get_obj_iter(jansson_object);
	while (NULL != iterator)
	{
		cur_subs_used = subs_used;
		key = obj_next_key(iterator);
		if (cur_subs_used == max_subs)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
		else if (0 != strcmp(key, root))	/* If this is the value that shares a variable name
							 * and subscripts with the subtree, we don't want to
							 * add a empty string ("") subscript
							 */
		{
			str_size = strlen(key);
			if (subsarray[cur_subs_used].len_alloc > str_size)
			{
				subsarray[cur_subs_used].len_used = str_size;
				memcpy(subsarray[cur_subs_used].buf_addr, key, str_size);
				cur_subs_used++;
			}
			else
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
					LEN_AND_LIT("buf_addr is too small for at least 1 key in JSON input"),
					LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
		}
		value = obj_next_value(iterator);
		switch (value->type)
		{
			case JSON_OBJECT:	/* object */
				decode_json_object(varname, cur_subs_used, subsarray, max_subs,
					value, decode_type, decode_svn_index, handle);
				break;
			case JSON_ARRAY:	/* array */
				decode_json_array(varname, cur_subs_used, subsarray, max_subs,
					value, decode_type, decode_svn_index, handle);
				break;
			case JSON_STRING:	/* string */
				decode_json_string(varname, cur_subs_used, subsarray, value,
					decode_type, decode_svn_index, handle);
				break;
			case JSON_INTEGER:	/* integer */
				decode_json_integer(varname, cur_subs_used, subsarray, value,
					decode_type, decode_svn_index, handle);
				break;
			case JSON_REAL:		/* real */
				decode_json_real(varname, cur_subs_used, subsarray, value,
					decode_type, decode_svn_index, handle);
				break;
			case JSON_TRUE:		/* TRUE */
				decode_json_bool(varname, cur_subs_used, subsarray, decode_type, decode_svn_index, 1);
				break;
			case JSON_FALSE:	/* FALSE */
				decode_json_bool(varname, cur_subs_used, subsarray, decode_type, decode_svn_index, 0);
				break;
			default:		/* NULL */
				decode_json_bool(varname, cur_subs_used, subsarray, decode_type, decode_svn_index, 2);
		}
		iterator = obj_iter_next(jansson_object, iterator);
	}
	return YDB_OK;
}

int decode_json_array(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, int max_subs,
			json_t *jansson_object, ydb_var_types decode_type, int decode_svn_index, void *handle)
{
	json_t		*value, *(*get_value)(const json_t *, size_t);
	int		cur_subs_used;
	size_t		array_size, (*get_size)(const json_t *);
	char		err_msg[YDB_MAX_ERRORMSG];

	get_size = dlsym(handle, "json_array_size");
	if (NULL == get_size)
	{
		strncpy(err_msg, dlerror(), YDB_MAX_ERRORMSG);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JANSSONDLERROR, 4,
			LEN_AND_STR(err_msg), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
	}
	get_value = dlsym(handle, "json_array_get");
	if (NULL == get_value)
	{
		strncpy(err_msg, dlerror(), YDB_MAX_ERRORMSG);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JANSSONDLERROR, 4,
			LEN_AND_STR(err_msg), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
	}
	array_size = get_size(jansson_object);
	if (YDB_MAX_SUBS < array_size)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
			LEN_AND_LIT("Length of at least 1 array in JSON input is > YDB_MAX_SUBS"),
			LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
	for (int i = 0; i < array_size; i++)
	{
		cur_subs_used = subs_used;
		if (max_subs > cur_subs_used)
		{
			subsarray[cur_subs_used].len_used = snprintf(NULL, 0, "%d", i);
			if (subsarray[cur_subs_used].len_used + 1 > subsarray[cur_subs_used].len_alloc)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
					LEN_AND_LIT("buf_addr is too small for at least 1 key in JSON input"),
					LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
			snprintf(subsarray[cur_subs_used].buf_addr, subsarray[cur_subs_used].len_used + 1, "%d", i);
			cur_subs_used++;
		}
		else
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
		value = get_value(jansson_object, (size_t)i);
		switch (value->type)
		{
			case JSON_OBJECT:	/* object */
				decode_json_object(varname, cur_subs_used, subsarray, max_subs,
					value, decode_type, decode_svn_index, handle);
				break;
			case JSON_ARRAY:	/* array */
				decode_json_array(varname, cur_subs_used, subsarray, max_subs,
					value, decode_type, decode_svn_index, handle);
				break;
			case JSON_STRING:	/* string */
				decode_json_string(varname, cur_subs_used, subsarray, value,
					decode_type, decode_svn_index, handle);
				break;
			case JSON_INTEGER:	/* integer */
				decode_json_integer(varname, cur_subs_used, subsarray, value,
					decode_type, decode_svn_index, handle);
				break;
			case JSON_REAL:		/* real */
				decode_json_real(varname, cur_subs_used, subsarray, value,
					decode_type, decode_svn_index, handle);
				break;
			case JSON_FALSE:	/* FALSE */
				decode_json_bool(varname, cur_subs_used, subsarray, decode_type, decode_svn_index, 0);
				break;
			case JSON_TRUE:		/* TRUE */
				decode_json_bool(varname, cur_subs_used, subsarray, decode_type, decode_svn_index, 1);
				break;
			default:		/* NULL */
				decode_json_bool(varname, cur_subs_used, subsarray, decode_type, decode_svn_index, 2);
		}
	}
	return YDB_OK;
}

int decode_json_string(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, json_t *jansson_object,
			ydb_var_types decode_type, int decode_svn_index, void *handle)
{
	const char	*value, *(*get_string_value)(const json_t *);
	ydb_buffer_t	value_buffer = {0};
	char		err_msg[YDB_MAX_ERRORMSG], done;

	get_string_value = dlsym(handle, "json_string_value");
	if (NULL == get_string_value)
	{
		strncpy(err_msg, dlerror(), YDB_MAX_ERRORMSG);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JANSSONDLERROR, 4,
			LEN_AND_STR(err_msg), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
	}
	value = get_string_value(jansson_object);
	assert(NULL != value);	/* This function should only be called if jansson_object was previously confirmed to be a string */
	value_buffer.len_alloc = strlen(value);
	if (YDB_MAX_STR < value_buffer.len_alloc)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
			LEN_AND_LIT("Size for string value in JSON input is > YDB_MAX_STR"),
			LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
	value_buffer.buf_addr = system_malloc(value_buffer.len_alloc);
	YDB_COPY_STRING_TO_BUFFER(value, &value_buffer, done);
	assert(TRUE == done);
	ydb_set_value(varname, subs_used, subsarray, &value_buffer, decode_type,
		decode_svn_index, (char *)LYDBRTNNAME(LYDB_RTN_DECODE));
	system_free(value_buffer.buf_addr);
	return YDB_OK;
}

int decode_json_integer(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, json_t *jansson_object,
			ydb_var_types decode_type, int decode_svn_index, void *handle)
{
	long long	value, (*get_int_value)(const json_t *);
	ydb_buffer_t	value_buffer;
	char		err_msg[YDB_MAX_ERRORMSG], buffer[21];

	get_int_value = dlsym(handle, "json_integer_value");
	if (NULL == get_int_value)
	{
		strncpy(err_msg, dlerror(), YDB_MAX_ERRORMSG);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JANSSONDLERROR, 4,
			LEN_AND_STR(err_msg), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
	}
	value = get_int_value(jansson_object);
	value_buffer.len_alloc = 21;	/* Jansson uses long long to store integers */
	value_buffer.buf_addr = (char *)&buffer[0];
	value_buffer.len_used = snprintf(value_buffer.buf_addr, value_buffer.len_alloc, "%lld", value);
	ydb_set_value(varname, subs_used, subsarray, &value_buffer, decode_type,
		decode_svn_index, (char *)LYDBRTNNAME(LYDB_RTN_DECODE));
	return YDB_OK;
}

int decode_json_real(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, json_t *jansson_object,
			ydb_var_types decode_type, int decode_svn_index, void *handle)
{
	double		value, (*get_real_value)(const json_t *);
	ydb_buffer_t	value_buffer;
	char		err_msg[YDB_MAX_ERRORMSG], buffer[21];

	get_real_value = dlsym(handle, "json_real_value");
	if (NULL == get_real_value)
	{
		strncpy(err_msg, dlerror(), YDB_MAX_ERRORMSG);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JANSSONDLERROR, 4,
			LEN_AND_STR(err_msg), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
	}
	value = get_real_value(jansson_object);
	value_buffer.len_alloc = 21;	/* Jansson uses double to store reals with a default precision of 17 */
	value_buffer.buf_addr = (char *)&buffer[0];
	value_buffer.len_used = snprintf(value_buffer.buf_addr, value_buffer.len_alloc, "%g", value);
	ydb_set_value(varname, subs_used, subsarray, &value_buffer, decode_type,
		decode_svn_index, (char *)LYDBRTNNAME(LYDB_RTN_DECODE));
	return YDB_OK;
}

/* This function handles JSON keys with a value of true, false or null (not strings).
 * These values are stored as "TRUE", "FALSE", and "NULL" in M arrays.
 *	bool_types are:
 *		0 for false
 *		1 for true
 *		2 for null
 */
int decode_json_bool(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray,
			ydb_var_types decode_type, int decode_svn_index, int bool_type)
{
	ydb_buffer_t	value_buffer;

	switch (bool_type)
	{
		case 0:
			value_buffer.len_alloc = 6;
			value_buffer.len_used = 5;
			value_buffer.buf_addr = "FALSE";
			break;
		case 1:
			value_buffer.len_alloc = 5;
			value_buffer.len_used = 4;
			value_buffer.buf_addr = "TRUE";
			break;
		default:
			value_buffer.len_alloc = 5;
			value_buffer.len_used = 4;
			value_buffer.buf_addr = "NULL";
			break;
	}
	ydb_set_value(varname, subs_used, subsarray, &value_buffer, decode_type,
		decode_svn_index, (char *)LYDBRTNNAME(LYDB_RTN_DECODE));
	return YDB_OK;
}
