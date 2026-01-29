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

#include "libyottadb_int.h"
#include "ydb_encode_decode.h"
#include "op_zyencode_zydecode.h"		/* for zyencode_glvn_ptr */
#include "namelook.h"
#include "deferred_events_queue.h"

#define YED_ARRAY	1

GBLREF	volatile int4		outofband;
GBLREF	int			zyencode_args;
GBLREF	zyencode_glvn_ptr	eglvnp;

STATICDEF	ydb_var_types	encode_type;
STATICDEF	char		*errmsg = NULL;
STATICDEF	int		key_size;

/* Routine to encode a local or global in to a formatted string
 *
 * Parameters:
 *   varname    - Gives name of local or global variable
 *   subs_used	- Count of subscripts already setup for source array (subtree root)
 *   subsarray  - an array of subscripts used to process source array, already containing "subs_used" subscripts
 *   format	- Format of string to be encoded (currently always "JSON" and ignored - for future use)
 *   ret_value	- Value fetched from local/global variable encoded in to a formatted string stored/returned here
 *
 *  NOTE: Caller of ydb_encode_s() must supply a pointer to an empty ydb_string_t struct for the ret_value argument
 *	  ret_value->address must be NULL and ret_value->length must be set to 0
 *	  When ydb_encode_s() returns, without an error, ret_value will contain a pointer to a callee-allocated
 *	  buffer containing the JSON-formatted string in ret_value->address, and ret_value->length will be
 *	  set to the length of the string (not including the trailing NUL byte)
 *	  Caller will be required to free the memory at ret_value->address, as it will be allocated on the heap
 */
