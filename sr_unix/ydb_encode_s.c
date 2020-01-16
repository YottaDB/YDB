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

#include "op.h"
#include "stringpool.h"
#include "libyottadb_int.h"
#include "namelook.h"
#include "dlopen_handle_array.h"
#include "ydb_encode_decode.h"
#include "real_len.h"
#include "fgncal.h"		/* Needed for MAX_ERRSTR_LEN */
#include "gdsfhead.h"
#include "mvalconv.h"		/* Needed for mval2i */
#include "outofband.h"

GBLREF	volatile int4	outofband;

int ydb_encode_s(const ydb_buffer_t *varname, int subs_used, const ydb_buffer_t *subsarray, const char *format, ydb_buffer_t *ret_value)
{
	boolean_t	error_encountered;
	ydb_var_types	encode_type;
	void		*handle;
	unsigned int	value_and_subtree;
	int		encode_svn_index, status, i;
	json_t		*(*new_object)(void);
	size_t		(*output_json)(const json_t *, char *, size_t, size_t);
	json_t		*obj;
	char		*dlerror_val;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	VERIFY_NON_THREADED_API;	/* clears a global variable "caller_func_is_stapi" set by SimpleThreadAPI caller
					 * so needs to be first invocation after SETUP_THREADGBL_ACCESS to avoid any error
					 * scenarios from not resetting this global variable even though this function returns.
					 */
	/* Verify entry conditions, make sure YDB CI environment is up etc. */
	LIBYOTTADB_INIT(LYDB_RTN_ENCODE, (int));		/* Note: macro could return from this function in case of errors */
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
	VALIDATE_VARNAME(varname, encode_type, encode_svn_index, FALSE);
	if (0 > subs_used)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MINNRSUBSCRIPTS);
		return YDB_ERR_MINNRSUBSCRIPTS;
	}
	if (YDB_MAX_SUBS < subs_used)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
		return YDB_ERR_MAXNRSUBSCRIPTS;
	}
	if (NULL == ret_value)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
			LEN_AND_LIT("NULL ret_value"), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_ENCODE)));
		return YDB_ERR_PARAMINVALID;
	}
	if (NULL == ret_value->buf_addr && 0 != ret_value->len_used)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
			LEN_AND_LIT("NULL ret_value->buf_addr and non-zero ret_value->len_used"),
			LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_ENCODE)));
		return YDB_ERR_PARAMINVALID;
	}
	if (ret_value->len_alloc < ret_value->len_used)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
			LEN_AND_LIT("ret_value->len_alloc < ret_value->len_used"),
			LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_ENCODE)));
		return YDB_ERR_PARAMINVALID;
	}
	for (i = 0; i < subs_used; i++)
	{
		if (subsarray[i].len_alloc < subsarray[i].len_used)
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
				LEN_AND_LIT("len_alloc < len_used for at least 1 subscript in subsarray"),
				LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_ENCODE)));
			return YDB_ERR_PARAMINVALID;
		}
		if ((0 != subsarray[i].len_used) && (NULL == subsarray[i].buf_addr))
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
				LEN_AND_LIT("len_used is non-zero and buff_addr is NULL for at least 1 subscript in subsarray"),
				LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_ENCODE)));
			return YDB_ERR_PARAMINVALID;
		}
	}
	value_and_subtree = get_value_and_subtree(varname, subs_used, subsarray, encode_type, (char *)LYDBRTNNAME(LYDB_RTN_ENCODE));
	if (value_and_subtree < 10) /* no subtree */
	{
		switch (encode_type)
		{
			case LYDB_VARREF_LOCAL:
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_LVUNDEF);
				return YDB_ERR_LVUNDEF;
				break;
			case LYDB_VARREF_GLOBAL:
			default:
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_GVUNDEF);
				return YDB_ERR_GVUNDEF;
				break;
		}
	}
	handle = dlopen("libjansson.so", RTLD_LAZY);
	if (NULL == handle)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JANSSONDLNOOPEN, 4,
			LEN_AND_LIT("attempt to open jansson failed"),
			LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_ENCODE)));
		return YDB_ERR_JANSSONDLNOOPEN;
	}
	dlopen_handle_array_add(handle);
	new_object = dlsym(handle, "json_object");
	output_json = dlsym(handle, "json_dumpb");
	obj = new_object();
	status = encode_tree(varname, subs_used, subsarray, encode_type, encode_svn_index, value_and_subtree, obj, NULL, NULL);
	if (YDB_OK != status)
		return status;
	ret_value->len_used = output_json(obj, ret_value->buf_addr, ret_value->len_alloc, 0);
	LIBYOTTADB_DONE;
	REVERT;
	return YDB_OK;
}

