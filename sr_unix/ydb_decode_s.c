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

#include "mdef.h"

#include <dlfcn.h>

#include "libyottadb_int.h"
#include "ydb_encode_decode.h"
#include "op_zyencode_zydecode.h"		/* for zydecode_glvn_ptr */
#include "dlopen_handle_array.h"
#include "namelook.h"
#include "deferred_events_queue.h"

GBLREF	volatile int4		outofband;
GBLREF	int			zydecode_args;
GBLREF	zydecode_glvn_ptr	dglvnp;

/* Jansson function pointers */
GBLDEF	json_t		*(*yed_decode_json)(const char *, size_t, size_t, json_error_t *),
			*(*yed_obj_next_value)(void *),
			*(*yed_get_value)(const json_t *, size_t),
			*(*yed_new_object)(void),
			*(*yed_new_array)(void),
			*(*yed_new_string)(const char *, size_t),
			*(*yed_new_integer)(long long),
			*(*yed_new_real)(double),
			*(*yed_new_true)(void),
			*(*yed_new_false)(void),
			*(*yed_new_null)(void);
GBLDEF	void		*(*yed_get_obj_iter)(json_t *),
			*(*yed_obj_iter_next)(json_t *, void *),
			(*yed_object_delete)(json_t *);
GBLDEF	const char	*(*yed_obj_next_key)(void *),
			*(*yed_get_string_value)(const json_t *);
GBLDEF	size_t		(*yed_get_size)(const json_t *);
GBLDEF	char		*(*yed_encode_json)(const json_t *, size_t);
GBLDEF	long long	(*yed_get_int_value)(const json_t *);
GBLDEF	double		(*yed_get_real_value)(const json_t *);
GBLDEF	int		(*yed_set_object)(json_t *, const char *, json_t *),
			(*yed_set_array)(json_t *, json_t *);

STATICDEF	ydb_var_types	decode_type;

/*
 * Routine to decode a formatted string in to a local or global
 *
 * Parameters:
 *   varname    - Gives name of local or global variable
 *   subs_used	- Count of subscripts already setup for destination array (subtree root)
 *   subsarray	- an array of "subs_used" subscripts
 *   format	- Format of string to be decoded (currently always "JSON" and ignored - for future use)
 *   value	- Value to be decoded and set into local/global
 *
 *  NOTE: Caller of ydb_decode_s() must supply a pointer to a non-empty ydb_string_t struct for the value argument
 *	  value->address must not be NULL, it must be a pointer to a valid JSON string
 *	  value->length must not be 0, it must be set to the length of the JSON string (not including the trailing NUL byte)
 *	  The caller is responsible for freeing the memory at value->address if it had allocated such on the heap
 */
