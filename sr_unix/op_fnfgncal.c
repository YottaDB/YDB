/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2023 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include <stdarg.h>
#include <errno.h>
#ifdef GTM_PTHREAD
#  include "gtm_pthread.h"
#endif
#include "gtm_stdlib.h"

#include "stringpool.h"
#include "copy.h"
#include "stack_frame.h"
#include "op.h"
#include "lv_val.h"		/* needed for "fgncal.h" */
#include "fgncal.h"
#include "gtmci.h"
#include "gtmxc_types.h"
#include "mvalconv.h"
#include "gt_timer.h"
#include "callg.h"
#include "callintogtmxfer.h"
#include "min_max.h"
#include "have_crit.h"
#include "gtmdbglvl.h"		/* for gtm_malloc.h */
#include "gtm_malloc.h"		/* for VERIFY_STORAGE_CHAINS */
#include "ydb_getenv.h"
#include "send_msg.h"
#include "io.h"
#include "iottdef.h"

/******************************************************************************
 *
 * This routine is called by generated code with the following arguments:
 *
 *	n	argument count (int4)
 *	dst	destination (mval *), that is, result for extrinsic function call
 *		null pointer means that this is a routine call
 *	package	package name (mval *)
 *	extref	external reference name (mval *)
 *	mask	call-by refrence mask (int4)
 *			If bit=1, then call by reference,
 *			otherwise, call by value.
 *		lsb is first argument, etc.
 *			Implies all call-by-reference arguments must be in the first 32 arguments.
 *	argcnt	argument count (int4).  Number of input parameters.
 *		always equal to argument count n - 5
 *	arg0	the first input paramter (mval *)
 *	arg1	the second input parameter (mval *)
 *	...	...
 *	argi	the last input parameter, i = argcnt - 1
 *
 *	example:
 *		d &mathpak.bessel(p1,p2,.p3)
 *
 *	would result in the following call:
 *
 *	op_fnfgncal(8, 0, mval*("mathpak"), mval*("bessel"), 4, 3, mval*(p1), mval*(p2), mval*(p3))
 *
 *	where, mval*(val) indicates the address of an mval that has the string value val.
 *
 *	If the extref has a routine name, than the extref looks like an M source
 *	reference.  E.g.:
 *		s x=&$foo^bar
 *		entryref-->"foo.bar"*
 *
 *
 *	This routine is platform independent (at least for UNIX).
 *	It requires two platform specific routines:
 *
 *		lookup package
 *		Return package_handle if success, NULL otherwise.
 *
 *		void * fgn_getpak(char *package_name, int error_severity)
 *		{
 *			lookup package
 *			If not found, return NULL;
 *			store any system specific information in package_handle
 *			return package_handle;
 *		}
 *
 *		lookup function
 *		return function address if success, NULL otherwise
 *
 *		typedef int (*fgnfnc)();
 *
 *		fgnfnc fgn_getrtn(void *package_handle, mstr *entry_name)
 *		{
 *			lookup entry_name
 *			If not found return NULL;
 *			return function address;
 *		}
 *
 ******************************************************************************/

GBLREF stack_frame      *frame_pointer;
GBLREF unsigned char    *msp;
GBLREF spdesc 		stringpool;
GBLREF int    		(*callintogtm_vectortable[])();
GBLREF int		mumps_status;
GBLREF volatile int4	gtmMallocDepth;
GBLREF io_pair		io_std_device;
#ifdef GTM_PTHREAD
GBLREF boolean_t	gtm_jvm_process;
GBLREF pthread_t	gtm_main_thread_id;
GBLREF boolean_t	gtm_main_thread_id_set;
#endif

LITREF	mval		literal_null;
LITREF mval		skiparg;

error_def(ERR_JNI);
error_def(ERR_MAXSTRLEN);
error_def(ERR_TEXT);
error_def(ERR_UNIMPLOP);
error_def(ERR_XCVOIDRET);
error_def(ERR_ZCARGMSMTCH);
error_def(ERR_ZCCONVERT);
error_def(ERR_ZCCTENV);
error_def(ERR_ZCMAXPARAM);
error_def(ERR_ZCNOPREALLOUTPAR);
error_def(ERR_ZCRTENOTF);
error_def(ERR_ZCSTATUSRET);
error_def(ERR_ZCUSRRTN);
error_def(ERR_ZCVECTORINDX);
error_def(ERR_XCRETNULLREF);
error_def(ERR_EXTCALLBOUNDS);
error_def(ERR_EXCEEDSPREALLOC);

/* Allocate a buffer to store list of external call parameters. Need to maintain one buffer for each call-in level possible
 * since it is possible for external calls to nest in which case the nested external call's buffer should not mess with
 * the parent external call's malloced buffer. Hence the CALLIN_MAX_LEVEL array definition below.
 */
STATICDEF	char		*op_fnfgncal_call_buff[CALLIN_MAX_LEVEL + 2];
STATICDEF	unsigned int	op_fnfgncal_call_buff_len[CALLIN_MAX_LEVEL + 2];

#define	OP_FNFGNCAL_ALLOCATE(PTR, SIZE)							\
{											\
	unsigned int	neededSize, buffLen, callinLevel;				\
											\
	callinLevel = TREF(gtmci_nested_level);						\
	assert(callinLevel < ARRAYSIZE(op_fnfgncal_call_buff_len));			\
	assert(callinLevel < ARRAYSIZE(op_fnfgncal_call_buff));				\
	neededSize = SIZE;								\
	buffLen = op_fnfgncal_call_buff_len[callinLevel];				\
	if (neededSize > buffLen)							\
	{										\
		if (buffLen)								\
			free(op_fnfgncal_call_buff[callinLevel]);			\
		/* The * 2 below is to avoid lots of incremental malloc/free calls */	\
		buffLen = neededSize * 2;						\
		op_fnfgncal_call_buff[callinLevel] = malloc(buffLen);			\
		op_fnfgncal_call_buff_len[callinLevel] = buffLen;			\
	}										\
	PTR = op_fnfgncal_call_buff[callinLevel];					\
}

STATICDEF int		call_table_initialized = 0;
/* The following are one-letter mnemonics for Java argument types (capital letters to denote output direction):
 * 						boolean	int	long	float	double	String	byte[] */
STATICDEF char		ydb_jtype_chars[] = {	'b',	'i',	'l',	'f',	'd',	'j',	'a',
						'B',	'I',	'L',	'F',	'D',	'J',	'A' };
STATICDEF int		ydb_jtype_start_idx = ydb_jboolean,	/* Value of first ydb_j... type for calculation of table indices. */
			ydb_jtype_count = ydb_jbyte_array - ydb_jboolean + 1;	/* Number of types supported with Java call-outs. */

STATICFNDCL void	extarg2mval(void *src, enum ydb_types typ, mval *dst, boolean_t java, boolean_t starred,
				int prealloc_size, char *m_label, gparam_list *ext_buff_start, int4 ext_buff_len);
STATICFNDCL int		extarg_getsize(void *src, enum ydb_types typ, mval *dst, struct extcall_entry_list *entry_ptr);
STATICFNDCL void	op_fgnjavacal(mval *dst, mval *package, mval *extref, uint4 mask, int4 argcnt, int4 entry_argcnt,
				struct extcall_package_list *package_ptr, struct extcall_entry_list *entry_ptr, va_list var);
STATICFNDCL gparam_list	*set_up_buffer(char *p_list, int len);
STATICFNDCL void	verify_buffer(char *p_list, int len, char *m_label);
STATICFNDCL void	free_return_type(INTPTR_T ret_val, enum ydb_types typ);

static const int buff_boarder_len = 8;	/* Needs to be 8-byte aligned or else "free_space_pointer" would not be 4-byte or
					 * 8-byte aligned for different input parameter types which could later show up
					 * as a SIGBUS error due to unaligned access on platforms that care about it (e.g. ARM).
					 */
static const char *buff_front_boarder = "STMARKER";
static const char *buff_end_boarder = "ENMARKER";