int encode_tree(const ydb_buffer_t *varname, int subs_used, const ydb_buffer_t *subsarray, ydb_var_types encode_type,
	int encode_svn_index, int value_and_subtree, json_t *obj, int *ret_subs_used, ydb_buffer_t *ret_subsarray)
{
	ydb_buffer_t	cur_value, cur_subsarray[MAX_GVSUBSCRIPTS], next_subsarray[MAX_LVSUBSCRIPTS];
	char		*root, *key;
	int		i, status, return_code, cur_subs_used, next_subs_used, more_subs_used;
	long long	value_ll;
	double		value_d;
	json_t		*cur, *val;
	void		*handle;
	int		(*set_object)(json_t *, const char *, json_t *), (*set_string)(json_t *, const char *, size_t);
	int		(*set_integer)(const json_t *, long long), (*set_real)(const json_t *, double);
	int		(*append_to_array)(json_t *, json_t *);
	json_t		*(*new_object)(void), *(*new_array)(void), *(*new_string)(const char *, size_t), *(*new_integer)(long long);
	json_t		*(*new_real)(double), *(*new_true)(void), *(*new_false)(void), *(*new_null)(void);
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	cur = obj;
	root = "_root";
	handle = dlopen("libjansson.so", RTLD_LAZY);
	if (NULL == handle)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JANSSONDLNOOPEN, 4,
			LEN_AND_LIT("attempt to open jansson failed"),
			LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_ENCODE)));
		return YDB_ERR_JANSSONDLNOOPEN;
	}
	dlopen_handle_array_add(handle);
	new_object = dlsym(handle, "json_object");
	new_array = dlsym(handle, "json_array");
	new_string = dlsym(handle, "json_stringn");
	new_integer = dlsym(handle, "json_integer");
	new_real = dlsym(handle, "json_real");
	new_true = dlsym(handle, "json_true");
	new_false = dlsym(handle, "json_false");
	new_null = dlsym(handle, "json_null");
	set_object = dlsym(handle, "json_object_set_new");
	set_string = dlsym(handle, "json_string_setn");
	set_integer = dlsym(handle, "json_integer_set");
	set_real = dlsym(handle, "json_real_set");
	append_to_array = dlsym(handle, "json_array_append_new");
	for (i = 0; i < MAX_GVSUBSCRIPTS; i++)
	{
		cur_subsarray[i].len_used = subsarray[i].len_used;
		next_subsarray[i].len_used = 0;
		cur_subsarray[i].len_alloc = MAX_LVSUBSCRIPTS;
		next_subsarray[i].len_alloc = MAX_LVSUBSCRIPTS;
		ENSURE_STP_FREE_SPACE(MAX_LVSUBSCRIPTS);
		cur_subsarray[i].buf_addr = (char *)stringpool.free;
		memcpy(cur_subsarray[i].buf_addr, subsarray[i].buf_addr, cur_subsarray[i].len_used);
		stringpool.free += MAX_LVSUBSCRIPTS;
		assert(stringpool.free <= stringpool.top);
		ENSURE_STP_FREE_SPACE(MAX_LVSUBSCRIPTS);
		next_subsarray[i].buf_addr = (char *)stringpool.free;
		stringpool.free += MAX_LVSUBSCRIPTS;
		assert(stringpool.free <= stringpool.top);
	}
	cur_subs_used = subs_used;
	cur_value.len_used = 0;
	cur_value.len_alloc = 0;

	if (11 == value_and_subtree) /* handle root of tree's value if it has a value */
	{
		ydb_get_value(varname, subs_used, subsarray, &cur_value, encode_type, encode_svn_index, YDB_GET_VALUE_SIZE_ONLY,
			(char *)LYDBRTNNAME(LYDB_RTN_ENCODE));
		ENSURE_STP_FREE_SPACE(cur_value.len_used);
		cur_value.len_alloc = cur_value.len_used;
		cur_value.buf_addr = (char *)stringpool.free;
		stringpool.free += cur_value.len_used;
		assert(stringpool.free <= stringpool.top);
		ydb_get_value(varname, subs_used, subsarray, &cur_value, encode_type, encode_svn_index, YDB_GET_VALUE_FULL_VALUE,
			(char *)LYDBRTNNAME(LYDB_RTN_ENCODE));
		if (is_integer(cur_value, &value_ll))
		{
			val = new_integer(value_ll);
			assert(val != NULL);
		}
		else if (is_real(cur_value, &value_d))
		{
			val = new_real(value_d);
			assert(val != NULL);
		}
		else if (is_true(cur_value))
			val = new_true();
		else if (is_false(cur_value))
			val = new_false();
		else if (is_null(cur_value))
			val = new_null();
		else
			val = new_string(cur_value.buf_addr, cur_value.len_used);
		return_code = set_object(cur, root, val);
		assert(0 == return_code);
	}
	assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* previously unused entries should have been cleared by that
							 * corresponding ydb_*_s() call or iteration of this loop.
							 */
	if (0 > cur_subs_used)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MINNRSUBSCRIPTS);
	if (YDB_MAX_SUBS < cur_subs_used)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
	next_subs_used = YDB_MAX_SUBS;
	status = get_next_node(varname, cur_subs_used, cur_subsarray, &next_subs_used, next_subsarray, encode_type,
		(char *)LYDBRTNNAME(LYDB_RTN_ENCODE));
	while (0 != next_subs_used)
	{
		assert(YDB_OK == status);
		if (!is_descendent_of(subs_used, subsarray, next_subs_used, next_subsarray))
		{	/* The next node is not part of this subtree. Return. */
			if(NULL != ret_subs_used)
			{	/* If this isn't the top level call to this function, update the return subsarray for the calling function */
				*ret_subs_used = cur_subs_used;
				for (i = 0; i < cur_subs_used; i++)
				{
					ret_subsarray[i].len_used = cur_subsarray[i].len_used;
					memcpy(ret_subsarray[i].buf_addr, cur_subsarray[i].buf_addr, ret_subsarray[i].len_used);
				}
			}
			return status;
		}
		assert(next_subs_used > subs_used);
		value_and_subtree = get_value_and_subtree(varname, next_subs_used, next_subsarray, encode_type, (char *)LYDBRTNNAME(LYDB_RTN_ENCODE));
		if(!is_direct_child_of(subs_used, subsarray, next_subs_used, next_subsarray))
		{	/* The next node is not a direct child of this one. The nodes in between should be represented as objects. */
			value_and_subtree = get_value_and_subtree(varname, subs_used + 1, next_subsarray, encode_type, (char *)LYDBRTNNAME(LYDB_RTN_ENCODE));
			val = new_object();
			key = get_key(next_subsarray[subs_used]);
			return_code = set_object(cur, key, val);
			assert(0 == return_code);
			status = encode_tree(varname, (subs_used + 1), next_subsarray, encode_type, encode_svn_index, value_and_subtree, val, &cur_subs_used, cur_subsarray);
			assert(YDB_OK == status);
		}
		else if (value_and_subtree > 9)
		{	/* Has a subtree. Should be represented as an object. */
			val = new_object();
			key = get_key(next_subsarray[subs_used]);
			return_code = set_object(cur, key, val);
			assert(0 == return_code);
			status = encode_tree(varname, next_subs_used, next_subsarray, encode_type, encode_svn_index, value_and_subtree, val, &cur_subs_used, cur_subsarray);
			assert(YDB_OK == status);
		}
		else
		{	/* No subtree. Represent as an int, real, string or boolean value. */
			ydb_get_value(varname, next_subs_used, next_subsarray, &cur_value, encode_type, encode_svn_index, YDB_GET_VALUE_SIZE_ONLY,
				(char *)LYDBRTNNAME(LYDB_RTN_ENCODE));
			if (cur_value.len_used > cur_value.len_alloc)
			{
				ENSURE_STP_FREE_SPACE(cur_value.len_used);
				cur_value.len_alloc = cur_value.len_used;
				cur_value.buf_addr = (char *)stringpool.free;
				stringpool.free += cur_value.len_used;
				assert(stringpool.free <= stringpool.top);
			}
			ydb_get_value(varname, next_subs_used, next_subsarray, &cur_value, encode_type, encode_svn_index, YDB_GET_VALUE_FULL_VALUE,
				(char *)LYDBRTNNAME(LYDB_RTN_ENCODE));
			if (is_integer(cur_value, &value_ll))
			{
				val = new_integer(value_ll);
				assert(val != NULL);
			}
			else if (is_real(cur_value, &value_d))
			{
				val = new_real(value_d);
				assert(val != NULL);
			}
			else if (is_true(cur_value))
				val = new_true();
			else if (is_false(cur_value))
				val = new_false();
			else if (is_null(cur_value))
				val = new_null();
			else
				val = new_string(cur_value.buf_addr, cur_value.len_used);
			key = get_key(next_subsarray[next_subs_used - 1]);
			return_code = set_object(cur, key, val);
			assert(0 == return_code);
			cur_subs_used = next_subs_used;
			for (i = 0; i < cur_subs_used; i++)
			{
				cur_subsarray[i].len_used = next_subsarray[i].len_used;
				memcpy(cur_subsarray[i].buf_addr, next_subsarray[i].buf_addr, cur_subsarray[i].len_used);
			}
		}
		assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* previously unused entries should have been cleared by that
								 * corresponding ydb_*_s() call or iteration of this loop.
								 */
		if (0 > cur_subs_used)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MINNRSUBSCRIPTS);
 		if (YDB_MAX_SUBS < cur_subs_used)
 			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
		next_subs_used = YDB_MAX_SUBS;
		status = get_next_node(varname, cur_subs_used, cur_subsarray, &next_subs_used, next_subsarray, encode_type,
			(char *)LYDBRTNNAME(LYDB_RTN_ENCODE));
	}

	if(ret_subs_used != NULL)
	{	/* If this isn't the top level call to this function, update the return subsarray for the calling function */
		*ret_subs_used = cur_subs_used;
		for (i = 0; i < cur_subs_used; i++)
		{
			ret_subsarray[i].len_used = cur_subsarray[i].len_used;
			memcpy(ret_subsarray[i].buf_addr, cur_subsarray[i].buf_addr, ret_subsarray[i].len_used);
		}
	}
	return YDB_OK;
}

