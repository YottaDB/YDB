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

#include "libyottadb_int.h"
#include "ydb_encode_decode.h"
#include "namelook.h"
#include "deferred_events_queue.h"

GBLREF	volatile int4	outofband;

STATICDEF	char	*errmsg = NULL;

/* Routine to encode a local or global in to a formatted string
 *
 * Parameters:
 *   varname    - Gives name of local or global variable
 *   subs_used	- Count of subscripts already setup for source array (subtree root)
 *   subsarray  - an array of subscripts used to process source array, already containing "subs_used" subscripts
 *   format	- Format of string to be encoded (currently always "JSON" and ignored - for future use)
 *   ret_value	- Value fetched from local/global variable encoded in to a formatted string stored/returned here
 */
int	ydb_encode_s(const ydb_buffer_t *varname, int subs_used, const ydb_buffer_t *subsarray,
			const char *format, ydb_buffer_t *ret_value)
{
	boolean_t	error_encountered;
	ydb_var_types	encode_type;
	unsigned int	data_value;
	int		encode_svn_index, status = YDB_OK;
	json_t		*jansson_object = NULL;
	size_t		size;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(!yed_lydb_rtn);	/* ydb_encode_s() and ydb_decode_s() set to TRUE, and they should never be nested */
	VERIFY_NON_THREADED_API;	/* clears a global variable "caller_func_is_stapi" set by SimpleThreadAPI caller
					 * so needs to be first invocation after SETUP_THREADGBL_ACCESS to avoid any error
					 * scenarios from not resetting this global variable even though this function returns.
					 */
	/* Verify entry conditions, make sure YDB CI environment is up etc. */
	LIBYOTTADB_INIT(LYDB_RTN_ENCODE, (int));	/* Note: macro could return from this function in case of errors */
	assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* previously unused entries should have been cleared by that
							 * corresponding ydb_*_s() call.
							 */
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{
		if (NULL != errmsg)
			system_free(errmsg);
		/* This code is from json_decref() in jansson.h, it calls json_delete() - but we can't call it directly.
		 * yed_object_delete() is a function pointer that is set by dlsym(3) to point to json_delete(), but since
		 * json_decref() is a static inline function defined in jansson.h, it calls json_delete() and we can't
		 * redefine symbols.
		 */
		if (jansson_object && jansson_object->refcount != (size_t)-1 && JSON_INTERNAL_DECREF(jansson_object) == 0)
			yed_object_delete(jansson_object);
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
	VALIDATE_VARNAME(varname, subs_used, FALSE, LYDB_RTN_ENCODE, -1, encode_type, encode_svn_index);
	if (0 > subs_used)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MINNRSUBSCRIPTS);
	if (YDB_MAX_SUBS < subs_used)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
	if (NULL == ret_value)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
			LEN_AND_LIT("NULL ret_value"), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_ENCODE)));
	if (NULL == ret_value->buf_addr && 0 != ret_value->len_used)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
			LEN_AND_LIT("NULL ret_value->buf_addr and non-zero ret_value->len_used"),
			LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_ENCODE)));
	if (ret_value->len_alloc < ret_value->len_used)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
			LEN_AND_LIT("ret_value->len_alloc < ret_value->len_used"),
			LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_ENCODE)));
	for (int i = 0; i < subs_used; i++)
	{
		if (subsarray[i].len_alloc < subsarray[i].len_used)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
				LEN_AND_LIT("len_alloc < len_used for at least 1 subscript in subsarray"),
				LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_ENCODE)));
		if ((0 != subsarray[i].len_used) && (NULL == subsarray[i].buf_addr))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
				LEN_AND_LIT("len_used is non-zero and buff_addr is NULL for at least 1 subscript in subsarray"),
				LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_ENCODE)));
	}
	ydb_data_s(varname, subs_used, subsarray, &data_value);
	if (YDB_DATA_UNDEF == data_value)	/* no subtree */
	{
		switch (encode_type)
		{
			case LYDB_VARREF_LOCAL:
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_LVUNDEF);
				break;
			case LYDB_VARREF_GLOBAL:
			default:
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_GVUNDEF);
				break;
		}
	}
	if (!yed_dl_complete)
		yed_dl_load((char *)LYDBRTNNAME(LYDB_RTN_ENCODE));
	jansson_object = yed_new_object();
	status = yed_encode_tree(varname, subs_used, subsarray, encode_type, encode_svn_index,
			data_value, jansson_object, NULL, NULL);
	if (YDB_OK != status)
	{
		switch (status)
		{
			case ERR_MINNRSUBSCRIPTS:
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MINNRSUBSCRIPTS);
			case ERR_MAXNRSUBSCRIPTS:
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
			case ERR_JANSSONENCODEERROR:
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JANSSONENCODEERROR, 4,
					LEN_AND_STR(errmsg), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_ENCODE)));
			default:
				if (jansson_object && jansson_object->refcount != (size_t)-1 &&
					JSON_INTERNAL_DECREF(jansson_object) == 0)
						yed_object_delete(jansson_object);
				assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* Should have been cleared by "ydb_simpleapi_ch" */
				yed_lydb_rtn = FALSE;
				REVERT;
				return status;
		}
	}
	size = yed_output_json(jansson_object, ret_value->buf_addr, ret_value->len_alloc, 0);
	if (0 == size)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JANSSONINVALIDJSON, 4,
			LEN_AND_LIT("Empty JSON returned"), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_ENCODE)));
	if (ret_value->len_alloc < size + 1)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_INVSTRLEN, 2, size + 1, ret_value->len_alloc);
	/* This code is from json_decref() in jansson.h, it calls json_delete() - but we can't call it directly.
	 * yed_object_delete() is a function pointer that is set by dlsym(3) to point to json_delete(), but since
	 * json_decref() is a static inline function defined in jansson.h, it calls json_delete() and we can't
	 * redefine symbols.
	 */
	if (jansson_object && jansson_object->refcount != (size_t)-1 && JSON_INTERNAL_DECREF(jansson_object) == 0)
		yed_object_delete(jansson_object);
	ret_value->len_used = size;
	ret_value->buf_addr[ret_value->len_used] = '\0';
	assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* the counter should have never become non-zero in this function */
	yed_lydb_rtn = FALSE;
	LIBYOTTADB_DONE;
	REVERT;
	return status;
}