int ydb_decode_s(const ydb_buffer_t *varname, int subs_used, const ydb_buffer_t *subsarray,
			const char *format, const ydb_string_t *value)
{
	ydb_buffer_t	cur_subsarray[YDB_MAX_SUBS] = {0};
	boolean_t	error_encountered;
	json_t		*jansson_object = NULL;
	json_error_t	jansson_error;
	int		decode_svn_index, json_type, i, key_size, status;
	char		*curpool = NULL;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	VERIFY_NON_THREADED_API;	/* Clears a global variable "caller_func_is_stapi" set by SimpleThreadAPI caller
					 * so needs to be first invocation after SETUP_THREADGBL_ACCESS to avoid any error
					 * scenarios from not resetting this global variable even though this function returns.
					 */
	assert(!yed_lydb_rtn);	/* ydb_encode_s() and ydb_decode_s() set to TRUE, and they should never be nested */
	if (zydecode_args)     /* zydecode_args is > 0 if called by op_zydecode() */
		TREF(libyottadb_active_rtn) = LYDB_RTN_DECODE;	/* Set active routine when called by op_zydecode() */
	else
		/* Verify entry conditions, make sure YDB CI environment is up etc. */
		LIBYOTTADB_INIT(LYDB_RTN_DECODE, (int));        /* Note: macro could return from this function in case of errors */
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{
		system_free(curpool);
		yed_object_decref(jansson_object);
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
	if (NULL == value)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
			LEN_AND_LIT("NULL value"), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
	if (NULL == value->address)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
			LEN_AND_LIT("NULL value->address"), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
	if (0 == value->length)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
			LEN_AND_LIT("0 value->length"), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
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
	jansson_object = yed_decode_json(value->address, value->length, 0, &jansson_error);
	if (NULL == jansson_object)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JANSSONINVALIDJSON, 4,
			LEN_AND_STR(jansson_error.text), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
	if (LYDB_VARREF_GLOBAL == decode_type)
		if (zydecode_args)	/* zydecode_args is > 0 if called by op_zydecode() */
			key_size = dglvnp->gblp[0]->s_gv_cur_region->max_key_size;
		else
			key_size = MAX_KEY_SZ;
	else
		key_size = YDB_MAX_STR;
	curpool = system_malloc(YDB_MAX_SUBS * key_size);
	for (i = 0; YDB_MAX_SUBS > i; i++)
	{
		cur_subsarray[i].len_alloc = key_size;
		cur_subsarray[i].buf_addr = &curpool[key_size * i];
		if (i < subs_used)
		{
			cur_subsarray[i].len_used = subsarray[i].len_used;
			memcpy(cur_subsarray[i].buf_addr, subsarray[i].buf_addr, cur_subsarray[i].len_used);
		}
	}
	json_type = jansson_object->type;
	/* Jansson only allows an object or an array as the enclosing data type by default */
	if (JSON_ARRAY == json_type)	/* array */
		status = yed_decode_array(varname, subs_used, cur_subsarray, jansson_object);
	else	/* object */
		status = yed_decode_object(varname, subs_used, cur_subsarray, jansson_object);
	system_free(curpool);
	yed_object_decref(jansson_object);
	yed_lydb_rtn = FALSE;
	LIBYOTTADB_DONE;
	REVERT;
	return status;
}

int yed_decode_object(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, json_t *jansson_object)
{
	void		*iterator;
	json_t		*value;
	int		cur_subs_used, str_size, status = YDB_OK;
	const char	*root, *key;

	root = "";	/* Used as a special JSON key to hold values at M array nodes that also have children at higher levels.
			 * An empty string is allowed as a key in JSON, but not as a subscript in M (by default).
			 */
	iterator = yed_get_obj_iter(jansson_object);
	while (NULL != iterator)
	{
		cur_subs_used = subs_used;
		key = yed_obj_next_key(iterator);
		if (0 != strcmp(key, root))	/* If this is the value that shares a variable name
						 * and subscripts with the subtree, we don't want to
						 * add an empty string ("") subscript.
						 */
		{
			if (YDB_MAX_SUBS == cur_subs_used)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
			str_size = strlen(key);
			if (subsarray[cur_subs_used].len_alloc < str_size)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
					LEN_AND_LIT("buf_addr is too small for at least 1 key in JSON input"),
					LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
			else
			{
				subsarray[cur_subs_used].len_used = str_size;
				memcpy(subsarray[cur_subs_used].buf_addr, key, str_size);
				cur_subs_used++;
			}
		}
		value = yed_obj_next_value(iterator);
		switch (value->type)
		{
			case JSON_OBJECT:	/* object */
				status = yed_decode_object(varname, cur_subs_used, subsarray, value);
				break;
			case JSON_ARRAY:	/* array */
				status = yed_decode_array(varname, cur_subs_used, subsarray, value);
				break;
			case JSON_STRING:	/* string */
				status = yed_decode_string(varname, cur_subs_used, subsarray, value);
				break;
			case JSON_INTEGER:	/* integer */
				status = yed_decode_integer(varname, cur_subs_used, subsarray, value);
				break;
			case JSON_REAL:		/* real */
				status = yed_decode_real(varname, cur_subs_used, subsarray, value);
				break;
			case JSON_TRUE:		/* true */
				status = yed_decode_bool(varname, cur_subs_used, subsarray, 1);
				break;
			case JSON_FALSE:	/* false */
				status = yed_decode_bool(varname, cur_subs_used, subsarray, 0);
				break;
			default:		/* null */
				status = yed_decode_bool(varname, cur_subs_used, subsarray, 2);
		}
		if (YDB_OK != status)
			break;
		iterator = yed_obj_iter_next(jansson_object, iterator);
	}
	return status;
}

int yed_decode_array(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, json_t *jansson_object)
{
	json_t	*value;
	int	cur_subs_used, status = YDB_OK;
	size_t	array_size;

	if (YDB_MAX_SUBS == subs_used)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
	array_size = yed_get_size(jansson_object);
	for (int i = 0; i < array_size; i++)
	{
		cur_subs_used = subs_used;
		subsarray[cur_subs_used].len_used = snprintf(NULL, 0, "%d", i);
		/*
		 * In yed_decode_object() there is error handling to prevent a JSON key coming from the
		 * decoder, that is longer than the allocation for a subscript in the subsarray, from being
		 * copied in to it. Here, that is not necessary because instead of an arbitrary key, the
		 * subscript that is stored in the subsarray is an integer representing the position in the
		 * array.
		 *
		 * The allocation size is either the actual max_key_size for the region if it's a global
		 * and coming from op_zydecode(), the MAX_KEY_SZ for any region if it's a global and not
		 * coming from op_zydecode(), or YDB_MAX_STR if it's a local.
		 *
		 * In the case of a global, the allocation, being the key size, will always be larger than a
		 * single subscript stored in subsarray, and will return a GVSUBOFLOW error well before it
		 * would get to a string representing a position in the JSON array being decoded, because the
		 * key includes extra bytes for the key encoding. The key size includes the global name, and
		 * all of the subscripts, but even if there is just one subscript that is really long, it
		 * still will hit the GVSUBOFLOW error elsewhere before overflowing the allocation in subsarray.
		 *
		 * In the case of a local, the allocation is 1 MiB, and that can store a JSON array position that
		 * is so ridiculously large that it is literally impossible. The JSON array would have to contain
		 * more than 10^1048576 items!
		 *
		 * So we use an assert to capture the invariant instead.
		 */
		assert(subsarray[cur_subs_used].len_used < subsarray[cur_subs_used].len_alloc);
		snprintf(subsarray[cur_subs_used].buf_addr, subsarray[cur_subs_used].len_used + 1, "%d", i);
		cur_subs_used++;
		value = yed_get_value(jansson_object, (size_t)i);
		switch (value->type)
		{
			case JSON_OBJECT:	/* object */
				status = yed_decode_object(varname, cur_subs_used, subsarray, value);
				break;
			case JSON_ARRAY:	/* array */
				status = yed_decode_array(varname, cur_subs_used, subsarray, value);
				break;
			case JSON_STRING:	/* string */
				status = yed_decode_string(varname, cur_subs_used, subsarray, value);
				break;
			case JSON_INTEGER:	/* integer */
				status = yed_decode_integer(varname, cur_subs_used, subsarray, value);
				break;
			case JSON_REAL:		/* real */
				status = yed_decode_real(varname, cur_subs_used, subsarray, value);
				break;
			case JSON_FALSE:	/* false */
				status = yed_decode_bool(varname, cur_subs_used, subsarray, 0);
				break;
			case JSON_TRUE:		/* true */
				status = yed_decode_bool(varname, cur_subs_used, subsarray, 1);
				break;
			default:		/* null */
				status = yed_decode_bool(varname, cur_subs_used, subsarray, 2);
		}
		if (YDB_OK != status)
			break;
	}
	return status;
}

int yed_decode_string(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, json_t *jansson_object)
{
	const char	*value;
	ydb_buffer_t	value_buffer = {0};
	char		done;
	int		status, record_size;
	long long	value_ll;
	double		value_d;
	size_t		value_len;

	value = yed_get_string_value(jansson_object);
	assert(NULL != value);	/* This function should only be called if jansson_object was previously confirmed to be a string */
	value_buffer.len_used = value_buffer.len_alloc = value_len = strlen(value);
	value_buffer.len_alloc++;	/* Add 1 for the NUL */
	value_buffer.buf_addr = (char *)value;
	if (yed_is_integer(value_buffer, &value_ll) || yed_is_real(value_buffer, &value_d))
	{
		value_buffer.len_alloc++;	/* Add 1 for the NUL character that will be prepended */
		value_buffer.len_used++;	/* Add 1 for the NUL character that will be prepended */
		value_buffer.buf_addr = system_malloc(value_buffer.len_alloc);
		value_buffer.buf_addr[0] = YED_NUL;	/* Prepend a NUL to encode JSON strings that match canonical numbers */
		memcpy(&value_buffer.buf_addr[1], (char *)value, value_buffer.len_used);	/* len_used is length of value */
	}
	if ((LYDB_VARREF_GLOBAL == decode_type) && zydecode_args)	/* zydecode_args is > 0 if called by op_zydecode() */
		record_size = dglvnp->gblp[0]->s_gv_cur_region->max_rec_size;
	else
		record_size = YDB_MAX_STR;
	if (record_size < value_buffer.len_used)
	{
		/* This indicates that a NUL was prepended to encode this, so we need to free it */
		if (value_len < value_buffer.len_used)
			system_free(value_buffer.buf_addr);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
			LEN_AND_LIT("Size for string value is > max string size in JSON input"),
			LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_DECODE)));
	}
	status = ydb_set_s(varname, subs_used, subsarray, &value_buffer);
	if (value_len < value_buffer.len_used)	/* This indicates that a NUL was prepended to encode this, so we need to free it */
		system_free(value_buffer.buf_addr);
	return status;
}