boolean_t is_integer(ydb_buffer_t buff, long long *value)
{
	long long ll;
	int i;
	char* str;
	char* endptr;

	ENSURE_STP_FREE_SPACE(buff.len_used + 1);
	str = (char *)stringpool.free;
	stringpool.free += (buff.len_used + 1);
	assert(stringpool.free <= stringpool.top);
	ENSURE_STP_FREE_SPACE(buff.len_used + 1);
	endptr = (char *)stringpool.free;
	stringpool.free += (buff.len_used + 1);
	assert(stringpool.free <= stringpool.top);
	for (i = 0; i < buff.len_used; i++)
	{
		str[i] = buff.buf_addr[i];
	}
	str[buff.len_used] = '\0';
	ll = strtoll(str, &endptr, 10);
	*value = ll;
	return (0 == strcmp(endptr, "\0"));
}

boolean_t is_real(ydb_buffer_t buff, double *value)
{
	double d;
	int i;
	char* str;
	char* endptr;

	ENSURE_STP_FREE_SPACE(buff.len_used + 1);
	str = (char *)stringpool.free;
	stringpool.free += (buff.len_used + 1);
	assert(stringpool.free <= stringpool.top);
	ENSURE_STP_FREE_SPACE(buff.len_used + 1);
	endptr = (char *)stringpool.free;
	stringpool.free += (buff.len_used + 1);
	assert(stringpool.free <= stringpool.top);
	for (i = 0; i < buff.len_used; i++)
	{
		str[i] = buff.buf_addr[i];
	}
	str[buff.len_used] = '\0';
	d = strtod(str, &endptr);
	*value = d;
	return (0 == strcmp(endptr, "\0"));
}