int	ydb_encode_s(const ydb_buffer_t *varname, int subs_used, const ydb_buffer_t *subsarray,
			const char *format, ydb_string_t *ret_value)
{
	ydb_buffer_t	next_subsarray[YDB_MAX_SUBS] = {0};
	boolean_t	error_encountered;
	unsigned int	data_value;
	int		encode_svn_index, status = YDB_OK, next_subs_used, i;
	json_t		*jansson_object = NULL;
	char		*nextpool = NULL;
	size_t		size;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	VERIFY_NON_THREADED_API;	/* clears a global variable "caller_func_is_stapi" set by SimpleThreadAPI caller
					 * so needs to be first invocation after SETUP_THREADGBL_ACCESS to avoid any error
					 * scenarios from not resetting this global variable even though this function returns.
					 */
	assert(!yed_lydb_rtn);	/* ydb_encode_s() and ydb_decode_s() set to TRUE, and they should never be nested */
	if (zyencode_args)     /* zyencode_args is > 0 if called by op_zyencode() */
		TREF(libyottadb_active_rtn) = LYDB_RTN_ENCODE;	/* set active routine when called by op_zyencode() */
	else
		/* Verify entry conditions, make sure YDB CI environment is up etc. */
		LIBYOTTADB_INIT(LYDB_RTN_ENCODE, (int));        /* Note: macro could return from this function in case of errors */
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{
		system_free(errmsg);
		errmsg = NULL;
		system_free(nextpool);
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
	VALIDATE_VARNAME(varname, subs_used, FALSE, LYDB_RTN_ENCODE, -1, encode_type, encode_svn_index);
	if (NULL == ret_value)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
			LEN_AND_LIT("NULL ret_value"), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_ENCODE)));
	for (i = 0; i < subs_used; i++)
	{
		if (subsarray[i].len_alloc < subsarray[i].len_used)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
				LEN_AND_LIT("len_alloc < len_used for at least 1 subscript in subsarray"),
				LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_ENCODE)));
		if ((0 != subsarray[i].len_used) && (NULL == subsarray[i].buf_addr))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
				LEN_AND_LIT("len_used is non-zero and buf_addr is NULL for at least 1 subscript in subsarray"),
				LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_ENCODE)));
	}
	if (YDB_OK != (status = ydb_data_s(varname, subs_used, subsarray, &data_value)))	/* Note assignment */
	{
		yed_lydb_rtn = FALSE;
		REVERT;
		return status;
	}
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
	if (LYDB_VARREF_GLOBAL == encode_type)
		if (zyencode_args)	/* zyencode_args is > 0 if called by op_zyencode() */
			key_size = eglvnp->gblp[1]->s_gv_cur_region->max_key_size;
		else
			key_size = MAX_KEY_SZ;
	else
		key_size = YDB_MAX_STR;
	key_size++;	/* add 1 for the NUL needed later when passed to the Jansson library */
	nextpool = system_malloc(YDB_MAX_SUBS * key_size);
	for (i = 0; YDB_MAX_SUBS > i; i++)
	{
		next_subsarray[i].len_alloc = key_size;
		next_subsarray[i].buf_addr = &nextpool[key_size * i];
	}
	next_subs_used = YDB_MAX_SUBS;
	if ((YDB_DATA_NOVALUE_DESC == data_value) &&	/* Note both assignments */
		(YDB_OK == (status = ydb_node_next_s(varname, subs_used, subsarray, &next_subs_used, next_subsarray))) &&
		(YED_ARRAY == (status = yed_array_test(varname, subs_used + 1, next_subsarray))))
	{
		system_free(nextpool);
		nextpool = NULL;
		jansson_object = yed_new_array();
		status = yed_encode_array(varname, subs_used, subsarray, jansson_object, NULL, NULL);
	}
	else if (YDB_OK == status)
	{
		system_free(nextpool);
		nextpool = NULL;
		jansson_object = yed_new_object();
		status = yed_encode_object(varname, subs_used, subsarray, data_value, jansson_object, NULL, NULL);
	}
	if (YDB_ERR_NODEEND == status)	/* YDB_ERR_NODEEND is not a real error */
		status = YDB_OK;
	if (YDB_OK != status)
	{
		switch (status)
		{
			case ERR_MINNRSUBSCRIPTS:
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MINNRSUBSCRIPTS);
				break;
			case ERR_MAXNRSUBSCRIPTS:
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
				break;
			case ERR_JANSSONENCODEERROR:
				assert(NULL != errmsg);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JANSSONENCODEERROR, 4,
					LEN_AND_STR(errmsg), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_ENCODE)));
				break;
			default:
				yed_object_decref(jansson_object);
				yed_lydb_rtn = FALSE;
				REVERT;
				break;
		}
		return status;
	}
	/* Jansson will return a buffer, which we will later have to free */
	ret_value->address = yed_encode_json(jansson_object, 0);
	if (NULL == ret_value->address)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JANSSONINVALIDJSON, 4,
			LEN_AND_LIT("Empty JSON returned"), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_ENCODE)));
	ret_value->length = strlen(ret_value->address);
	yed_object_decref(jansson_object);
	yed_lydb_rtn = FALSE;
	LIBYOTTADB_DONE;
	REVERT;
	return status;
}