/* If the assigned pointer value is NULL, pass back a literal_null */
#define	VALIDATE_AND_CONVERT_PTR_TO_TYPE(LMVTYPE, TYPE, CONTAINER, DST, SRC)	\
MBSTART {									\
	if (NULL == SRC)							\
		*DST = literal_null;						\
	else 									\
	{									\
		CONTAINER = *((TYPE *)SRC);					\
		MV_FORCE_##LMVTYPE(DST, CONTAINER);				\
	}									\
} MBEND

/* if FLAG is false, set it to true and syslog it once */
#define ISSUE_ERR_ONCE(FLAG, ...)		\
{						\
	if (!FLAG)				\
	{					\
		FLAG = TRUE;			\
		send_msg_csa(__VA_ARGS__);	\
	}					\
}

/* Routine to set up boarders around the external call buffer */
STATICFNDCL gparam_list *set_up_buffer(char *p_list, int len)
{
	memcpy(p_list, buff_front_boarder, buff_boarder_len);
	memcpy((p_list + len + buff_boarder_len), buff_end_boarder, buff_boarder_len);
	return (gparam_list *)(p_list + buff_boarder_len);
}

STATICFNDCL void verify_buffer(char *p_list, int len, char *m_label)
{
	if ((0 != memcmp((p_list + len), buff_end_boarder, buff_boarder_len))
		|| (0 != memcmp((p_list - buff_boarder_len), buff_front_boarder, buff_boarder_len)))
	{
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_EXTCALLBOUNDS, 1, m_label);
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_EXTCALLBOUNDS, 1, m_label);
	}
	VERIFY_STORAGE_CHAINS;
}

STATICFNDCL void free_return_type(INTPTR_T ret_val, enum ydb_types typ)
{
	if (0 == ret_val)	/* Nothing to free regardless of type */
		return;
	switch (typ)
	{
		case ydb_jstring:
		case ydb_jbyte_array:
			free((struct extcall_string *)ret_val);
			break;
		case ydb_notfound:
		case ydb_void:
		case ydb_status:
		case ydb_int:
		case ydb_uint:
		case ydb_long:
		case ydb_ulong:
		case ydb_int64:
		case ydb_uint64:
		case ydb_float:
		case ydb_double:
		case ydb_pointertofunc:
		case ydb_pointertofunc_star:
		case ydb_jboolean:
		case ydb_jint:
		case ydb_jlong:
		case ydb_jfloat:
		case ydb_jdouble:
		case ydb_jbig_decimal:
			break;
		case ydb_string_star:
			if (NULL != (((ydb_string_t *)ret_val)->address))
				free((((ydb_string_t *)ret_val)->address));
			free(((ydb_string_t *)ret_val));
			break;
		case ydb_buffer_star:
			if (NULL != (((ydb_buffer_t *)ret_val)->buf_addr))
				free((((ydb_buffer_t *)ret_val)->buf_addr));
			free(((ydb_buffer_t *)ret_val));
			break;
		case ydb_float_star:
			free(((ydb_float_t *)ret_val));
			break;
		case ydb_double_star:
			free(((ydb_double_t *)ret_val));
			break;
		case ydb_int_star:
			free(((ydb_int_t *)ret_val));
			break;
		case ydb_uint_star:
			free(((ydb_uint_t *)ret_val));
			break;
		case ydb_long_star:
			free(((ydb_long_t *)ret_val));
			break;
		case ydb_ulong_star:
			free(((ydb_ulong_t *)ret_val));
			break;
		case ydb_int64_star:
			free(((ydb_int64_t *)ret_val));
			break;
		case ydb_uint64_star:
			free(((ydb_uint64_t *)ret_val));
			break;
		case ydb_char_star:
			free(((ydb_char_t *)ret_val));
			break;
		case ydb_char_starstar:
			if (NULL != *((ydb_char_t **)ret_val))
				free(*((ydb_char_t **)ret_val));
			free(((ydb_char_t **)ret_val));
			break;
	}
}
/* Routine to convert external return values to mval's */
STATICFNDEF void extarg2mval(void *src, enum ydb_types typ, mval *dst, boolean_t java, boolean_t starred,
	int prealloc_size, char *m_label, gparam_list *ext_buff_start, int4 ext_buff_len)
{
	ydb_int_t		s_int_num;
	ydb_long_t		str_len, s_long_num;
	ydb_uint_t		uns_int_num;
	ydb_ulong_t		uns_long_num;
	ydb_int64_t		s_int64_num;
	ydb_uint64_t		uns_int64_num;
	char			*cp;
	struct extcall_string	*sp;

	if (java)
	{
		switch (typ)
		{
			case ydb_notfound:
				break;
			case ydb_void:
				break;
			case ydb_status:
				/* Note: reason for double cast is to first turn ptr to same sized int, then big int to little int
				 * (on 64 bit platforms). This avoids a warning msg with newer 64 bit gcc compilers.
				 */
				s_int_num = (ydb_int_t)(intszofptr_t)src;
				if (0 != s_int_num)
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_ZCSTATUSRET);
				MV_FORCE_MVAL(dst, s_int_num);
				break;
			case ydb_jboolean:
			case ydb_jint:
				if (starred)
					s_int_num = *((ydb_int_t *)src);
				else
					s_int_num = (ydb_int_t)(intszofptr_t)src;
				MV_FORCE_MVAL(dst, s_int_num);
				break;
			case ydb_jlong:
#				ifdef GTM64
				if (starred)
					s_long_num = *((ydb_long_t *)src);
				else
					s_long_num = (ydb_long_t)src;
				MV_FORCE_LMVAL(dst, s_long_num);
#				else
				i82mval(dst, *(gtm_int64_t *)src);
#				endif
				break;
			case ydb_jfloat:
				float2mval(dst, *((float *)src));
				break;
			case ydb_jdouble:
				double2mval(dst, *((double *)src));
				break;
			case ydb_jstring:
			case ydb_jbyte_array:
				sp = (struct extcall_string *)src;
				dst->mvtype = MV_STR;
				if (sp->len > MAX_STRLEN)
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_MAXSTRLEN);
				if ((0 < sp->len) && (NULL != sp->addr))
				{
					dst->str.len = (mstr_len_t)sp->len;
					dst->str.addr = sp->addr;
					s2pool(&dst->str);
					/* In case of GTMByteArray or GTMString, the buffer is allocated in xc_gateway.c (since the
					 * user might need to return a bigger buffer than the original array size will allow (in
					 * case of a string the content is immutable). So, if we have determined that the provided
					 * value buffer is legitimate (non-null and non-empty), we free it on the GT.M side. */
					free(sp->addr);
				} else
					*dst = literal_null;
				break;
			default:
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_UNIMPLOP);
				break;
		}
		return;
	}
	/* The following switch is for non-Java call-outs. */
	switch (typ)
	{
		case ydb_notfound:
			break;
		case ydb_void:
			break;
		case ydb_status:
			/* Note: reason for double cast is to first turn ptr to same sized int, then big int to little int
			 * (on 64 bit platforms). This avoids a warning msg with newer 64 bit gcc compilers.
			 */
			s_int_num = (ydb_int_t)(intszofptr_t)src;
			if (0 != s_int_num)
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_ZCSTATUSRET);
			MV_FORCE_MVAL(dst, s_int_num);
			break;
		case ydb_int:
			s_int_num = (ydb_int_t)(intszofptr_t)src;
			MV_FORCE_MVAL(dst, s_int_num);
			break;
		case ydb_uint:
			uns_int_num = (ydb_uint_t)(intszofptr_t)src;
			MV_FORCE_UMVAL(dst, uns_int_num);
			break;
		case ydb_int_star:
			VALIDATE_AND_CONVERT_PTR_TO_TYPE(MVAL, ydb_int_t, s_int_num, dst, src);
			break;
		case ydb_uint_star:
			VALIDATE_AND_CONVERT_PTR_TO_TYPE(UMVAL, ydb_uint_t, uns_int_num, dst, src);
			break;
		case ydb_long:
			s_long_num = (ydb_long_t)src;
			MV_FORCE_LMVAL(dst, s_long_num);
			break;
		case ydb_ulong:
			uns_long_num = (ydb_ulong_t)src;
			MV_FORCE_ULMVAL(dst, uns_long_num);
			break;
		case ydb_long_star:
			VALIDATE_AND_CONVERT_PTR_TO_TYPE(LMVAL, ydb_long_t, s_long_num, dst, src);
			break;
		case ydb_ulong_star:
			VALIDATE_AND_CONVERT_PTR_TO_TYPE(ULMVAL, ydb_ulong_t, uns_long_num, dst, src);
			break;