int yed_encode_tree(const ydb_buffer_t *varname, int subs_used, const ydb_buffer_t *subsarray, ydb_var_types encode_type,
			int encode_svn_index, unsigned int data_value, json_t *obj, int *ret_subs_used, ydb_buffer_t *ret_subsarray)
{
	ydb_buffer_t	cur_subsarray[YDB_MAX_SUBS] = {0}, next_subsarray[YDB_MAX_SUBS] = {0}, cur_value = {0};
	const char	*root;
	long long	value_ll;
	double		value_d;
	unsigned int	size;
	int		return_code, cur_subs_used = 0, next_subs_used = 0, i, string_size, status = YDB_OK;
	json_t		*cur, *val;
	char		*curpool, *nextpool, *valuepool;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	cur = obj;
	root = "";	/* Used as a special JSON key to hold values at M array nodes that also have children at higher levels.
			 * An empty string is allowed as a key in JSON, but not as a subscript in M (by default).
			 */
	if (LYDB_VARREF_GLOBAL == encode_type)
		string_size = MAX_KEY_SZ;
	else
		string_size = YDB_MAX_STR;
	curpool = system_malloc(YDB_MAX_SUBS * string_size);
	nextpool = system_malloc(YDB_MAX_SUBS * string_size);
	for (i = 0; YDB_MAX_SUBS > i; i++)
	{
		cur_subsarray[i].len_alloc = string_size;
		cur_subsarray[i].buf_addr = &curpool[string_size * i];
		if (i < subs_used)
		{
			cur_subsarray[i].len_used = subsarray[i].len_used;
			memcpy(cur_subsarray[i].buf_addr, subsarray[i].buf_addr, cur_subsarray[i].len_used);
		}
		next_subsarray[i].len_alloc = string_size;
		next_subsarray[i].buf_addr = &nextpool[string_size * i];
	}
	cur_subs_used = subs_used;
	next_subs_used = YDB_MAX_SUBS;
	cur_value.len_alloc = YDB_MAX_STR;
	valuepool = system_malloc(cur_value.len_alloc);
	cur_value.buf_addr = valuepool;
	if (data_value % 2)	/* handle root of tree's value if it has one */
	{
		ydb_get_s(varname, subs_used, subsarray, &cur_value);
		if (0 == cur_value.len_used)
			val = yed_new_string(cur_value.buf_addr, cur_value.len_used);
		else if (yed_is_integer(cur_value, &value_ll))
			val = yed_new_integer(value_ll);
		else if (yed_is_real(cur_value, &value_d))
			val = yed_new_real(value_d);
		else if (yed_is_false(cur_value))
			val = yed_new_false();
		else if (yed_is_true(cur_value))
			val = yed_new_true();
		else if (yed_is_null(cur_value))
			val = yed_new_null();
		else
			val = yed_new_string(cur_value.buf_addr, cur_value.len_used);
		return_code = yed_set_object(cur, root, val);
		if (-1 == return_code)
		{
			char	*errsrc;
			int	len;
			mval	src, dst;

			len = cur_value.len_used;
			errsrc = system_malloc(len);
			memcpy(errsrc, cur_value.buf_addr, len);
			src.mvtype = MV_STR;
			src.str.len = len;
			src.str.addr = errsrc;
			op_fnzwrite(FALSE, &src, &dst); /* convert to ZWRITE format to show control characters */
			errmsg = system_malloc(varname->len_used + dst.str.len + 22); /* 22 for string literals & NUL */
			memcpy(errmsg, "variable = ", 11);
			len = 11;
			memcpy(&errmsg[len], varname->buf_addr, varname->len_used);
			len += varname->len_used;
			memcpy(&errmsg[len], " : data = ", 10);
			len += 10;
			memcpy(&errmsg[len], dst.str.addr, dst.str.len);
			len += dst.str.len;
			errmsg[len] = '\0';
			system_free(errsrc);
			system_free(curpool);
			system_free(nextpool);
			system_free(valuepool);
			return ERR_JANSSONENCODEERROR;
		}
	}
	if (0 > cur_subs_used)
	{
		system_free(curpool);
		system_free(nextpool);
		system_free(valuepool);
		return ERR_MINNRSUBSCRIPTS;
	}
	if (YDB_MAX_SUBS < cur_subs_used)
	{
		system_free(curpool);
		system_free(nextpool);
		system_free(valuepool);
		return ERR_MAXNRSUBSCRIPTS;
	}
	status = ydb_node_next_s(varname, cur_subs_used, cur_subsarray, &next_subs_used, next_subsarray);
	while (0 != next_subs_used)
	{
		assert(YDB_OK == status);
		if (!yed_is_descendant_of(subs_used, subsarray, next_subs_used, next_subsarray))
		{	/* The next node is not part of this subtree. Return. */
			if (NULL != ret_subs_used)
			{	/* If this isn't the top level call to this function,
				 * update the return subsarray for the calling function
				 */
				*ret_subs_used = cur_subs_used;
				for (i = 0; i < cur_subs_used; i++)
				{
					ret_subsarray[i].len_used = cur_subsarray[i].len_used;
					memcpy(ret_subsarray[i].buf_addr, cur_subsarray[i].buf_addr, ret_subsarray[i].len_used);
				}
			}
			system_free(curpool);
			system_free(nextpool);
			system_free(valuepool);
			return YDB_OK;
		}
		assert(next_subs_used > subs_used);
		ydb_data_s(varname, next_subs_used, next_subsarray, &data_value);
		if (!yed_is_direct_child_of(subs_used, subsarray, next_subs_used, next_subsarray))
		{	/* The next node is not a direct child of this one. The nodes in between should be represented as objects */
			ydb_data_s(varname, subs_used + 1, next_subsarray, &data_value);
			val = yed_new_object();
			next_subsarray[subs_used].buf_addr[next_subsarray[subs_used].len_used] = '\0';
			return_code = yed_set_object(cur, next_subsarray[subs_used].buf_addr, val);
			if (-1 == return_code)
			{
				char	*errsrc;
				int	len;
				mval	src, dst;

				len = next_subsarray[subs_used].len_used;
				errsrc = system_malloc(len);
				memcpy(errsrc, next_subsarray[subs_used].buf_addr, len);
				src.mvtype = MV_STR;
				src.str.len = len;
				src.str.addr = errsrc;
				op_fnzwrite(FALSE, &src, &dst); /* convert to ZWRITE format to show control characters */
				errmsg = system_malloc(varname->len_used + dst.str.len + 27); /* 27 for string literals & NUL */
				memcpy(errmsg, "variable = ", 11);
				len = 11;
				memcpy(&errmsg[len], varname->buf_addr, varname->len_used);
				len += varname->len_used;
				memcpy(&errmsg[len], " : subscript = ", 15);
				len += 15;
				memcpy(&errmsg[len], dst.str.addr, dst.str.len);
				len += dst.str.len;
				errmsg[len] = '\0';
				system_free(errsrc);
				system_free(curpool);
				system_free(nextpool);
				system_free(valuepool);
				return ERR_JANSSONENCODEERROR;
			}
			status = yed_encode_tree(varname, subs_used + 1, next_subsarray, encode_type,
					encode_svn_index, data_value, val, &cur_subs_used, cur_subsarray);
			if (YDB_OK != status)
			{
				system_free(curpool);
				system_free(nextpool);
				system_free(valuepool);
				return status;
			}
		}
		else if (9 < data_value)
		{	/* Has a subtree. Should be represented as an object. */
			val = yed_new_object();
			next_subsarray[subs_used].buf_addr[next_subsarray[subs_used].len_used] = '\0';
			return_code = yed_set_object(cur, next_subsarray[subs_used].buf_addr, val);
			if (-1 == return_code)
			{
				char	*errsrc;
				int	len;
				mval	src, dst;

				len = next_subsarray[subs_used].len_used;
				errsrc = system_malloc(len);
				memcpy(errsrc, next_subsarray[subs_used].buf_addr, len);
				src.mvtype = MV_STR;
				src.str.len = len;
				src.str.addr = errsrc;
				op_fnzwrite(FALSE, &src, &dst); /* convert to ZWRITE format to show control characters */
				errmsg = system_malloc(varname->len_used + dst.str.len + 27); /* 27 for string literals & NUL */
				memcpy(errmsg, "variable = ", 11);
				len = 11;
				memcpy(&errmsg[len], varname->buf_addr, varname->len_used);
				len += varname->len_used;
				memcpy(&errmsg[len], " : subscript = ", 15);
				len += 15;
				memcpy(&errmsg[len], dst.str.addr, dst.str.len);
				len += dst.str.len;
				errmsg[len] = '\0';
				system_free(errsrc);
				system_free(curpool);
				system_free(nextpool);
				system_free(valuepool);
				return ERR_JANSSONENCODEERROR;
			}
			status = yed_encode_tree(varname, next_subs_used, next_subsarray, encode_type,
					encode_svn_index, data_value, val, &cur_subs_used, cur_subsarray);
			if (YDB_OK != status)
			{
				system_free(curpool);
				system_free(nextpool);
				system_free(valuepool);
				return status;
			}
		}
		else
		{	/* No subtree. Represent as an int, real, string or boolean value. */
			ydb_get_s(varname, next_subs_used, next_subsarray, &cur_value);
			if (0 == cur_value.len_used)
				val = yed_new_string(cur_value.buf_addr, cur_value.len_used);
			else if (yed_is_integer(cur_value, &value_ll))
				val = yed_new_integer(value_ll);
			else if (yed_is_real(cur_value, &value_d))
				val = yed_new_real(value_d);
			else if (yed_is_false(cur_value))
				val = yed_new_false();
			else if (yed_is_true(cur_value))
				val = yed_new_true();
			else if (yed_is_null(cur_value))
				val = yed_new_null();
			else
				val = yed_new_string(cur_value.buf_addr, cur_value.len_used);
			next_subsarray[next_subs_used - 1].buf_addr[next_subsarray[next_subs_used - 1].len_used] = '\0';
			return_code = yed_set_object(cur, next_subsarray[next_subs_used - 1].buf_addr, val);
			if (-1 == return_code)
			{
				char	*errsrc, *errtmp;
				int	len, tmplen;
				mval	src, dst;

				len = next_subsarray[next_subs_used - 1].len_used;
				errsrc = system_malloc(len);
				memcpy(errsrc, next_subsarray[next_subs_used - 1].buf_addr, len);
				src.mvtype = MV_STR;
				src.str.len = len;
				src.str.addr = errsrc;
				op_fnzwrite(FALSE, &src, &dst); /* convert to ZWRITE format to show control characters */
				errtmp = system_malloc(dst.str.len);
				memcpy(errtmp, dst.str.addr, dst.str.len);
				tmplen = dst.str.len;
				system_free(errsrc);
				len = cur_value.len_used;
				errsrc = system_malloc(len);
				memcpy(errsrc, cur_value.buf_addr, len);
				src.mvtype = MV_STR;
				src.str.len = len;
				src.str.addr = errsrc;
				op_fnzwrite(FALSE, &src, &dst); /* convert to ZWRITE format to show control characters */
				/* 37 for string literals & NUL */
				errmsg = system_malloc(varname->len_used + tmplen + dst.str.len + 37);
				memcpy(errmsg, "variable = ", 11);
				len = 11;
				memcpy(&errmsg[len], varname->buf_addr, varname->len_used);
				len += varname->len_used;
				memcpy(&errmsg[len], " : subscript = ", 15);
				len += 15;
				memcpy(&errmsg[len], errtmp, tmplen);
				len += tmplen;
				memcpy(&errmsg[len], " : data = ", 10);
				len += 10;
				memcpy(&errmsg[len], dst.str.addr, dst.str.len);
				len += dst.str.len;
				errmsg[len] = '\0';
				system_free(errsrc);
				system_free(errtmp);
				system_free(curpool);
				system_free(nextpool);
				system_free(valuepool);
				return ERR_JANSSONENCODEERROR;
			}
			cur_subs_used = next_subs_used;
			for (i = 0; i < cur_subs_used; i++)
			{
				cur_subsarray[i].len_used = next_subsarray[i].len_used;
				memcpy(cur_subsarray[i].buf_addr, next_subsarray[i].buf_addr, cur_subsarray[i].len_used);
			}
		}
		if (0 > cur_subs_used)
		{
			system_free(curpool);
			system_free(nextpool);
			system_free(valuepool);
			return ERR_MINNRSUBSCRIPTS;
		}
		if (YDB_MAX_SUBS < cur_subs_used)
		{
			system_free(curpool);
			system_free(nextpool);
			system_free(valuepool);
			return ERR_MAXNRSUBSCRIPTS;
		}
		next_subs_used = YDB_MAX_SUBS;
		status = ydb_node_next_s(varname, cur_subs_used, cur_subsarray, &next_subs_used, next_subsarray);
	}
	if (NULL != ret_subs_used)
	{	/* If this isn't the top level call to this function, update the return subsarray for the calling function */
		*ret_subs_used = cur_subs_used;
		for (i = 0; i < cur_subs_used; i++)
		{
			ret_subsarray[i].len_used = cur_subsarray[i].len_used;
			memcpy(ret_subsarray[i].buf_addr, cur_subsarray[i].buf_addr, ret_subsarray[i].len_used);
		}
	}
	system_free(curpool);
	system_free(nextpool);
	system_free(valuepool);
	return YDB_OK;
}