int yed_encode_object(const ydb_buffer_t *varname, int subs_used, const ydb_buffer_t *subsarray,
			unsigned int data_value, json_t *obj, int *ret_subs_used, ydb_buffer_t *ret_subsarray)
{
	ydb_buffer_t	cur_subsarray[YDB_MAX_SUBS] = {0}, next_subsarray[YDB_MAX_SUBS] = {0}, cur_value = {0};
	const char	*root;
	long long	value_ll;
	double		value_d;
	int		return_code, cur_subs_used, next_subs_used, i, status = YDB_OK;
	json_t		*cur, *val;
	char		*curpool = NULL, *nextpool = NULL, *valuepool = NULL, *buf_addr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	cur = obj;
	root = "";	/* Used as a special JSON key to hold values at M array nodes that also have children at higher levels.
			 * An empty string is allowed as a key in JSON, but not as a subscript in M (by default).
			 */
	curpool = system_malloc(YDB_MAX_SUBS * key_size);
	nextpool = system_malloc(YDB_MAX_SUBS * key_size);
	for (i = 0; YDB_MAX_SUBS > i; i++)
	{
		cur_subsarray[i].len_alloc = key_size;
		cur_subsarray[i].buf_addr = &curpool[key_size * i];
		if (i < subs_used)
		{
			cur_subsarray[i].len_used = subsarray[i].len_used;
			memcpy(cur_subsarray[i].buf_addr, subsarray[i].buf_addr, cur_subsarray[i].len_used);
		}
		next_subsarray[i].len_alloc = key_size;
		next_subsarray[i].buf_addr = &nextpool[key_size * i];
	}
	cur_subs_used = subs_used;
	next_subs_used = YDB_MAX_SUBS;
	if ((LYDB_VARREF_GLOBAL == encode_type) && zyencode_args)	/* zyencode_args is > 0 if called by op_zyencode() */
		cur_value.len_alloc = eglvnp->gblp[1]->s_gv_cur_region->max_rec_size;
	else
		cur_value.len_alloc = YDB_MAX_STR;
	cur_value.len_alloc++;	/* add 1 for the NUL needed later when passed to the Jansson library */
	valuepool = system_malloc(cur_value.len_alloc);
	cur_value.buf_addr = valuepool;
	if (data_value % 2)	/* handle root of tree's value if it has one */
	{
		if (YDB_OK != (status = ydb_get_s(varname, subs_used, subsarray, &cur_value)))	/* Note assignment */
		{
			system_free(curpool);
			system_free(nextpool);
			system_free(valuepool);
			return status;
		}
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
		if (-1 == (return_code = yed_set_object(cur, root, val)))	/* Note assignment */
		{
			char	*errsrc;
			int	len, errmsg_len;
			mval	src, dst;

			len = cur_value.len_used;
			errsrc = system_malloc(len);
			memcpy(errsrc, cur_value.buf_addr, len);
			src.mvtype = MV_STR;
			src.str.len = len;
			src.str.addr = errsrc;
			op_fnzwrite(FALSE, &src, &dst); /* convert to ZWRITE format to show control characters */
			errmsg_len = varname->len_used + dst.str.len + 22;	/* 22 for string literals & NUL */
			errmsg = system_malloc(errmsg_len);
			SNPRINTF(errmsg, errmsg_len, "variable = %.*s : data = %.*s",
				varname->len_used, varname->buf_addr, dst.str.len, dst.str.addr);
			system_free(errsrc);
			system_free(curpool);
			system_free(nextpool);
			system_free(valuepool);
			return ERR_JANSSONENCODEERROR;
		}
	}
	if (0 > cur_subs_used)
		status = ERR_MINNRSUBSCRIPTS;
	else if (YDB_MAX_SUBS < cur_subs_used)
		status = ERR_MAXNRSUBSCRIPTS;
	if (YDB_OK == status)
		status = ydb_node_next_s(varname, cur_subs_used, cur_subsarray, &next_subs_used, next_subsarray);
	while ((YDB_OK == status) && (0 != next_subs_used))
	{
		if (!yed_is_descendant_of(subs_used, subsarray, next_subs_used, next_subsarray) ||
			yed_same_node_next(cur_subs_used, cur_subsarray, next_subs_used, next_subsarray))
		{	/* The next node is not part of this subtree or it's a local variable with its
			 * last node ending in a null subscript, so return.
			 */
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
			break;
		}
		assert(next_subs_used > subs_used);
		/* Note assignment */
		if (YDB_OK != (status = ydb_data_s(varname, next_subs_used, next_subsarray, &data_value)))
			break;
		if (!yed_is_direct_child_of(subs_used, subsarray, next_subs_used, next_subsarray))
		{	/* The next node is not a direct child of this one, nodes in between should be objects or arrays */
			/* Note assignment */
			if (YDB_OK != (status = ydb_data_s(varname, subs_used + 1, next_subsarray, &data_value)))
				break;
			if ((YDB_DATA_NOVALUE_DESC == data_value) &&	/* Note assignment */
					(YED_ARRAY == (status = yed_array_test(varname, subs_used + 2, next_subsarray))))
				val = yed_new_array();
			else if (YDB_OK == status)
				val = yed_new_object();
			else
				break;
			next_subsarray[subs_used].buf_addr[next_subsarray[subs_used].len_used] = '\0';
			return_code = yed_set_object(cur, next_subsarray[subs_used].buf_addr, val);
			if (-1 == return_code)
			{
				char	*errsrc;
				int	len, errmsg_len;
				mval	src, dst;

				len = next_subsarray[subs_used].len_used;
				errsrc = system_malloc(len);
				memcpy(errsrc, next_subsarray[subs_used].buf_addr, len);
				src.mvtype = MV_STR;
				src.str.len = len;
				src.str.addr = errsrc;
				op_fnzwrite(FALSE, &src, &dst); /* convert to ZWRITE format to show control characters */
				errmsg_len = varname->len_used + dst.str.len + 27;	/* 27 for string literals & NUL */
				errmsg = system_malloc(errmsg_len);
				SNPRINTF(errmsg, errmsg_len, "variable = %.*s : subscript = %.*s",
					varname->len_used, varname->buf_addr, dst.str.len, dst.str.addr);
				system_free(errsrc);
				status = ERR_JANSSONENCODEERROR;
				break;
			}
			if (JSON_ARRAY == val->type)
			{
				if (YDB_OK != (status = yed_encode_array(varname, subs_used + 1, next_subsarray,
						val, &cur_subs_used, cur_subsarray)))	/* Note assignment */
					break;
			}
			else
			{
				if (YDB_OK != (status = yed_encode_object(varname, subs_used + 1, next_subsarray,
						data_value, val, &cur_subs_used, cur_subsarray)))	/* Note assignment */
					break;
			}
		}
		else if (9 < data_value)
		{	/* Has a subtree. Should be represented as an object or an array. */
			if ((YDB_DATA_NOVALUE_DESC == data_value) &&	/* Note assignment */
					(YED_ARRAY == (status = yed_array_test(varname, subs_used + 2, next_subsarray))))
				val = yed_new_array();
			else if (YDB_OK == status)
				val = yed_new_object();
			else
				break;
			next_subsarray[subs_used].buf_addr[next_subsarray[subs_used].len_used] = '\0';
			return_code = yed_set_object(cur, next_subsarray[subs_used].buf_addr, val);
			if (-1 == return_code)
			{
				char	*errsrc;
				int	len, errmsg_len;
				mval	src, dst;

				len = next_subsarray[subs_used].len_used;
				errsrc = system_malloc(len);
				memcpy(errsrc, next_subsarray[subs_used].buf_addr, len);
				src.mvtype = MV_STR;
				src.str.len = len;
				src.str.addr = errsrc;
				op_fnzwrite(FALSE, &src, &dst); /* convert to ZWRITE format to show control characters */
				errmsg_len = varname->len_used + dst.str.len + 27;	/* 27 for string literals & NUL */
				errmsg = system_malloc(errmsg_len);
				SNPRINTF(errmsg, errmsg_len, "variable = %.*s : subscript = %.*s",
					varname->len_used, varname->buf_addr, dst.str.len, dst.str.addr);
				system_free(errsrc);
				status = ERR_JANSSONENCODEERROR;
				break;
			}
			if (JSON_ARRAY == val->type)
			{
				if (YDB_OK != (status = yed_encode_array(varname, next_subs_used, next_subsarray,
						val, &cur_subs_used, cur_subsarray)))	/* Note assignment */
					break;
			}
			else
			{
				if (YDB_OK != (status = yed_encode_object(varname, next_subs_used, next_subsarray,
						data_value, val, &cur_subs_used, cur_subsarray)))	/* Note assignment */
					break;
			}
		}
		else
		{	/* No subtree. Represent as an int, real, string or boolean value. */
			/* Note assignment */
			if (YDB_OK != (status = ydb_get_s(varname, next_subs_used, next_subsarray, &cur_value)))
				break;
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
				int	len, tmplen, errmsg_len;
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
				errmsg_len = varname->len_used + tmplen + dst.str.len + 37;	/* 37 for string literals & NUL */
				errmsg = system_malloc(errmsg_len);
				SNPRINTF(errmsg, errmsg_len, "variable = %.*s : subscript = %.*s : data = %.*s",
					varname->len_used, varname->buf_addr, tmplen, errtmp, dst.str.len, dst.str.addr);
				system_free(errsrc);
				system_free(errtmp);
				status = ERR_JANSSONENCODEERROR;
				break;
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
			status = ERR_MINNRSUBSCRIPTS;
			break;
		}
		if (YDB_MAX_SUBS < cur_subs_used)
		{
			status = ERR_MAXNRSUBSCRIPTS;
			break;
		}
		next_subs_used = YDB_MAX_SUBS;
		/* Note assignment */
		if (YDB_OK != (status = ydb_node_next_s(varname, cur_subs_used, cur_subsarray, &next_subs_used, next_subsarray)))
			break;
	}
	if ((YDB_OK == status) && (NULL != ret_subs_used))
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
	return status;
}

