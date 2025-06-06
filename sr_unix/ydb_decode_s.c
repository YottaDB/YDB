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
#include "dlopen_handle_array.h"
#include "namelook.h"
#include "deferred_events_queue.h"

GBLREF	volatile int4	outofband;

/* Jansson function pointers */
GBLDEF	json_t		*(*yed_decode_json)(const char *, size_t, json_error_t *), *(*yed_obj_next_value)(void *),
			*(*yed_get_value)(const json_t *, size_t), *(*yed_new_object)(void),
			*(*yed_new_string)(const char *, size_t), *(*yed_new_integer)(long long), *(*yed_new_real)(double),
			*(*yed_new_false)(void), *(*yed_new_true)(void), *(*yed_new_null)(void);
GBLDEF	void		*(*yed_get_obj_iter)(json_t *), *(*yed_obj_iter_next)(json_t *, void *), (*yed_object_delete)(json_t *);
GBLDEF	const char	*(*yed_obj_next_key)(void *), *(*yed_get_string_value)(const json_t *);
GBLDEF	size_t		(*yed_get_size)(const json_t *), (*yed_output_json)(const json_t *, char *, size_t, size_t);
GBLDEF	long long	(*yed_get_int_value)(const json_t *);
GBLDEF	double		(*yed_get_real_value)(const json_t *);
GBLDEF	int		(*yed_set_object)(json_t *, const char *, json_t *);

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
	json_t		*jansson_object = NULL;
	json_error_t	jansson_error;
	int		decode_svn_index, json_type, i, string_size, status;
	char		*curpool = NULL;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	VERIFY_NON_THREADED_API;	/* clears a global variable "caller_func_is_stapi" set by SimpleThreadAPI caller
					 * so needs to be first invocation after SETUP_THREADGBL_ACCESS to avoid any error
					 * scenarios from not resetting this global variable even though this function returns.
					 */
	assert(!yed_lydb_rtn);	/* ydb_encode_s() and ydb_decode_s() set to TRUE, and they should never be nested */
	/* Verify entry conditions, make sure YDB CI environment is up etc. */
	LIBYOTTADB_INIT(LYDB_RTN_DECODE, (int));	/* Note: macro could return from this function in case of errors */
	assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* previously unused entries should have been cleared by that
							 * corresponding ydb_*_s() call.
							 */
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{
		if (NULL != curpool)
			system_free(curpool);
		YED_OBJECT_DELETE(jansson_object);
		assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* Should have been cleared by "ydb_simpleapi_ch" */
		yed_lydb_rtn = FALSE;
		REVERT;
		return ((ERR_TPRETRY == SIGNAL) ? YDB_TP_RESTART : -(TREF(ydb_error_code)));
	}
	/* Check if an outofband action that might care about has popped up */
	if (outofband)
		outofband_action(FALSE);
	yed_lydb_rtn = TRUE;
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
	if (!yed_dl_complete)
		yed_dl_load((char *)LYDBRTNNAME(LYDB_RTN_DECODE));
	jansson_object = yed_decode_json(value, 0, &jansson_error);
	if (NULL == jansson_object)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JANSSONINVALIDJSON, 4,
			LEN_AND_STR(jansson_error.text), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
	if (LYDB_VARREF_GLOBAL == decode_type)
		string_size = MAX_KEY_SZ;
	else
		string_size = YDB_MAX_STR;
	curpool = system_malloc(YDB_MAX_SUBS * string_size);
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
		status = yed_decode_object(varname, subs_used, cur_subsarray, max_subs,
				jansson_object, decode_type, decode_svn_index);
	else if (JSON_ARRAY == json_type)	/* array */
		status = yed_decode_array(varname, subs_used, cur_subsarray, max_subs,
				jansson_object, decode_type, decode_svn_index);
	else
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JANSSONINVALIDJSON, 4,
			LEN_AND_LIT("Invalid JSON"), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
	}
	system_free(curpool);
	YED_OBJECT_DELETE(jansson_object);
	assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* the counter should have never become non-zero in this function */
	yed_lydb_rtn = FALSE;
	LIBYOTTADB_DONE;
	REVERT;
	return status;
}