#		ifdef GTM64
		case ydb_int64:
			s_int64_num = (ydb_int64_t)src;
			MV_FORCE_LMVAL(dst, s_int64_num);
			break;
		case ydb_uint64:
			uns_int64_num = (ydb_uint64_t)src;
			MV_FORCE_ULMVAL(dst, uns_int64_num);
			break;
#		endif
		case ydb_int64_star:
			VALIDATE_AND_CONVERT_PTR_TO_TYPE(MVAL, ydb_int64_t, s_int64_num, dst, src);
			break;
		case ydb_uint64_star:
			VALIDATE_AND_CONVERT_PTR_TO_TYPE(UMVAL, ydb_uint64_t, uns_int64_num, dst, src);
			break;
		case ydb_string_star:
			sp = (struct extcall_string *)src;
			if (NULL == sp) /* If the assigned pointer value is NULL, pass back a literal_null */
				*dst = literal_null;
			else
			{
				dst->mvtype = MV_STR;
				if (sp->len > MAX_STRLEN)
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_MAXSTRLEN);
				if ((0 <= prealloc_size) && (sp->len > prealloc_size)
						&& (sp->addr >= (char *)ext_buff_start)
						&& (sp->addr < ((char *)ext_buff_start + ext_buff_len)))
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(5) ERR_EXCEEDSPREALLOC, 3,
						prealloc_size, m_label, sp->len);
				if ((0 < sp->len) && (NULL != sp->addr))
				{
					dst->str.len = (mstr_len_t)sp->len;
					dst->str.addr = sp->addr;
					s2pool(&dst->str);
				} else
					*dst = literal_null;
			}
			break;
		case ydb_buffer_star:;
			ydb_buffer_t	*buff_ptr;

			buff_ptr = (ydb_buffer_t *)src;
			if (NULL == buff_ptr) /* If the assigned pointer value is NULL, pass back a literal_null */
				*dst = literal_null;
			else
			{
				dst->mvtype = MV_STR;
				if (buff_ptr->len_used > MAX_STRLEN)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXSTRLEN);
				assert((-1 == prealloc_size) || (prealloc_size == buff_ptr->len_alloc));
				if ((0 <= prealloc_size) && (buff_ptr->len_used > prealloc_size)
						&& (buff_ptr->buf_addr >= (char *)ext_buff_start)
						&& (buff_ptr->buf_addr < ((char *)ext_buff_start + ext_buff_len)))
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_EXCEEDSPREALLOC, 3,
							prealloc_size, m_label, buff_ptr->len_used);
				if ((0 < buff_ptr->len_used) && (NULL != buff_ptr->buf_addr))
				{
					dst->str.len = (mstr_len_t)buff_ptr->len_used;
					dst->str.addr = buff_ptr->buf_addr;
					s2pool(&dst->str);
				} else
					*dst = literal_null;
			}
			break;
		case ydb_float_star:
			if (NULL == src) /* If the assigned pointer value is NULL, pass back a literal_null */
				*dst = literal_null;
			else
				float2mval(dst, *((float *)src));
			break;
		case ydb_char_star:
			cp = (char *)src;
			if (NULL == cp) /* If the assigned pointer value is NULL, pass back a literal_null */
				*dst = literal_null;
			else
			{
				assert(((INTPTR_T)cp < (INTPTR_T)stringpool.base) || ((INTPTR_T)cp > (INTPTR_T)stringpool.top));
				dst->mvtype = MV_STR;
				str_len = STRLEN(cp);
				if (str_len > MAX_STRLEN)
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_MAXSTRLEN);
				if ((0 <= prealloc_size) && (str_len > prealloc_size))
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(5) ERR_EXCEEDSPREALLOC,
						3, prealloc_size, m_label, str_len);
				dst->str.len = (mstr_len_t)str_len;
				dst->str.addr = cp;
				s2pool(&dst->str);
			}
			break;
		case ydb_char_starstar:
			if (NULL == src) /* If the assigned pointer value is NULL, pass back a literal_null */
				*dst = literal_null;
			else
				extarg2mval(*((char **)src), ydb_char_star, dst, java, starred,
						prealloc_size, m_label, ext_buff_start, ext_buff_len);
			break;
		case ydb_double_star:
			if (NULL == src) /* If the assigned pointer value is NULL, pass back a literal_null */
				*dst = literal_null;
			else
			double2mval(dst, *((double *)src));
			break;
		default:
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_UNIMPLOP);
			break;
	}
	return;
}

/* Subroutine to calculate stringpool requirements for an external argument */
STATICFNDEF int extarg_getsize(void *src, enum ydb_types typ, mval *dst, struct extcall_entry_list *entry_ptr)
{
	char			*cp, **cpp;
	struct extcall_string	*sp;
	static bool		issued_ERR_XCRETNULLREF = FALSE;
	static bool		issued_ERR_ZCCONVERT = FALSE;

	if (!src)
		switch (typ)
		{
			case ydb_notfound:
			case ydb_void:
			case ydb_status:
			case ydb_int:
			case ydb_uint:
			case ydb_long:
			case ydb_ulong:
#		ifdef GTM64
			case ydb_int64:
			case ydb_uint64:
#		endif
			case ydb_jboolean:
			case ydb_jint:
			case ydb_jlong:
			case ydb_jfloat:
			case ydb_jdouble:
				return 0;
			default:
				/* Handle null pointer return types */
				ISSUE_ERR_ONCE(issued_ERR_XCRETNULLREF, CSA_ARG(NULL) VARLSTCNT(4)
						ERR_XCRETNULLREF, 2, LEN_AND_STR(entry_ptr->call_name.addr))
				return 0;
				break;
		}
	switch (typ)
	{	/* The following group of cases either return nothing or use the numeric part of the mval */
		case ydb_notfound:
		case ydb_void:
		case ydb_double_star:
		case ydb_status:
		case ydb_int:
		case ydb_uint:
		case ydb_long:
		case ydb_ulong:
#		ifdef GTM64
		case ydb_int64:
		case ydb_uint64:
#		endif
		case ydb_float_star:
		case ydb_int_star:
		case ydb_uint_star:
		case ydb_long_star:
		case ydb_ulong_star:
		case ydb_int64_star:
		case ydb_uint64_star:
		case ydb_jboolean:
		case ydb_jint:
		case ydb_jlong:
		case ydb_jfloat:
		case ydb_jdouble:
			return 0;
		case ydb_char_starstar:
			cpp = (char **)src;
			if (NULL == (*cpp))
			{
				ISSUE_ERR_ONCE(issued_ERR_XCRETNULLREF, CSA_ARG(NULL) VARLSTCNT(4)
						ERR_XCRETNULLREF, 2, LEN_AND_STR(entry_ptr->call_name.addr))
				return 0;
			}
			return STRLEN(*cpp);
		case ydb_char_star:
			cp = (char *)src;
			return STRLEN(cp);
		case ydb_jstring:
		case ydb_jbyte_array:
		case ydb_string_star:
			sp = (struct extcall_string *)src;
			if (NULL == sp->addr)
			{
				sp->len = 0;
				ISSUE_ERR_ONCE(issued_ERR_XCRETNULLREF, CSA_ARG(NULL) VARLSTCNT(4)
						ERR_XCRETNULLREF, 2, LEN_AND_STR(entry_ptr->call_name.addr))
			}
			if (0 > sp->len)
			{	/* Negative string length. syslog and reset to zero */
				sp->len = 0;
				ISSUE_ERR_ONCE(issued_ERR_ZCCONVERT, CSA_ARG(NULL) VARLSTCNT(4)
						ERR_ZCCONVERT, 2, LEN_AND_STR(entry_ptr->call_name.addr))
			} else
				assert(!(((INTPTR_T)sp->addr < (INTPTR_T)stringpool.free)
					&& ((INTPTR_T)sp->addr >= (INTPTR_T)stringpool.base)));
			return (int)(sp->len);
			break;
		case ydb_buffer_star:;
			ydb_buffer_t	*buff_ptr;

			buff_ptr = (ydb_buffer_t *)src;
			if (NULL == buff_ptr->buf_addr)
			{
				buff_ptr->len_used = 0;
				ISSUE_ERR_ONCE(issued_ERR_XCRETNULLREF, CSA_ARG(NULL) VARLSTCNT(4)
						ERR_XCRETNULLREF, 2, LEN_AND_STR(entry_ptr->call_name.addr))
			}
			if (0 > buff_ptr->len_used)
			{	/* Negative string length. syslog and reset to zero */
				buff_ptr->len_used = 0;
				ISSUE_ERR_ONCE(issued_ERR_ZCCONVERT, CSA_ARG(NULL) VARLSTCNT(4)
						ERR_ZCCONVERT, 2, LEN_AND_STR(entry_ptr->call_name.addr))
			} else
				assert(!(((INTPTR_T)buff_ptr->buf_addr < (INTPTR_T)stringpool.free)
					&& ((INTPTR_T)buff_ptr->buf_addr >= (INTPTR_T)stringpool.base)));
			return (int)(buff_ptr->len_used);
			break;
		default:
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_UNIMPLOP);
			break;
	}

	return 0; /* This should never get executed, added to make compiler happy */
}