boolean_t is_true(ydb_buffer_t buff)
{
	if (buff.len_used != 4)
		return FALSE;
	if (buff.buf_addr[0] != 't' && buff.buf_addr[0] != 'T')
		return FALSE;
	if (buff.buf_addr[1] != 'r' && buff.buf_addr[1] != 'R')
		return FALSE;
	if (buff.buf_addr[2] != 'u' && buff.buf_addr[2] != 'U')
		return FALSE;
	if (buff.buf_addr[3] != 'e' && buff.buf_addr[3] != 'E')
		return FALSE;
	return TRUE;
}

boolean_t is_false(ydb_buffer_t buff)
{
	if (buff.len_used != 5)
		return FALSE;
	if (buff.buf_addr[0] != 'f' && buff.buf_addr[0] != 'F')
		return FALSE;
	if (buff.buf_addr[1] != 'a' && buff.buf_addr[1] != 'A')
		return FALSE;
	if (buff.buf_addr[2] != 'l' && buff.buf_addr[2] != 'L')
		return FALSE;
	if (buff.buf_addr[3] != 's' && buff.buf_addr[3] != 'S')
		return FALSE;
	if (buff.buf_addr[4] != 'e' && buff.buf_addr[4] != 'E')
		return FALSE;
	return TRUE;
}

