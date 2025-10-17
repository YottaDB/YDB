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
#include "op_zyencode_zydecode.h"		/* for zyencode_glvn_ptr */
#include "namelook.h"
#include "deferred_events_queue.h"

GBLREF	volatile int4		outofband;
GBLREF	int			zyencode_args;
GBLREF	zyencode_glvn_ptr	eglvnp;

STATICDEF	ydb_var_types	encode_type;
STATICDEF	char		*errmsg = NULL;

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
	unsigned int	data_value;
	int		encode_svn_index, status;
	json_t		*jansson_object = NULL;
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
		YED_OBJECT_DELETE(jansson_object);
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
	/* If ret_value is empty, that signals that Jansson should return a buffer, which we will later have to free */
	if ((NULL == ret_value->buf_addr) && ((0 < ret_value->len_alloc) || (0 < ret_value->len_used)))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PARAMINVALID, 4,
			LEN_AND_LIT("NULL ret_value->buf_addr"), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_ENCODE)));
	for (int i = 0; i < subs_used; i++)
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
	status = ydb_data_s(varname, subs_used, subsarray, &data_value);
	if (YDB_OK != status)
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
	jansson_object = yed_new_object();
	status = yed_encode_tree(varname, subs_used, subsarray, data_value, jansson_object, NULL, NULL);
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
				YED_OBJECT_DELETE(jansson_object);
				yed_lydb_rtn = FALSE;
				REVERT;
				break;
		}
		return status;
	}
	if ((0 == ret_value->len_alloc) && (0 == ret_value->len_used) && (NULL == ret_value->buf_addr))
	{	/* If ret_value is empty, that signals that Jansson should return a buffer, which we will later have to free */
		ret_value->buf_addr = yed_dump_json(jansson_object, 0);
		if (NULL == ret_value->buf_addr)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JANSSONINVALIDJSON, 4,
				LEN_AND_LIT("Empty JSON returned"), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_ENCODE)));
		size = strlen(ret_value->buf_addr);
		if (UINT_MAX < (size + 1))	/* add 1 for the NUL */
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_JANSSONINVSTRLEN, 2, size + 1, UINT_MAX);
		ret_value->len_alloc = ret_value->len_used = size;
		ret_value->len_alloc++;	/* add 1 for the NUL */
	}
	else
	{	/* Otherwise, caller has passed in their own buffer */
		size = yed_output_json(jansson_object, ret_value->buf_addr, ret_value->len_alloc, 0);
		if (0 == size)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JANSSONINVALIDJSON, 4,
				LEN_AND_LIT("Empty JSON returned"), LEN_AND_STR(LYDBRTNNAME(LYDB_RTN_ENCODE)));
		if (ret_value->len_alloc < size + 1)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_JANSSONINVSTRLEN, 2, size + 1, ret_value->len_alloc);
		ret_value->len_used = size;
		ret_value->buf_addr[ret_value->len_used] = '\0';
	}
	YED_OBJECT_DELETE(jansson_object);
	yed_lydb_rtn = FALSE;
	LIBYOTTADB_DONE;
	REVERT;
	return status;
}

int yed_encode_tree(const ydb_buffer_t *varname, int subs_used, const ydb_buffer_t *subsarray,
			unsigned int data_value, json_t *obj, int *ret_subs_used, ydb_buffer_t *ret_subsarray)
{
	ydb_buffer_t	cur_subsarray[YDB_MAX_SUBS] = {0}, next_subsarray[YDB_MAX_SUBS] = {0}, cur_value = {0};
	const char	*root;
	long long	value_ll;
	double		value_d;
	unsigned int	size;
	int		return_code, cur_subs_used = 0, next_subs_used = 0, i, string_size, status;
	json_t		*cur, *val;
	char		*curpool, *nextpool, *valuepool;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	cur = obj;
	root = "";	/* Used as a special JSON key to hold values at M array nodes that also have children at higher levels.
			 * An empty string is allowed as a key in JSON, but not as a subscript in M (by default).
			 */
	if (LYDB_VARREF_GLOBAL == encode_type)
		if (zyencode_args)	/* zyencode_args is > 0 if called by op_zyencode() */
			string_size = eglvnp->gblp[1]->s_gv_cur_region->max_key_size;
		else
			string_size = MAX_KEY_SZ;
	else
		string_size = YDB_MAX_STR;
	string_size++;	/* add 1 for the NUL needed later when passed to the Jansson library */
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
	if ((LYDB_VARREF_GLOBAL == encode_type) && zyencode_args)	/* zyencode_args is > 0 if called by op_zyencode() */
		cur_value.len_alloc = eglvnp->gblp[1]->s_gv_cur_region->max_rec_size;
	else
		cur_value.len_alloc = YDB_MAX_STR;
	cur_value.len_alloc++;	/* add 1 for the NUL needed later when passed to the Jansson library */
	valuepool = system_malloc(cur_value.len_alloc);
	cur_value.buf_addr = valuepool;
	if (data_value % 2)	/* handle root of tree's value if it has one */
	{
		status = ydb_get_s(varname, subs_used, subsarray, &cur_value);
		if (YDB_OK != status)
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
		return_code = yed_set_object(cur, root, val);
		if (-1 == return_code)
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
	if (YDB_OK != status)
	{
		system_free(curpool);
		system_free(nextpool);
		system_free(valuepool);
		return status;
	}
	while (0 != next_subs_used)
	{
		assert(YDB_OK == status);
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
			system_free(curpool);
			system_free(nextpool);
			system_free(valuepool);
			return YDB_OK;
		}
		assert(next_subs_used > subs_used);
		status = ydb_data_s(varname, next_subs_used, next_subsarray, &data_value);
		if (YDB_OK != status)
		{
			system_free(curpool);
			system_free(nextpool);
			system_free(valuepool);
			return status;
		}
		if (!yed_is_direct_child_of(subs_used, subsarray, next_subs_used, next_subsarray))
		{	/* The next node is not a direct child of this one. The nodes in between should be represented as objects */
			status = ydb_data_s(varname, subs_used + 1, next_subsarray, &data_value);
			if (YDB_OK != status)
			{
				system_free(curpool);
				system_free(nextpool);
				system_free(valuepool);
				return status;
			}
			val = yed_new_object();
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
				system_free(curpool);
				system_free(nextpool);
				system_free(valuepool);
				return ERR_JANSSONENCODEERROR;
			}
			status = yed_encode_tree(varname, subs_used + 1, next_subsarray,
					data_value, val, &cur_subs_used, cur_subsarray);
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
				system_free(curpool);
				system_free(nextpool);
				system_free(valuepool);
				return ERR_JANSSONENCODEERROR;
			}
			status = yed_encode_tree(varname, next_subs_used, next_subsarray,
					data_value, val, &cur_subs_used, cur_subsarray);
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
			status = ydb_get_s(varname, next_subs_used, next_subsarray, &cur_value);
			if (YDB_OK != status)
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
		if (YDB_OK != status)
		{
			system_free(curpool);
			system_free(nextpool);
			system_free(valuepool);
			return status;
		}
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

	if (YED_DEFAULT_PRECISION < buff.len_used)
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

	if (YED_DEFAULT_PRECISION < buff.len_used)
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