STATICFNDEF void op_fgnjavacal(mval *dst, mval *package, mval *extref, uint4 mask, int4 argcnt, int4 entry_argcnt,
    struct extcall_package_list *package_ptr, struct extcall_entry_list *entry_ptr, va_list var)
{
	boolean_t	error_in_xc = FALSE, save_in_ext_call;
	char		*free_string_pointer, *free_string_pointer_start, jtype_char, *param_list_buff;
	char		*jni_err_buf;
	char		*types_descr_ptr, *types_descr_dptr, *xtrnl_table_name;
	gparam_list	*param_list;
	gtm_long_t	*free_space_pointer;
	int		i, j, save_mumps_status;
	int4 		m1, m2, n, space_n, call_buff_size;
	INTPTR_T	status;
	mval		*v;
	va_list		var_copy;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	ifdef GTM_PTHREAD
	gtm_jvm_process = TRUE;
	if (!gtm_main_thread_id_set)
	{
		gtm_main_thread_id = pthread_self();
		gtm_main_thread_id_set = TRUE;
	}
#	endif
	/* This is how many parameters we will use for callg, including the implicit ones described below. So, better make
	 * sure we are not trying to pass more than callg can handle.
	 */
	if (MAX_ACTUALS < argcnt + 3)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_ZCMAXPARAM);
	VAR_COPY(var_copy, var);
	/* Compute size of parameter block */
	n = entry_ptr->parmblk_size + (3 * SIZEOF(void *));	/* This is enough for the parameters and the fixed length entries */
	mask >>= 2;						/* Bypass the class and method arguments. */
	/* The first byte of the first argument (types_descr_ptr) stores the number of expected arguments, according to the external
	 * calls table; the second byte describes the expected return type, and subsequent bytes---the expected argument types using
	 * a one-character mnemonic which gets deciphered in the JNI layer. Note that in case of an error the xc_gateway.c module
	 * would set the first byte to 0xFF, followed by an address at which the error message is stored. For that reason, we
	 * allocate more space than we might strictly need for the arguments, return type, and type descriptor.
	 */
	types_descr_ptr = (char *)malloc(MAX(SIZEOF(char) * (argcnt + 2), SIZEOF(char *) * 2));
	/* zero the contents for unmodified output values */
	memset(types_descr_ptr, 0, MAX(SIZEOF(char) * (argcnt + 2), SIZEOF(char *) * 2));
	types_descr_dptr = types_descr_ptr;
	*types_descr_dptr = (char)entry_argcnt;
	types_descr_dptr++;
	if (dst)
	{	/* Record the expected return type. */
		switch (entry_ptr->return_type)
		{
			case ydb_status:
				*types_descr_dptr = 'i';
				break;
			case ydb_jlong:
				*types_descr_dptr = 'l';
				break;
			case ydb_jfloat:
				*types_descr_dptr = 'b';
				break;
			case ydb_jdouble:
				*types_descr_dptr = 'd';
				break;
			case ydb_jstring:
				*types_descr_dptr = 'j';
				break;
			case ydb_jbyte_array:
				*types_descr_dptr = 'a';
				break;
			default:
				*types_descr_dptr = 'v';
		}
	} else
		*types_descr_dptr = 'v';
	types_descr_dptr++;
	assert(2 * ydb_jtype_count == SIZEOF(ydb_jtype_chars));
	for (i = argcnt + 2, j = -2, m1 = entry_ptr->input_mask, m2 = entry_ptr->output_mask, space_n = 0; 0 < i; i--, j++)
	{	/* Enforce mval values and record expected argument types. */
		v = va_arg(var, mval *);
		if (0 > j)
		{
			MV_FORCE_STR(v);
			n += SIZEOF(void *) + v->str.len + 1;
			continue;
		}
		if (MASK_BIT_ON(m1))
		{
			MV_FORCE_DEFINED_UNLESS_SKIPARG(v);
		}
		/* Estimate how much allocation we are going to need for arguments which require more than pointer-size space. */
		if (MASK_BIT_ON(m1) && ((ydb_jstring == entry_ptr->parms[j]) || (ydb_jbyte_array == entry_ptr->parms[j])))
		{
			if (MV_DEFINED(v))
			{
				MV_FORCE_STR(v);
				n += SIZEOF(ydb_long_t) + v->str.len + 1;  /* length + string + '\0' */
			} else
				n += SIZEOF(ydb_long_t) + 1;		    /* length + '\0' */
		}
#		ifndef GTM64
		else if ((ydb_jdouble == entry_ptr->parms[j]) || (ydb_jlong == entry_ptr->parms[j]))
		{	/* Account for potential 8-byte alignment on 32-bit boxes */
			n += SIZEOF(gtm_int64_t);
			space_n += SIZEOF(gtm_int64_t);
		}
#		endif
		jtype_char = entry_ptr->parms[j] - ydb_jtype_start_idx;
		if ((0 > jtype_char) || (ydb_jtype_count <= jtype_char))
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_UNIMPLOP);
		else
			*types_descr_dptr = ydb_jtype_chars[MASK_BIT_ON(m2) ? (ydb_jtype_count + jtype_char) : jtype_char];
		types_descr_dptr++;
		m1 >>= 1;
		m2 >>= 1;
	}
	va_end(var);
        /* Allocate space for argument handling. Overall, the allocated space has the following structure:
	 *   ___________________________________________
	 *  |            |              |               |
	 *  | param_list | space buffer | string buffer |
	 *  |____________|______________|_______________|
	 *
	 * All input-output and output-only parameters have to be passed by reference, which means that param_list contains a
	 * pointer to the space buffer where the actual value is stored. Furthermore, in case of ydb_jstring_t and ydb_jbyte_array_t
	 * another pointer from within the space buffer is referencing an area inside the string buffer. Note, however, that certain
	 * arguments, such as ydb_jfloat_t and ydb_jdouble_t, and ydb_jlong_t on 32-bit boxes, are always passed by reference.
	 */
	/* Note that this is the nominal buffer size; or, explicitly, the size of buffer without the protection tags */
	call_buff_size = n;
	OP_FNFGNCAL_ALLOCATE(param_list_buff, call_buff_size + 2 * buff_boarder_len);
	param_list = set_up_buffer(param_list_buff, n);
	param_list->arg[0] = (void *)types_descr_ptr;
	/* Adding 3 to account for type descriptions, class name, and method name arguments. */
	free_space_pointer = (ydb_long_t *)((char *)param_list + SIZEOF(intszofptr_t) + (SIZEOF(void *) * (argcnt + 3)));
	/* Adding 3 for the same reason as above and another 3 to account for the fact that each of type description, class name,
	 * and method name arguments require room in the free_space buffer, which comes ahead of free_string buffer in memory.
	 */
	free_string_pointer_start = free_string_pointer
		= (char *)param_list + entry_ptr->parmblk_size + (SIZEOF(void *) * 3) + space_n;
	/* Load-up the parameter list */
	VAR_COPY(var, var_copy);
	/* We need to enter this loop even if argcnt == 0, so that the class and method arguments get set. */
	for (i = (0 == argcnt ? -1 : 0), j = 1, m1 = entry_ptr->input_mask, m2 = entry_ptr->output_mask; i < argcnt; j++)
	{
		v = va_arg(var_copy, mval *);
		if (j < 3)
		{
			param_list->arg[j] = free_string_pointer;
			if (v->str.len)
			{
				memcpy(free_string_pointer, v->str.addr, v->str.len);
				free_string_pointer += v->str.len;
			}
			*free_string_pointer++ = '\0';
			/* In case there are 0 arguments. */
			if ((2 == j) && (0 > i))
				i = 0;
			continue;
		}
		/* Verify that all input values are defined. */
		switch (entry_ptr->parms[i])
		{
			case ydb_jboolean:
				if (MASK_BIT_ON(m2))
				{	/* Output expected. */
					param_list->arg[j] = free_space_pointer;
					*((ydb_int_t *)free_space_pointer) = (ydb_int_t)(MV_ON(m1, v) ? (mval2i(v) ? 1 : 0) : 0);
					free_space_pointer++;
				} else	/* Input expected. */
					param_list->arg[j] = (void *)(ydb_long_t)(MV_ON(m1, v) ? (mval2i(v) ? 1 : 0) : 0);
				break;
			case ydb_jint:
				if (MASK_BIT_ON(m2))
				{	/* Output expected. */
					param_list->arg[j] = free_space_pointer;
					*((ydb_int_t *)free_space_pointer) = (ydb_int_t)(MV_ON(m1, v) ? mval2i(v) : 0);
					free_space_pointer++;
				} else	/* Input expected. */
					param_list->arg[j] = (void *)(ydb_long_t)(MV_ON(m1, v) ? mval2i(v) : 0);
				break;
			case ydb_jlong:
#				ifndef GTM64
				/* Only need to do this rounding on non-64 it platforms because this one type has a 64-bit
				 * alignment requirement on those platforms.
				 */
				free_space_pointer = (ydb_long_t *)(ROUND_UP2(((INTPTR_T)free_space_pointer), SIZEOF(gtm_int64_t)));
#				endif
#				ifdef GTM64
				if (MASK_BIT_ON(m2))
				{	/* Output expected. */
#				endif
					param_list->arg[j] = free_space_pointer;
					*((gtm_int64_t *)free_space_pointer) = (gtm_int64_t)(MV_ON(m1, v) ? mval2i8(v) : 0);
					free_space_pointer = (ydb_long_t *)((char *)free_space_pointer + SIZEOF(gtm_int64_t));
#				ifdef GTM64
				} else	/* Input expected. */
					param_list->arg[j] = (void *)(gtm_int64_t)(MV_ON(m1, v) ? mval2i8(v) : 0);
#				endif
				break;
			case ydb_jfloat:
				/* Have to go with additional storage either way due to the limitations of callg. */
				param_list->arg[j] = free_space_pointer;
				*((float *)free_space_pointer) = (float)(MV_ON(m1, v) ? mval2double(v) : 0.0);
				free_space_pointer++;
				break;
			case ydb_jdouble:
#				ifndef GTM64
				/* Only need to do this rounding on non-64 it platforms because this one type has a 64 bit
				 * alignment requirement on those platforms.
				 */
				free_space_pointer = (ydb_long_t *)(ROUND_UP2(((INTPTR_T)free_space_pointer), SIZEOF(double)));
#				endif
				/* Have to go with additional storage either way due to the limitations of callg. */
				param_list->arg[j] = free_space_pointer;
				*((double *)free_space_pointer) = (double)(MV_ON(m1, v) ? mval2double(v) : 0.0);
				free_space_pointer = (ydb_long_t *)((char *)free_space_pointer + SIZEOF(double));
				break;
			case ydb_jstring:
			case ydb_jbyte_array:
				param_list->arg[j] = free_space_pointer;
				/* If this is input-enabled and defined, it should have been forced to string in an earlier loop. */
				assert(!MV_ON(m1, v) || MV_IS_STRING(v));
				if (MV_ON(m1, v) && v->str.len)
				{
					*free_space_pointer++ = (ydb_long_t)v->str.len;
					memcpy(free_string_pointer, v->str.addr, v->str.len);
					*(char **)free_space_pointer = (char *)free_string_pointer;
					free_string_pointer += v->str.len;
				} else
				{	/* If an argument is for output only, an empty string, or skipped altogether, we still want
					 * a valid length and a pointer to an empty null-terminated character array.
					 */
					*free_space_pointer++ = 0;
					*(char **)free_space_pointer = (char *)free_string_pointer;
				}
				free_space_pointer++;
				*free_string_pointer++ = '\0';
				break;
			default:
				va_end(var_copy);
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_UNIMPLOP);
				break;
		}
		assert(((char *)free_string_pointer <= ((char *)param_list + n * 2))
			&& ((char *)free_space_pointer <= ((char *)param_list + n * 2)));
		i++;
		m1 = m1 >> 1;
		m2 = m2 >> 1;
	}
	assert((char *)free_space_pointer <= free_string_pointer_start);
	va_end(var_copy);
	param_list->n = argcnt + 3;		/* Take care of the three implicit parameters. */
	VERIFY_STORAGE_CHAINS;
	save_mumps_status = mumps_status; 	/* Save mumps_status as a callin from external call may change it. */
	save_in_ext_call = TREF(in_ext_call);
	assert(INTRPT_OK_TO_INTERRUPT == intrpt_ok_state);	/* Expected for DEFERRED_SIGNAL_HANDLING_CHECK below */
	TREF(in_ext_call) = TRUE;
	status = callg((callgfnptr)entry_ptr->fcn, param_list);
	TREF(in_ext_call) = save_in_ext_call;
	verify_buffer((char *)param_list, n, entry_ptr->entry_name.addr);
	if (!save_in_ext_call)
		check_for_timer_pops(!entry_ptr->ext_call_behaviors[SIGSAFE]);
	mumps_status = save_mumps_status;
	/* The first byte of the type description argument gets set to 0xFF in case error happened in JNI glue code,
	 * so check for that and act accordingly.
	 */
	if ((char)0xFF == *(char *)param_list->arg[0])
	{
		error_in_xc = TRUE;
		jni_err_buf = *(char **)((char *)param_list->arg[0] + SIZEOF(char *));
		if (NULL != jni_err_buf)
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_JNI, 2, LEN_AND_STR(jni_err_buf));
		else
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_ZCSTATUSRET);
	}
	free(types_descr_ptr);
	/* Exit from the residual call-in environment(SFT_CI base frame) which might still exist on M stack when the externally
	 * called function in turn called into an M routine.
	 */
	if (frame_pointer->type & SFT_CI)
		ci_ret_code_quit();
	/* Only process the input-output and output-only arguments if the external call succeeded; otherwise, return -1
	 * if non-void return is expected.
	 */
	if (!error_in_xc)
	{	/* Compute space requirement for return values. */
		n = 0;
		VAR_COPY(var_copy, var);
		for (i = 0, j = 1, m1 = mask & entry_ptr->output_mask; i < argcnt; j++)
		{
			v = va_arg(var, mval *);
			if (j < 3)
				continue;
			if (MASK_BIT_ON(m1))
				n += extarg_getsize(param_list->arg[j], entry_ptr->parms[i], v, entry_ptr);
			i++;
			m1 = m1 >> 1;
		}
		va_end(var);
		if (dst)
			n += extarg_getsize((void *)status, entry_ptr->return_type, dst, entry_ptr);
		ENSURE_STP_FREE_SPACE(n);
		/* Convert return values. */
		for (i = 0, j = 1, m1 = mask & entry_ptr->output_mask; i < argcnt; j++)
		{
			v = va_arg(var_copy, mval *);
			if (j < 3)
				continue;
			if (MASK_BIT_ON(m1))
				extarg2mval((void *)param_list->arg[j], entry_ptr->parms[i], v, TRUE, TRUE,
					entry_ptr->param_pre_alloc_size[i], entry_ptr->entry_name.addr, param_list, call_buff_size);
			i++;
			m1 = m1 >> 1;
		}
		va_end(var_copy);
		if (dst)
		{
			if (entry_ptr->return_type != ydb_void)
				extarg2mval((void *)status, entry_ptr->return_type, dst, TRUE, FALSE,
						-1, entry_ptr->entry_name.addr, param_list, call_buff_size);
			else
			{
				if (package->str.len)
					xtrnl_table_name = ydb_getenv(YDBENVINDX_XC_PREFIX, &package->str, NULL_IS_YDB_ENV_MATCH);
				else
					xtrnl_table_name = ydb_getenv(YDBENVINDX_XC, NULL_SUFFIX, NULL_IS_YDB_ENV_MATCH);
				assert(NULL != xtrnl_table_name);	/* or else a ZCCTENV error would have been issued earlier */
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_XCVOIDRET, 4,
					  LEN_AND_STR(entry_ptr->call_name.addr), LEN_AND_STR(xtrnl_table_name));
			}
		}
	} else if (dst && (ydb_void != entry_ptr->return_type))
		i2mval(dst, -1);
	free_return_type(status, entry_ptr->return_type);
	return;
}