inline boolean_t yed_is_integer(ydb_buffer_t buff, long long *value)
{
	char	*str, *endptr, ptr;

	if (buff.len_used > 16)
		return FALSE;
	if ('-' != buff.buf_addr[0] && '.' != buff.buf_addr[0] && !isdigit(buff.buf_addr[0]))
		return FALSE;
	for (int i = 1; i < buff.len_used; i++)
	{
		if ('.' != buff.buf_addr[i] && !isdigit(buff.buf_addr[i]))
			return FALSE;
	}
	buff.buf_addr[buff.len_used] = '\0';
	endptr = &ptr;
	*value = strtoll(buff.buf_addr, &endptr, 10);
	return (0 == strcmp(endptr, "\0"));
}

inline boolean_t yed_is_real(ydb_buffer_t buff, double *value)
{
	char	*str, *endptr, ptr;

	if (buff.len_used > 16)
		return FALSE;
	if ('-' != buff.buf_addr[0] && '.' != buff.buf_addr[0] && !isdigit(buff.buf_addr[0]))
		return FALSE;
	for (int i = 1; i < buff.len_used; i++)
	{
		if ('.' != buff.buf_addr[i] && !isdigit(buff.buf_addr[i]))
			return FALSE;
	}
	buff.buf_addr[buff.len_used] = '\0';
	endptr = &ptr;
	*value = strtod(buff.buf_addr, &endptr);
	return (0 == strcmp(endptr, "\0"));
}