int yed_decode_integer(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, json_t *jansson_object)
{
	long long	value;
	ydb_buffer_t	value_buffer;
	char		buffer[YED_INTEGER_MAX_LEN];

	value = yed_get_int_value(jansson_object);
	value_buffer.len_alloc = YED_INTEGER_MAX_LEN;	/* Jansson uses long long to store integers */
	value_buffer.buf_addr = (char *)&buffer[0];
	value_buffer.len_used = snprintf(value_buffer.buf_addr, value_buffer.len_alloc, "%lld", value);
	/* Match the precision in the encoder - extra for minus sign, et al. */
	if (('-' == value_buffer.buf_addr[0]) && ((YED_PRECISION + 2) < value_buffer.len_used))
		value_buffer.len_used = YED_PRECISION + 2;
	else if ((YED_PRECISION + 1) < value_buffer.len_used)
		value_buffer.len_used = YED_PRECISION + 1;
	return ydb_set_s(varname, subs_used, subsarray, &value_buffer);
}

int yed_decode_real(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, json_t *jansson_object)
{
	double		value;
	ydb_buffer_t	value_buffer;
	char		buffer[YED_REAL_MAX_LEN];

	value = yed_get_real_value(jansson_object);
	value_buffer.len_alloc = YED_REAL_MAX_LEN;	/* Jansson uses double to store reals */
	value_buffer.buf_addr = (char *)&buffer[0];
	/* The call to snprintf() matches the precision in the encoder */
	value_buffer.len_used = snprintf(value_buffer.buf_addr, value_buffer.len_alloc, "%.*g", YED_PRECISION, value);
	/* Remove leading 0 which is not allowed in M */
	if (('0' == value_buffer.buf_addr[0]) && ('.' == value_buffer.buf_addr[1]))
		memmove(value_buffer.buf_addr, value_buffer.buf_addr + 1, --value_buffer.len_used);
	/* Remove leading 0 after minus sign which is not allowed in M */
	else if (('-' == value_buffer.buf_addr[0]) && ('0' == value_buffer.buf_addr[1]) && ('.' == value_buffer.buf_addr[2]))
		memmove(value_buffer.buf_addr + 1, value_buffer.buf_addr + 2, --value_buffer.len_used);
	return ydb_set_s(varname, subs_used, subsarray, &value_buffer);
}