void op_fnfgncal(uint4 n_mvals, mval *dst, mval *package, mval *extref, uint4 mask, int4 argcnt, ...)
{
	boolean_t	java = FALSE, save_in_ext_call, is_tpretry;
	char		*free_string_pointer, *free_string_pointer_start, *param_list_buff;
	char		*xtrnl_table_name;
	int		i, pre_alloc_size, rslt, save_mumps_status;
	int4 		callintogtm_vectorindex, n, call_buff_size;
	gparam_list	*param_list;
	ydb_long_t	*free_space_pointer;
	INTPTR_T	status;
	mval		*v;
	struct extcall_package_list	*package_ptr;
	struct extcall_entry_list	*entry_ptr;
	uint4		m1;
	va_list		var;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(n_mvals == argcnt + 5);
	assert(MV_IS_STRING(package));	/* Package and routine are literal strings */
	assert(MV_IS_STRING(extref));
	if (MAX_ACTUALS < argcnt)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZCMAXPARAM);
	assert(INTRPT_OK_TO_INTERRUPT == intrpt_ok_state); /* Interrupts should be enabled for external calls */
	/* Find package */
	for (package_ptr = TREF(extcall_package_root); package_ptr; package_ptr = package_ptr->next_package)
	{
		MSTR_CMP(package_ptr->package_name, package->str, rslt);
		if (0 == rslt)
			break;
	}
	/* If package has not been found, create it */
	if (NULL == package_ptr)
	{
		package_ptr = exttab_parse(package);
		package_ptr->next_package = TREF(extcall_package_root);
		TREF(extcall_package_root) = package_ptr;
	}
	/* At this point, we have a valid package, pointed to by package_ptr */
	assert(NULL != package_ptr);
	if (NULL == package_ptr->package_handle)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) errno);
	/* Find entry */
	for (entry_ptr = package_ptr->first_entry; entry_ptr; entry_ptr = entry_ptr->next_entry)
	{
		MSTR_CMP(entry_ptr->entry_name, extref->str, rslt);
		if (0 == rslt)
			break;
	}
	if (call_table_initialized == FALSE)
	{
		call_table_initialized = TRUE;
		init_callin_functable();
	}
	/* Entry not found */
	if ((NULL == entry_ptr) || (NULL == entry_ptr->fcn) || (NULL == entry_ptr->call_name.addr))
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_ZCRTENOTF, 2, extref->str.len, extref->str.addr);
	/* Detect a call-out to Java. The java plugin still has references to "gtm_xcj" (not "ydb_xcj") hence the below check. */
	if (!strncmp(entry_ptr->call_name.addr, "gtm_xcj", 7))
	{
		java = TRUE;
		argcnt -= 2;
	}
	/* It is an error to have more actual parameters than formal parameters */
	if (argcnt > entry_ptr->argcnt)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_ZCARGMSMTCH, 2, argcnt, entry_ptr->argcnt);
	/* If $PRINCIPAL is a terminal device, and it has unflushed data on the output device side, flush that
	 * before making the foreign call as otherwise the foreign call could in turn write to the terminal resulting
	 * in out-of-order terminal writes if the unflushed data gets flushed after the foreign call (YDB#940).
	 */
	if (tt == io_std_device.out->type)
	{
		d_tt_struct	*tt_ptr;

		tt_ptr = io_std_device.out->dev_sp;
		/* Since "iott_flush()" does ESTABLISH (i.e. setjmp() calls), it is best for performance to avoid calling it if
		 * not necessary. Hence the use of the macro below to check if there is any unflushed data and only then call it.
		 */
		if (0 != TT_UNFLUSHED_DATA_LEN(tt_ptr))
			iott_flush(io_std_device.out);
	}
	VAR_START(var, argcnt);
	if (java)
	{
		op_fgnjavacal(dst, package, extref, mask, argcnt, entry_ptr->argcnt, package_ptr, entry_ptr, var);
		return;
	}
	/* Compute size of parameter block */
	n = entry_ptr->parmblk_size;	/* This is enough for the parameters and the fixed length entries */
	/* Now, add enough for the char *'s and the char **'s and string *'s */
	for (i = 0, m1 = entry_ptr->input_mask; i < argcnt; i++, m1 = m1 >> 1)
	{
		v = va_arg(var, mval *);
		/* For char*, char **, and ydb_string_t * types, add the length. Also a good time to force it into string form. */
		switch (entry_ptr->parms[i])
		{
			case ydb_string_star:	/* CAUTION: Fall-through. */
			case ydb_buffer_star:	/* CAUTION: Fall-through. */
			case ydb_char_star:
				n += (-1 != entry_ptr->param_pre_alloc_size[i]) ? entry_ptr->param_pre_alloc_size[i] : 0;
				/* CAUTION: Fall-through. */
			case ydb_char_starstar:
				if (MASK_BIT_ON(m1))
				{
					if (MV_DEFINED(v))
					{
						MV_FORCE_STR(v);
						n += v->str.len + 1;	/* Note: ydb_string_star and ydb_buffer_star
									 * do not really need the extra byte.
									 */
					} else
					{
						MV_FORCE_DEFINED_UNLESS_SKIPARG(v);
						n += 1;
					}
				}
				break;
#			ifndef GTM64
			case ydb_double_star:
				n += SIZEOF(double);
				/* CAUTION: Fall-through. */
#			endif
			default:
				if (MASK_BIT_ON(m1))
				{
					MV_FORCE_DEFINED_UNLESS_SKIPARG(v);
				}
		}
	}
	va_end(var);
        /* Double the size, to take care of any alignments in the middle. Overall, the allocated space has the following structure:
	 *   ___________________________________________
	 *  |            |              |               |
	 *  | param_list | space buffer | string buffer |
	 *  |____________|______________|_______________|
	 *
	 * For pointer-type arguments (ydb_long_t *, ydb_float_t *, etc.) the value in param_list is a pointer to a slot inside the
	 * space buffer, unless it is ydb_char_t *, in which case the string buffer is used. For double-pointer types (char ** or
	 * ydb_string_t *) the value in param_list is always a pointer inside the space buffer, where a pointer to an area inside
	 * the string buffer is stored.
	 */
	/* Note that this is the nominal buffer size; or, explicitly, the size of buffer without the protection tags */
	call_buff_size = 2 * n;
	OP_FNFGNCAL_ALLOCATE(param_list_buff, call_buff_size + 2 * buff_boarder_len);
	param_list = set_up_buffer(param_list_buff, 2 * n);
	free_space_pointer = (ydb_long_t *)((char *)param_list + SIZEOF(intszofptr_t) + (SIZEOF(void *) * argcnt));
	free_string_pointer_start = free_string_pointer = (char *)param_list + entry_ptr->parmblk_size;
	/* Load-up the parameter list */
	VAR_START(var, argcnt);
	for (i = 0, m1 = entry_ptr->input_mask; i < argcnt; i++, m1 = m1 >> 1)
	{
		v = va_arg(var, mval *);
		/* Verify that all input values are defined */
		pre_alloc_size = entry_ptr->param_pre_alloc_size[i];
		switch (entry_ptr->parms[i])
		{
			case ydb_uint:
				param_list->arg[i] = (void *)(ydb_ulong_t)(MV_ON(m1, v) ? mval2ui(v) : 0);
				/* Note: output ydb_int and ydb_uint is an error (only "star" flavor can be modified). */
				break;
			case ydb_int:
				param_list->arg[i] = (void *)(ydb_long_t)(MV_ON(m1, v) ? mval2i(v) : 0);
				break;
			case ydb_ulong:
				param_list->arg[i] = (void *)GTM64_ONLY((gtm_uint64_t)) NON_GTM64_ONLY((ydb_ulong_t))
					(MV_ON(m1, v) ? GTM64_ONLY(mval2ui8(v)) NON_GTM64_ONLY(mval2ui(v)) : 0);
				/* Note: output xc_long and xc_ulong is an error as described above. */
				break;
			case ydb_long:
				param_list->arg[i] = (void *)GTM64_ONLY((gtm_int64_t)) NON_GTM64_ONLY((ydb_long_t))
					(MV_ON(m1, v) ? GTM64_ONLY(mval2i8(v)) NON_GTM64_ONLY(mval2i(v)) : 0);
				break;
#			ifdef GTM64
			case ydb_uint64:
				param_list->arg[i] = (void *)(ydb_uint64_t)(MV_ON(m1, v) ? mval2ui8(v) : 0);
				/* Note: output xc_long and xc_ulong is an error as described above. */
				break;
			case ydb_int64:
				param_list->arg[i] = (void *)(ydb_int64_t)(MV_ON(m1, v) ? mval2i8(v) : 0);
				break;
#			endif
			case ydb_char_star:
				param_list->arg[i] = free_string_pointer;
				if (MASK_BIT_ON(m1))
				{	/* If this is defined and input-enabled, it should have already been forced to string. */
					assert(!MV_DEFINED(v) || MV_IS_STRING(v));
					if (MV_DEFINED(v) && v->str.len)
					{
						memcpy(free_string_pointer, v->str.addr, v->str.len);
						free_string_pointer += v->str.len;
					}
					*free_string_pointer++ = '\0';
				} else if (0 < pre_alloc_size)
				{
					*free_string_pointer = '\0';
					free_string_pointer += pre_alloc_size;
				} else /* Output and no pre-allocation specified */
				{
					if (0 == package->str.len)
						/* Default package - do not display package name */
						RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(7) ERR_ZCNOPREALLOUTPAR, 5, i + 1,
							RTS_ERROR_LITERAL("<DEFAULT>"), extref->str.len, extref->str.addr);
					else
						RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(7) ERR_ZCNOPREALLOUTPAR, 5, i + 1,
							package->str.len, package->str.addr, extref->str.len, extref->str.addr);
				}
				break;
			case ydb_char_starstar:
				param_list->arg[i] = free_space_pointer;
				/* If this is defined and input-enabled, it should have been forced to string in an earlier loop. */
				assert(!MV_ON(m1, v) || MV_IS_STRING(v));
				*(char **)free_space_pointer = free_string_pointer;
				if (MV_ON(m1, v) && v->str.len)
				{
					memcpy(free_string_pointer, v->str.addr, v->str.len);
					free_string_pointer += v->str.len;
				}
				*free_string_pointer++ = '\0';
				free_space_pointer++;
				break;
			case ydb_int_star:
				param_list->arg[i] = free_space_pointer;
				*((ydb_int_t *)free_space_pointer) = MV_ON(m1, v) ? (ydb_int_t)mval2i(v) : 0;
				free_space_pointer++;
				break;
			case ydb_uint_star:
				param_list->arg[i] = free_space_pointer;
				*((ydb_uint_t *)free_space_pointer) = MV_ON(m1, v) ? (ydb_uint_t)mval2ui(v) : 0;
				free_space_pointer++;
				break;
			case ydb_long_star:
				param_list->arg[i] = free_space_pointer;
				GTM64_ONLY(*((gtm_int64_t *)free_space_pointer) = MV_ON(m1, v) ? (gtm_int64_t)mval2i8(v) : 0);
				NON_GTM64_ONLY(*((ydb_long_t *)free_space_pointer) = MV_ON(m1, v) ? (ydb_long_t)mval2i(v) : 0);
				free_space_pointer++;
				break;
			case ydb_ulong_star:
				param_list->arg[i] = free_space_pointer;
				GTM64_ONLY(*((gtm_uint64_t *)free_space_pointer) = MV_ON(m1, v) ? (gtm_uint64_t)mval2ui8(v) : 0);
				NON_GTM64_ONLY(*((ydb_ulong_t *)free_space_pointer) = MV_ON(m1, v) ? (ydb_ulong_t)mval2ui(v) : 0);
				free_space_pointer++;
				break;
			case ydb_int64_star:
				param_list->arg[i] = free_space_pointer;
				*((gtm_int64_t *)free_space_pointer) = MV_ON(m1, v) ? (ydb_int64_t)mval2i8(v) : 0;
				free_space_pointer++;
				break;
			case ydb_uint64_star:
				param_list->arg[i] = free_space_pointer;
				*((gtm_uint64_t *)free_space_pointer) = MV_ON(m1, v) ? (ydb_uint64_t)mval2ui8(v) : 0;
				free_space_pointer++;
				break;
			case ydb_string_star:
				param_list->arg[i] = free_space_pointer;
				if (MASK_BIT_ON(m1))
				{	/* If this is defined and input-enabled, it should have already been forced to string. */
					assert(!MV_DEFINED(v) || MV_IS_STRING(v));
					if (MV_DEFINED(v) && v->str.len)
					{
						*free_space_pointer++ = (ydb_long_t)v->str.len;
						*(char **)free_space_pointer = (char *)free_string_pointer;
						memcpy(free_string_pointer, v->str.addr, v->str.len);
						free_string_pointer += v->str.len;
						free_space_pointer++;
					} else
					{
						*free_space_pointer++ = 0;
						*free_space_pointer++ = 0;	/* Effectively a NULL pointer. */
					}
				} else if (0 < pre_alloc_size)
				{
					*free_space_pointer++ = (ydb_long_t)pre_alloc_size;
					*(char **)free_space_pointer = (char *)free_string_pointer;
					*free_string_pointer = '\0';
					free_space_pointer++;
					free_string_pointer += pre_alloc_size;
				} else /* Output and no pre-allocation specified */
				{
					if (0 == package->str.len)
						/* Default package - do not display package name */
						RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(7) ERR_ZCNOPREALLOUTPAR, 5, i + 1,
							RTS_ERROR_LITERAL("<DEFAULT>"),
							extref->str.len, extref->str.addr);
					else
						RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(7) ERR_ZCNOPREALLOUTPAR, 5, i + 1,
							package->str.len, package->str.addr,
							extref->str.len, extref->str.addr);
				}
				break;
			case ydb_buffer_star:
				param_list->arg[i] = free_space_pointer;
				if (MASK_BIT_ON(m1))
				{	/* If this is defined and input-enabled, it should have already been forced to string. */
					ydb_buffer_t	*buff_ptr;

					buff_ptr = (ydb_buffer_t *)free_space_pointer;
					assert(!MV_DEFINED(v) || MV_IS_STRING(v));
					if (MV_DEFINED(v) && v->str.len)
					{
						buff_ptr->len_alloc = buff_ptr->len_used = v->str.len;
						buff_ptr->buf_addr = (char *)free_string_pointer;
						memcpy(buff_ptr->buf_addr, v->str.addr, v->str.len);
						free_string_pointer += v->str.len;
					} else
					{
						buff_ptr->len_alloc = buff_ptr->len_used = 0;
						buff_ptr->buf_addr = NULL;
					}
					free_space_pointer = (ydb_long_t *)((char *)free_space_pointer + SIZEOF(ydb_buffer_t));
				} else if (0 < pre_alloc_size)
				{
					ydb_buffer_t	*buff_ptr;

					buff_ptr = (ydb_buffer_t *)free_space_pointer;
					buff_ptr->len_alloc = pre_alloc_size;
					buff_ptr->len_used = 0;
					buff_ptr->buf_addr = (char *)free_string_pointer;
					free_space_pointer = (ydb_long_t *)((char *)free_space_pointer + SIZEOF(ydb_buffer_t));
					free_string_pointer += pre_alloc_size;
				} else /* Output and no pre-allocation specified */
				{
					if (0 == package->str.len)
						/* Default package - do not display package name */
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_ZCNOPREALLOUTPAR, 5, i + 1,
							  RTS_ERROR_LITERAL("<DEFAULT>"),
							  extref->str.len, extref->str.addr);
					else
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_ZCNOPREALLOUTPAR, 5, i + 1,
							  package->str.len, package->str.addr,
							  extref->str.len, extref->str.addr);
				}
				break;
			case ydb_float_star:
				param_list->arg[i] = free_space_pointer;
				*((float *)free_space_pointer) = MV_ON(m1, v) ? (float)mval2double(v) : (float)0.0;
				free_space_pointer++;
				break;
			case ydb_double_star:
				/* Only need to do this rounding on non-64 bit platforms because this one type has a 64 bit
				 * alignment requirement on those platforms.
				 */
				NON_GTM64_ONLY(free_space_pointer = (ydb_long_t *)(ROUND_UP2(((INTPTR_T)free_space_pointer),
											    SIZEOF(double))));
				param_list->arg[i] = free_space_pointer;
				*((double *)free_space_pointer) = MV_ON(m1, v) ? (double)mval2double(v) : 0.0;
				free_space_pointer += (SIZEOF(double) / SIZEOF(ydb_long_t));
				break;
			case ydb_pointertofunc:
				callintogtm_vectorindex = MV_DEFINED(v) ? (int4)mval2i(v) : 0;
				if ((callintogtm_vectorindex >= gtmfunc_unknown_function) || (callintogtm_vectorindex < 0))
				{
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_ZCVECTORINDX, 1, callintogtm_vectorindex,
						ERR_TEXT, 2, RTS_ERROR_TEXT("Passing Null vector"));
					param_list->arg[i] = 0;
				} else
					param_list->arg[i] = (void *)callintogtm_vectortable[callintogtm_vectorindex];
				break;
			case ydb_pointertofunc_star:
				/* Cannot pass in a function address to be modified by the user program */
				free_space_pointer = (ydb_long_t *)ROUND_UP2(((INTPTR_T)free_space_pointer), SIZEOF(INTPTR_T));
				param_list->arg[i] = free_space_pointer;
				*((INTPTR_T *)free_space_pointer) = 0;
				free_space_pointer++;
				break;
			default:
				va_end(var);
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_UNIMPLOP);
				break;
		}
	}
	assert((char *)free_space_pointer <= free_string_pointer_start);
	va_end(var);
	param_list->n = argcnt;
	VERIFY_STORAGE_CHAINS;
	save_mumps_status = mumps_status; /* Save mumps_status as a callin from external call may change it */
	save_in_ext_call = TREF(in_ext_call);
	assert(INTRPT_OK_TO_INTERRUPT == intrpt_ok_state);
	TREF(in_ext_call) = TRUE;
	status = callg((callgfnptr)entry_ptr->fcn, param_list);
	TREF(in_ext_call) = save_in_ext_call;
	is_tpretry = (ERR_TPRETRY == mumps_status);	/* note down whether the callg invocation had a TPRETRY error code */
	mumps_status = save_mumps_status;
	verify_buffer((char *)param_list, (2*n), entry_ptr->entry_name.addr);
	check_for_timer_pops(!entry_ptr->ext_call_behaviors[SIGSAFE]);
	/* Exit from the residual call-in environment(SFT_CI base frame) which might
	 * still exist on M stack when the externally called function in turn called
	 * into an M routine.
	 */
	if (frame_pointer->type & SFT_CI)
		ci_ret_code_quit();
	/* NOTE: ADD RETURN STATUS CALCUATIONS HERE */
	/* Compute space requirement for return values */
	n = 0;
	VAR_START(var, argcnt);
	for (i = 0, m1 = mask & entry_ptr->output_mask; i < argcnt; i++, m1 = m1 >> 1)
	{
		v = va_arg(var, mval *);
		if (MASK_BIT_ON(m1))
			n += extarg_getsize(param_list->arg[i], entry_ptr->parms[i], v, entry_ptr);
	}
	va_end(var);
	if (dst)
		n += extarg_getsize((void *)status, entry_ptr->return_type, dst, entry_ptr);
	ENSURE_STP_FREE_SPACE(n);
	/* Convert return values */
	VAR_START(var, argcnt);
	for (i = 0, m1 = mask & entry_ptr->output_mask; i < argcnt; i++, m1 = m1 >> 1)
	{
		v = va_arg(var, mval *);
		if (MASK_BIT_ON(m1))
			extarg2mval((void *)param_list->arg[i], entry_ptr->parms[i], v, FALSE, TRUE,
				entry_ptr->param_pre_alloc_size[i], entry_ptr->entry_name.addr, param_list, call_buff_size);
	}
	va_end(var);
	if (dst)
	{
		if (entry_ptr->return_type != ydb_void)
			extarg2mval((void *)status, entry_ptr->return_type, dst, FALSE, FALSE, -1, entry_ptr->entry_name.addr,
					param_list, call_buff_size);
		else
		{
			if (package->str.len)
				xtrnl_table_name = ydb_getenv(YDBENVINDX_XC_PREFIX, &package->str, NULL_IS_YDB_ENV_MATCH);
			else
				xtrnl_table_name = ydb_getenv(YDBENVINDX_XC, NULL_SUFFIX, NULL_IS_YDB_ENV_MATCH);
			assert(NULL != xtrnl_table_name);	/* or else a ZCCTENV error would have been issued earlier */
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_XCVOIDRET, 4,
				  LEN_AND_STR(entry_ptr->call_name.addr), LEN_AND_STR(xtrnl_table_name));
		}
	}
	free_return_type(status, entry_ptr->return_type);
	if (is_tpretry)
	{	/* A TPRETRY error code occurred inside the "callg" invocation.
		 * Now that all cleanup (free of param_list etc.) has happened,
		 * it is okay to bubble the ERR_TPRETRY error upto the caller.
		 */
		INVOKE_RESTART;
	}
	return;
}