int yed_encode_array(const ydb_buffer_t *varname, int subs_used, const ydb_buffer_t *subsarray,
			json_t *obj, int *ret_subs_used, ydb_buffer_t *ret_subsarray)
{
	ydb_buffer_t	cur_subsarray[YDB_MAX_SUBS] = {0}, next_subsarray[YDB_MAX_SUBS] = {0}, cur_value = {0};
	long long	value_ll;
	double		value_d;
	unsigned int	data_value;
	int		return_code, cur_subs_used, next_subs_used, i, status = YDB_OK;
	json_t		*cur, *val;
	char		*curpool = NULL, *nextpool = NULL, *valuepool = NULL, *buf_addr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	cur = obj;
	if (0 > subs_used)
		return ERR_MINNRSUBSCRIPTS;
	if (YDB_MAX_SUBS < subs_used)
		return ERR_MAXNRSUBSCRIPTS;
	curpool = system_malloc(YDB_MAX_SUBS * key_size);
	nextpool = system_malloc(YDB_MAX_SUBS * key_size);
	for (i = 0; YDB_MAX_SUBS > i; i++)
	{
		cur_subsarray[i].len_alloc = key_size;
		cur_subsarray[i].buf_addr = &curpool[key_size * i];
		if (i < subs_used)
		{
			cur_subsarray[i].len_used = subsarray[i].len_used;
			memcpy(cur_subsarray[i].buf_addr, subsarray[i].buf_addr, cur_subsarray[i].len_used);
		}
		next_subsarray[i].len_alloc = key_size;
		next_subsarray[i].buf_addr = &nextpool[key_size * i];
	}
	cur_subs_used = subs_used;
	next_subs_used = YDB_MAX_SUBS;
	if ((LYDB_VARREF_GLOBAL == encode_type) && zyencode_args)	/* zyencode_args is > 0 if called by op_zyencode() */
		cur_value.len_alloc = eglvnp->gblp[1]->s_gv_cur_region->max_rec_size;
	else
		cur_value.len_alloc = YDB_MAX_STR;
	cur_value.len_alloc++;	/* add 1 for the NUL needed later when passed to the Jansson library */
	valuepool = system_malloc(cur_value.len_alloc);
	cur_value.buf_addr = valuepool;
	if (0 > cur_subs_used)
		status = ERR_MINNRSUBSCRIPTS;
	else if (YDB_MAX_SUBS < cur_subs_used)
		status = ERR_MAXNRSUBSCRIPTS;
	if (YDB_OK == status)
		status = ydb_node_next_s(varname, cur_subs_used, cur_subsarray, &next_subs_used, next_subsarray);
	while ((YDB_OK == status) && (0 != next_subs_used))
	{
		if (!yed_is_descendant_of(subs_used, subsarray, next_subs_used, next_subsarray) ||
			yed_same_node_next(cur_subs_used, cur_subsarray, next_subs_used, next_subsarray))
		{	/* The next node is not part of this subtree or it's a local variable with its
			 * last node ending in a null subscript, so return.
			 */
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
			break;
		}
		assert(next_subs_used > subs_used);
		/* Note assignment */
		if (YDB_OK != (status = ydb_data_s(varname, next_subs_used, next_subsarray, &data_value)))
			break;
		if (!yed_is_direct_child_of(subs_used, subsarray, next_subs_used, next_subsarray))
		{	/* The next node is not a direct child of this one, nodes in between should be objects or arrays */
			/* Note assignment */
			if (YDB_OK != (status = ydb_data_s(varname, subs_used + 1, next_subsarray, &data_value)))
				break;
			if ((YDB_DATA_NOVALUE_DESC == data_value) &&	/* Note assignment */
					(YED_ARRAY == (status = yed_array_test(varname, subs_used + 2, next_subsarray))))
				val = yed_new_array();
			else if (YDB_OK == status)
				val = yed_new_object();
			else
				break;
			return_code = yed_set_array(cur, val);
			if (-1 == return_code)
			{
				char	*errsrc;
				int	len, errmsg_len;
				mval	src, dst;

				len = next_subsarray[subs_used].len_used;
				errsrc = system_malloc(len);
				memcpy(errsrc, next_subsarray[subs_used].buf_addr, len);
				src.mvtype = MV_STR;
				src.str.len = len;
				src.str.addr = errsrc;
				op_fnzwrite(FALSE, &src, &dst); /* convert to ZWRITE format to show control characters */
				errmsg_len = varname->len_used + dst.str.len + 27;	/* 27 for string literals & NUL */
				errmsg = system_malloc(errmsg_len);
				SNPRINTF(errmsg, errmsg_len, "variable = %.*s : subscript = %.*s",
					varname->len_used, varname->buf_addr, dst.str.len, dst.str.addr);
				system_free(errsrc);
				status = ERR_JANSSONENCODEERROR;
				break;
			}
			if (JSON_ARRAY == val->type)
			{
				if (YDB_OK != (status = yed_encode_array(varname, subs_used + 1, next_subsarray,
						val, &cur_subs_used, cur_subsarray)))	/* Note assignment */
					break;
			}
			else
			{
				if (YDB_OK != (status = yed_encode_object(varname, subs_used + 1, next_subsarray,
						data_value, val, &cur_subs_used, cur_subsarray)))	/* Note assignment */
					break;
			}
		}
		else if (9 < data_value)
		{	/* Has a subtree. Should be represented as an object or an array. */
			if ((YDB_DATA_NOVALUE_DESC == data_value) &&	/* Note assignment */
					(YED_ARRAY == (status = yed_array_test(varname, subs_used + 2, next_subsarray))))
				val = yed_new_array();
			else if (YDB_OK == status)
				val = yed_new_object();
			else
				break;
			return_code = yed_set_array(cur, val);
			if (-1 == return_code)
			{
				char	*errsrc;
				int	len, errmsg_len;
				mval	src, dst;

				len = next_subsarray[subs_used].len_used;
				errsrc = system_malloc(len);
				memcpy(errsrc, next_subsarray[subs_used].buf_addr, len);
				src.mvtype = MV_STR;
				src.str.len = len;
				src.str.addr = errsrc;
				op_fnzwrite(FALSE, &src, &dst); /* convert to ZWRITE format to show control characters */
				errmsg_len = varname->len_used + dst.str.len + 27;	/* 27 for string literals & NUL */
				errmsg = system_malloc(errmsg_len);
				SNPRINTF(errmsg, errmsg_len, "variable = %.*s : subscript = %.*s",
					varname->len_used, varname->buf_addr, dst.str.len, dst.str.addr);
				system_free(errsrc);
				status = ERR_JANSSONENCODEERROR;
				break;
			}
			if (JSON_ARRAY == val->type)
			{
				if (YDB_OK != (status = yed_encode_array(varname, next_subs_used, next_subsarray,
						val, &cur_subs_used, cur_subsarray)))	/* Note assignment */
					break;
			}
			else
			{
				if (YDB_OK != (status = yed_encode_object(varname, next_subs_used, next_subsarray,
						data_value, val, &cur_subs_used, cur_subsarray)))	/* Note assignment */
					break;
			}
		}
		else
		{	/* No subtree. Represent as an int, real, string or boolean value. */
			/* Note assignment */
			if (YDB_OK != (status = ydb_get_s(varname, next_subs_used, next_subsarray, &cur_value)))
				break;
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
			return_code = yed_set_array(cur, val);
			if (-1 == return_code)
			{
				char	*errsrc, *errtmp;
				int	len, tmplen, errmsg_len;
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
				errmsg_len = varname->len_used + tmplen + dst.str.len + 37;	/* 37 for string literals & NUL */
				errmsg = system_malloc(errmsg_len);
				SNPRINTF(errmsg, errmsg_len, "variable = %.*s : subscript = %.*s : data = %.*s",
					varname->len_used, varname->buf_addr, tmplen, errtmp, dst.str.len, dst.str.addr);
				system_free(errsrc);
				system_free(errtmp);
				status = ERR_JANSSONENCODEERROR;
				break;
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
			status = ERR_MINNRSUBSCRIPTS;
			break;
		}
		if (YDB_MAX_SUBS < cur_subs_used)
		{
			status = ERR_MAXNRSUBSCRIPTS;
			break;
		}
		next_subs_used = YDB_MAX_SUBS;
		/* Note assignment */
		if (YDB_OK != (status = ydb_node_next_s(varname, cur_subs_used, cur_subsarray, &next_subs_used, next_subsarray)))
			break;
	}
	if ((YDB_OK == status) && (NULL != ret_subs_used))
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
	return status;
}

/*  Tests if the variable subscript level fits the pattern of a JSON array;
 *	first subscript is 0
 *	next subscript is 1
 *	and so on, increasing by 1, no strings or real numbers, et al.
 *	until the subscript level ends
 *
 *  Return type is one of:
 *	1  YED_ARRAY  - Subscript level fits the pattern of a JSON array
 *	0  YDB_OK     - Subscript level does not fit the pattern of a JSON array (overloaded for simpler logic)
 *	Anything else - ydb_subscript_next_s() has returned something other than YDB_OK
 */
inline int yed_array_test(const ydb_buffer_t *varname, int order_subs_used, ydb_buffer_t *order_subsarray)
{
	int		status = YDB_OK, rstatus = YDB_OK;
	long long	prev_value_ll, value_ll;

	if (!yed_is_integer(order_subsarray[order_subs_used - 1], &value_ll) || (0 != value_ll))
		return YDB_OK;
	prev_value_ll = value_ll;
	while (YDB_OK ==	/* Note assignment */
		(status = ydb_subscript_next_s(varname, order_subs_used, order_subsarray, &order_subsarray[order_subs_used - 1])))
	{
		if (!yed_is_integer(order_subsarray[order_subs_used - 1], &value_ll) || ((prev_value_ll + 1) != value_ll))
			return YDB_OK;
		prev_value_ll = value_ll;
	}
	if ((YDB_ERR_NODEEND != status) && (YDB_OK != status))
		return status;
	/* Reset last subscript to empty string and then reset array to the first node at this level */
	order_subsarray[order_subs_used - 1].len_used = 0;
	if (YDB_OK !=
		(rstatus = ydb_subscript_next_s(varname, order_subs_used, order_subsarray, &order_subsarray[order_subs_used - 1])))
		return rstatus;
	if (YDB_ERR_NODEEND == status)	/* YDB_ERR_NODEEND is not a real error - it means we have a full array */
		return YED_ARRAY;
	return YDB_OK;
}

inline boolean_t yed_is_integer(ydb_buffer_t buff, long long *value)
{
	char	*str, *endptr, ptr;

	if (YED_DEFAULT_PRECISION < buff.len_used)
		return FALSE;
	if ((1 < buff.len_used) && ('0' == buff.buf_addr[0]))
		return FALSE;
	if ((1 == buff.len_used) && ('-' == buff.buf_addr[0]))
		return FALSE;
	if ((2 < buff.len_used) && ('-' == buff.buf_addr[0]) && ('0' == buff.buf_addr[1]))
		return FALSE;
	if (('-' != buff.buf_addr[0]) && !isdigit(buff.buf_addr[0]))
		return FALSE;
	for (int i = 1; i < buff.len_used; i++)
	{
		if (!isdigit(buff.buf_addr[i]))
			return FALSE;
	}
	buff.buf_addr[buff.len_used] = '\0';
	endptr = &ptr;
	*value = strtoll(buff.buf_addr, &endptr, 10);
	return (0 == strcmp(endptr, "\0"));
}

inline boolean_t yed_is_real(ydb_buffer_t buff, double *value)
{
	char		*str, *endptr, ptr;
	boolean_t	dpnt = FALSE;

	if (YED_DEFAULT_PRECISION < buff.len_used)
		return FALSE;
	if ((1 < buff.len_used) && ('0' == buff.buf_addr[0]) && ('.' != buff.buf_addr[1]))
		return FALSE;
	if ((1 == buff.len_used) && ('-' == buff.buf_addr[0]))
		return FALSE;
	if ((2 < buff.len_used) && ('-' == buff.buf_addr[0]) && ('0' == buff.buf_addr[1]))
		return FALSE;
	for (int i = 0; i < (buff.len_used - 1); i++)
	{
		if ((0 == i) && ('-' == buff.buf_addr[i]))	/* A - can only be the first character */
			continue;
		if ('.' == buff.buf_addr[i])
		{
			if (dpnt)
				return FALSE;
			dpnt = TRUE;
		}
		else if (!isdigit(buff.buf_addr[i]))
			return FALSE;
	}
	if (dpnt && ('0' == buff.buf_addr[buff.len_used - 1]))	/* Decimal point and a 0 cannot be the last character */
		return FALSE;
	if (!isdigit(buff.buf_addr[buff.len_used - 1]))	/* Decimal point cannot be last character */
		return FALSE;
	buff.buf_addr[buff.len_used] = '\0';
	endptr = &ptr;
	*value = strtod(buff.buf_addr, &endptr);
	return (0 == strcmp(endptr, "\0"));
}

/* The JSON false type is prefaced by a null byte to disambiguate it from the string "false"
 * when stored in M arrays. See note at yed_decode_bool() in ydb_decode_s.c.
 */
inline boolean_t yed_is_false(ydb_buffer_t buff)
{
	if (STR_LIT_LEN(YED_FALSE) != buff.len_used)
		return FALSE;
	if (0 != MEMCMP_LIT(buff.buf_addr, YED_FALSE))
		return FALSE;
	return TRUE;
}

/* The JSON true type is prefaced by a null byte to disambiguate it from the string "true"
 * when stored in M arrays. See note at yed_decode_bool() in ydb_decode_s.c.
 */
inline boolean_t yed_is_true(ydb_buffer_t buff)
{
	if (STR_LIT_LEN(YED_TRUE) != buff.len_used)
		return FALSE;
	if (0 != MEMCMP_LIT(buff.buf_addr, YED_TRUE))
		return FALSE;
	return TRUE;
}

/* The JSON null type is prefaced by a null byte to disambiguate it from the string "null"
 * when stored in M arrays. See note at yed_decode_bool() in ydb_decode_s.c.
 */
inline boolean_t yed_is_null(ydb_buffer_t buff)
{
	if (STR_LIT_LEN(YED_NULL) != buff.len_used)
		return FALSE;
	if (0 != MEMCMP_LIT(buff.buf_addr, YED_NULL))
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
	if (subs_used == next_subs_used)	/* null subscripts must be enabled, so not actually a descendant */
		return FALSE;
	return TRUE;
}

inline boolean_t yed_same_node_next(int cur_subs_used, const ydb_buffer_t *cur_subsarray,
		int next_subs_used, ydb_buffer_t *next_subsarray)
{
	if (cur_subs_used != next_subs_used)
		return FALSE;
	for (int i = 0; i < cur_subs_used; i++)
	{
		if (cur_subsarray[i].len_used != next_subsarray[i].len_used)
			return FALSE;
		if (strncmp(cur_subsarray[i].buf_addr, next_subsarray[i].buf_addr, cur_subsarray[i].len_used))
			return FALSE;
	}
	return TRUE;
}