int yed_decode_object(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, int max_subs,
			json_t *jansson_object, ydb_var_types decode_type, int decode_svn_index)
{
	void		*iterator;
	json_t		*value;
	int		cur_subs_used, str_size, status;
	const char	*root, *key;

	root = "";	/* Used as a special JSON key to hold values at M array nodes that also have children at higher levels.
			 * An empty string is allowed as a key in JSON, but not as a subscript in M (by default).
			 */
	iterator = yed_get_obj_iter(jansson_object);
	while (NULL != iterator)
	{
		cur_subs_used = subs_used;
		key = yed_obj_next_key(iterator);
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
		value = yed_obj_next_value(iterator);
		switch (value->type)
		{
			case JSON_OBJECT:	/* object */
				status = yed_decode_object(varname, cur_subs_used, subsarray, max_subs,
						value, decode_type, decode_svn_index);
				break;
			case JSON_ARRAY:	/* array */
				status = yed_decode_array(varname, cur_subs_used, subsarray, max_subs,
						value, decode_type, decode_svn_index);
				break;
			case JSON_STRING:	/* string */
				status = yed_decode_string(varname, cur_subs_used, subsarray, value, decode_type, decode_svn_index);
				break;
			case JSON_INTEGER:	/* integer */
				status = yed_decode_integer(varname, cur_subs_used, subsarray,
						value, decode_type, decode_svn_index);
				break;
			case JSON_REAL:		/* real */
				status = yed_decode_real(varname, cur_subs_used, subsarray, value, decode_type, decode_svn_index);
				break;
			case JSON_TRUE:		/* true */
				status = yed_decode_bool(varname, cur_subs_used, subsarray, decode_type, decode_svn_index, 1);
				break;
			case JSON_FALSE:	/* false */
				status = yed_decode_bool(varname, cur_subs_used, subsarray, decode_type, decode_svn_index, 0);
				break;
			default:		/* null */
				status = yed_decode_bool(varname, cur_subs_used, subsarray, decode_type, decode_svn_index, 2);
		}
		if (YDB_OK != status)
			return status;
		iterator = yed_obj_iter_next(jansson_object, iterator);
	}
	return YDB_OK;
}

int yed_decode_array(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, int max_subs,
			json_t *jansson_object, ydb_var_types decode_type, int decode_svn_index)
{
	json_t		*value;
	int		cur_subs_used, status;
	size_t		array_size;

	array_size = yed_get_size(jansson_object);
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
		value = yed_get_value(jansson_object, (size_t)i);
		switch (value->type)
		{
			case JSON_OBJECT:	/* object */
				status = yed_decode_object(varname, cur_subs_used, subsarray, max_subs,
						value, decode_type, decode_svn_index);
				break;
			case JSON_ARRAY:	/* array */
				status = yed_decode_array(varname, cur_subs_used, subsarray, max_subs,
						value, decode_type, decode_svn_index);
				break;
			case JSON_STRING:	/* string */
				status = yed_decode_string(varname, cur_subs_used, subsarray, value, decode_type, decode_svn_index);
				break;
			case JSON_INTEGER:	/* integer */
				status = yed_decode_integer(varname, cur_subs_used, subsarray,
						value, decode_type, decode_svn_index);
				break;
			case JSON_REAL:		/* real */
				status = yed_decode_real(varname, cur_subs_used, subsarray, value, decode_type, decode_svn_index);
				break;
			case JSON_FALSE:	/* false */
				status = yed_decode_bool(varname, cur_subs_used, subsarray, decode_type, decode_svn_index, 0);
				break;
			case JSON_TRUE:		/* true */
				status = yed_decode_bool(varname, cur_subs_used, subsarray, decode_type, decode_svn_index, 1);
				break;
			default:		/* null */
				status = yed_decode_bool(varname, cur_subs_used, subsarray, decode_type, decode_svn_index, 2);
		}
		if (YDB_OK != status)
			return status;
	}
	return YDB_OK;
}

int yed_decode_string(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, json_t *jansson_object,
			ydb_var_types decode_type, int decode_svn_index)
{
	const char	*value;
	ydb_buffer_t	value_buffer = {0};
	char		done;
	int		status;

	value = yed_get_string_value(jansson_object);
	assert(NULL != value);	/* This function should only be called if jansson_object was previously confirmed to be a string */
	value_buffer.len_alloc = strlen(value);
	if (YDB_MAX_STR < value_buffer.len_alloc)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
			LEN_AND_LIT("Size for string value in JSON input is > YDB_MAX_STR"),
			LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
	value_buffer.buf_addr = system_malloc(value_buffer.len_alloc);
	YDB_COPY_STRING_TO_BUFFER(value, &value_buffer, done);
	assert(TRUE == done);
	status = ydb_set_s(varname, subs_used, subsarray, &value_buffer);
	system_free(value_buffer.buf_addr);
	return status;
}