/*
 * This function handles the JSON value types of false, true, and null (not
 * strings). M does not support the false, true, and null JSON data types, so
 * we have to distinguish between false and "false", true and "true", null and
 * "null" while stored in M. So these values are stored as $char(0)_"true",
 * $char(0)_"false", and $char(0)_"null" in M arrays. A $char(0) character is
 * supported as data in M arrays, but is not supported as a value when decoding
 * in Jansson by default. Thus a value such as "\0_true" cannot be decoded when
 * used as a JSON value in Jansson, and this encoding is therefore unambiguous.
 * Storing the NUL byte at the beginning instead of the end of the string makes
 * it less likely that it will be confused for a normal C string terminator.
 *
 *	bool_type is one of:
 *		0 for false
 *		1 for true
 *		2 for null
 */
int yed_decode_bool(const ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, int bool_type)
{
	ydb_buffer_t	value_buffer;

	switch (bool_type)
	{
		case 0:
			value_buffer.len_alloc = SIZEOF(YED_JSON_FALSE);
			value_buffer.len_used = STR_LIT_LEN(YED_JSON_FALSE);
			value_buffer.buf_addr = YED_JSON_FALSE;
			break;
		case 1:
			value_buffer.len_alloc = SIZEOF(YED_JSON_TRUE);
			value_buffer.len_used = STR_LIT_LEN(YED_JSON_TRUE);
			value_buffer.buf_addr = YED_JSON_TRUE;
			break;
		default:
			value_buffer.len_alloc = SIZEOF(YED_JSON_NULL);
			value_buffer.len_used = STR_LIT_LEN(YED_JSON_NULL);
			value_buffer.buf_addr = YED_JSON_NULL;
			break;
	}
	return ydb_set_s(varname, subs_used, subsarray, &value_buffer);
}

/*
 * This function code is from json_decref() in jansson.h. We can't call it directly, because it is a static inline
 * function defined in jansson.h, and it calls the json_delete() symbol, which is a regular visible symbol defined
 * in the library. Since we only load the Jansson library when ydb_encode_s() or ydb_decode_s() are invoked, it is
 * loaded at run-time, using the dlsym(3) API. Thus it requires use of function pointers to call through to the
 * symbols that are lazily loaded. The json_delete() function is pointed to by the yed_object_delete() pointer, so
 * when json_decref() calls json_delete(), it is undefined, and we cannot redefine symbols. So instead we duplicate
 * the code from json_decref() in jansson.h here as yed_object_decref(), which hasn't changed in a long time.
 */
void yed_object_decref(json_t *json)
{
	if (json && json->refcount != (size_t)-1 && JSON_INTERNAL_DECREF(json) == 0)
		yed_object_delete(json);
	return;
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