inline boolean_t yed_is_false(ydb_buffer_t buff)
{
	if (buff.len_used != 6)
		return FALSE;
	if (0 != memcmp(buff.buf_addr, "\0false", buff.len_used))
		return FALSE;
	return TRUE;
}

inline boolean_t yed_is_true(ydb_buffer_t buff)
{
	if (buff.len_used != 5)
		return FALSE;
	if (0 != memcmp(buff.buf_addr, "\0true", buff.len_used))
		return FALSE;
	return TRUE;
}

inline boolean_t yed_is_null(ydb_buffer_t buff)
{
	if (buff.len_used != 5)
		return FALSE;
	if (0 != memcmp(buff.buf_addr, "\0null", buff.len_used))
		return FALSE;
	return TRUE;
}

inline boolean_t yed_is_direct_child_of(int subs_used, const ydb_buffer_t *subsarray,
		int next_subs_used, ydb_buffer_t *next_subsarray)
{
	if ((subs_used + 1) != next_subs_used)
		return FALSE;
	for (int i = 0; i < subs_used; i++)
	{
		if (subsarray[i].len_used != next_subsarray[i].len_used)
			return FALSE;
		if (strncmp(subsarray[i].buf_addr, next_subsarray[i].buf_addr, subsarray[i].len_used))
			return FALSE;
	}
	return TRUE;
}

inline boolean_t yed_is_descendant_of(int subs_used, const ydb_buffer_t *subsarray,
		int next_subs_used, ydb_buffer_t *next_subsarray)
{
	if (subs_used > next_subs_used)
		return FALSE;
	for (int i = 0; i < subs_used; i++)
	{
		if (subsarray[i].len_used != next_subsarray[i].len_used)
			return FALSE;
		if (strncmp(subsarray[i].buf_addr, next_subsarray[i].buf_addr, subsarray[i].len_used))
			return FALSE;
	}
	return TRUE;
}