boolean_t is_null(ydb_buffer_t buff)
{
	if (buff.len_used != 4)
		return FALSE;
	if (buff.buf_addr[0] != 'n' && buff.buf_addr[0] != 'N')
		return FALSE;
	if (buff.buf_addr[1] != 'u' && buff.buf_addr[1] != 'U')
		return FALSE;
	if (buff.buf_addr[2] != 'l' && buff.buf_addr[2] != 'L')
		return FALSE;
	if (buff.buf_addr[3] != 'l' && buff.buf_addr[3] != 'L')
		return FALSE;
	return TRUE;
}

boolean_t is_direct_child_of(int subs_used, const ydb_buffer_t *subsarray, int next_subs_used, ydb_buffer_t *next_subsarray)
{
	int i, j;
	if ((subs_used + 1) != next_subs_used)
		return FALSE;

	for (i = 0; i < subs_used; i++)
	{
		if (subsarray[i].len_used != next_subsarray[i].len_used)
			return FALSE;
		for (j = 0; j < subsarray[i].len_used; j++)
		{
			if(subsarray[i].buf_addr[j] != next_subsarray[i].buf_addr[j])
				return FALSE;
		}
	}
	return TRUE;
}

boolean_t is_descendent_of(int subs_used, const ydb_buffer_t *subsarray, int next_subs_used, ydb_buffer_t *next_subsarray)
{
	int i, j;
	if (subs_used > next_subs_used)
		return FALSE;

	for (i = 0; i < subs_used; i++)
	{
		if (subsarray[i].len_used != next_subsarray[i].len_used)
			return FALSE;
		for (j = 0; j < subsarray[i].len_used; j++)
		{
			if(subsarray[i].buf_addr[j] != next_subsarray[i].buf_addr[j])
				return FALSE;
		}
	}
	return TRUE;
}


char *get_key(ydb_buffer_t buffer)
{
	int	i;
	char *	str;

	ENSURE_STP_FREE_SPACE(buffer.len_used + 1);
	str = (char *)stringpool.free;
	stringpool.free += (buffer.len_used + 1);
	assert(stringpool.free <= stringpool.top);
	for (i = 0; i < buffer.len_used; i++)
	{
		str[i] = buffer.buf_addr[i];
	}
	str[buffer.len_used] = '\0';
	return str;
}