int yed_decode_integer(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, json_t *jansson_object,
			ydb_var_types decode_type, int decode_svn_index)
{
	long long	value;
	ydb_buffer_t	value_buffer;
	char		buffer[YED_INTEGER_MAX_LEN];

	value = yed_get_int_value(jansson_object);
	value_buffer.len_alloc = YED_INTEGER_MAX_LEN;	/* Jansson uses long long to store integers */
	value_buffer.buf_addr = (char *)&buffer[0];
	value_buffer.len_used = snprintf(value_buffer.buf_addr, value_buffer.len_alloc, "%lld", value);
	return ydb_set_s(varname, subs_used, subsarray, &value_buffer);
}

int yed_decode_real(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, json_t *jansson_object,
			ydb_var_types decode_type, int decode_svn_index)
{
	double		value;
	ydb_buffer_t	value_buffer;
	char		buffer[YED_REAL_MAX_LEN];

	value = yed_get_real_value(jansson_object);
	value_buffer.len_alloc = YED_REAL_MAX_LEN;	/* Jansson uses double to store reals with a default precision of 17 */
	value_buffer.buf_addr = (char *)&buffer[0];
	value_buffer.len_used = snprintf(value_buffer.buf_addr, value_buffer.len_alloc, "%g", value);
	return ydb_set_s(varname, subs_used, subsarray, &value_buffer);
}

/* This function handles the JSON value types of false, true, and null (not strings).
 * M does not support the false, true, and null JSON data types, so we have to
 * distinguish between false and "false", true and "true", null and "null" while
 * stored in M. So these values are stored as $char(0)_"true", $char(0)_"false",
 * and $char(0)_"null" in M arrays. A $char(0) character is supported as data in
 * M arrays, but is not supported as a value in Jansson by default. Thus a value such
 * as "\0_true" cannot be stored as a JSON value in Jansson, and this encoding is
 * therefore unambiguous. Storing the null byte at the beginning instead of the end
 * of the string makes it less likely that it will be confused for a normal C string
 * terminator.
 *
 *	bool_type is one of:
 *		0 for false
 *		1 for true
 *		2 for null
 */
int yed_decode_bool(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray,
			ydb_var_types decode_type, int decode_svn_index, int bool_type)
{
	ydb_buffer_t	value_buffer;

	switch (bool_type)
	{
		case 0:
			value_buffer.len_alloc = SIZEOF(YED_FALSE);
			value_buffer.len_used = STR_LIT_LEN(YED_FALSE);
			value_buffer.buf_addr = YED_FALSE;
			break;
		case 1:
			value_buffer.len_alloc = SIZEOF(YED_TRUE);
			value_buffer.len_used = STR_LIT_LEN(YED_TRUE);
			value_buffer.buf_addr = YED_TRUE;
			break;
		default:
			value_buffer.len_alloc = SIZEOF(YED_NULL);
			value_buffer.len_used = STR_LIT_LEN(YED_NULL);
			value_buffer.buf_addr = YED_NULL;
			break;
	}
	return ydb_set_s(varname, subs_used, subsarray, &value_buffer);
}

/* Load the Jansson library */
void yed_dl_load(char *ydb_caller_fn)
{
	void	*handle;
	char	err_msg[YDB_MAX_ERRORMSG];

	handle = dlopen("libjansson.so", RTLD_LAZY);
	if (NULL == handle)
	{
		strncpy(err_msg, dlerror(), YDB_MAX_ERRORMSG);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JANSSONDLERROR, 4, LEN_AND_STR(err_msg), LEN_AND_STR(ydb_caller_fn));
	}
	dlopen_handle_array_add(handle);
	dlerror();	/* Clear any errors that may have been hanging around */
	/* Get the function pointers from the library */
#	define	YDB_ENCODE_DECODE_FNPTR(YED_FNPTR, SYMBOL_NAME)							\
	{													\
		YED_FNPTR = dlsym(handle, SYMBOL_NAME);								\
		if (NULL == YED_FNPTR)										\
		{												\
			strncpy(err_msg, dlerror(), YDB_MAX_ERRORMSG);						\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6)						\
				ERR_JANSSONDLERROR, 4, LEN_AND_STR(err_msg), LEN_AND_STR(ydb_caller_fn));	\
		}												\
	}
#	include "ydb_encode_decode_fnptr_table.h"
	yed_dl_complete = TRUE;
}
