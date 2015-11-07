/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#  include <pthread.h>
#endif
#include "gtm_stdlib.h"

#include "stringpool.h"
#include "copy.h"
#include <rtnhdr.h>
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
 *		void * fgn_getpak(char *package_name)
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
#ifdef GTM_PTHREAD
GBLREF boolean_t	gtm_jvm_process;
GBLREF pthread_t	gtm_main_thread_id;
GBLREF boolean_t	gtm_main_thread_id_set;
#endif

error_def(ERR_JNI);
error_def(ERR_MAXSTRLEN);
error_def(ERR_TEXT);
error_def(ERR_UNIMPLOP);
error_def(ERR_XCVOIDRET);
error_def(ERR_ZCARGMSMTCH);
error_def(ERR_ZCCTENV);
error_def(ERR_ZCMAXPARAM);
error_def(ERR_ZCNOPREALLOUTPAR);
error_def(ERR_ZCRTENOTF);
error_def(ERR_ZCSTATUSRET);
error_def(ERR_ZCUSRRTN);
error_def(ERR_ZCVECTORINDX);

STATICDEF int		call_table_initialized = 0;
/* The following are one-letter mnemonics for Java argument types (capital letters to denote output direction):
 * 						boolean	int	long	float	double	String	byte[] */
STATICDEF char		gtm_jtype_chars[] = {	'b',	'i',	'l',	'f',	'd',	'j',	'a',
						'B',	'I',	'L',	'F',	'D',	'J',	'A' };
STATICDEF int		gtm_jtype_start_idx = gtm_jboolean,	/* Value of first gtm_j... type for calculation of table indices. */
	  		gtm_jtype_count = gtm_jbyte_array - gtm_jboolean + 1;	/* Number of types supported with Java call-outs. */

STATICFNDCL void	extarg2mval(void *src, enum gtm_types typ, mval *dst, boolean_t java, boolean_t starred);
STATICFNDCL int		extarg_getsize(void *src, enum gtm_types typ, mval *dst);
STATICFNDCL void	op_fgnjavacal(mval *dst, mval *package, mval *extref, uint4 mask, int4 argcnt, int4 entry_argcnt,
				struct extcall_package_list *package_ptr, struct extcall_entry_list *entry_ptr, va_list var);

/* Routine to convert external return values to mval's */
STATICFNDEF void extarg2mval(void *src, enum gtm_types typ, mval *dst, boolean_t java, boolean_t starred)
{
	gtm_int_t		s_int_num;
	gtm_long_t		str_len, s_long_num;
	gtm_uint_t		uns_int_num;
	gtm_ulong_t		uns_long_num;
	char			*cp;
	struct extcall_string	*sp;

	if (java)
	{
		switch(typ)
		{
			case gtm_notfound:
				break;
			case gtm_void:
				break;
			case gtm_status:
				/* Note: reason for double cast is to first turn ptr to same sized int, then big int to little int
				 * (on 64 bit platforms). This avoids a warning msg with newer 64 bit gcc compilers.
				 */
				s_int_num = (gtm_int_t)(intszofptr_t)src;
				if (0 != s_int_num)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZCSTATUSRET);
				MV_FORCE_MVAL(dst, s_int_num);
				break;
			case gtm_jboolean:
			case gtm_jint:
				if (starred)
					s_int_num = *((gtm_int_t *)src);
				else
					s_int_num = (gtm_int_t)(intszofptr_t)src;
				MV_FORCE_MVAL(dst, s_int_num);
				break;
			case gtm_jlong:
#				ifdef GTM64
				if (starred)
					s_long_num = *((gtm_long_t *)src);
				else
					s_long_num = (gtm_long_t)src;
				MV_FORCE_LMVAL(dst, s_long_num);
#				else
				i82mval(dst, *(gtm_int64_t *)src);
#				endif
				break;
			case gtm_jfloat:
				float2mval(dst, *((float *)src));
				break;
			case gtm_jdouble:
				double2mval(dst, *((double *)src));
				break;
			case gtm_jstring:
			case gtm_jbyte_array:
				sp = (struct extcall_string *)src;
				dst->mvtype = MV_STR;
				if (sp->len > MAX_STRLEN)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXSTRLEN);
				dst->str.len = (mstr_len_t)sp->len;
				if ((0 < sp->len) && (NULL != sp->addr))
				{
					dst->str.addr = sp->addr;
					s2pool(&dst->str);
					/* In case of GTMByteArray or GTMString, the buffer is allocated in xc_gateway.c (since the
					 * user might need to return a bigger buffer than the original array size will allow (in
					 * case of a string the content is immutable). So, if we have determined that the provided
					 * value buffer is legitimate (non-null and non-empty), we free it on the GT.M side. */
					free(sp->addr);
				}
				break;
			default:
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_UNIMPLOP);
				break;
		}
		return;
	}
	/* The following switch is for non-Java call-outs. */
	switch(typ)
	{
		case gtm_notfound:
			break;
		case gtm_void:
			break;
		case gtm_status:
			/* Note: reason for double cast is to first turn ptr to same sized int, then big int to little int
			 * (on 64 bit platforms). This avoids a warning msg with newer 64 bit gcc compilers.
			 */
			s_int_num = (gtm_int_t)(intszofptr_t)src;
			if (0 != s_int_num)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZCSTATUSRET);
			MV_FORCE_MVAL(dst, s_int_num);
			break;
		case gtm_int:
			s_int_num = (gtm_int_t)(intszofptr_t)src;
			MV_FORCE_MVAL(dst, s_int_num);
			break;
		case gtm_uint:
			uns_int_num = (gtm_uint_t)(intszofptr_t)src;
			MV_FORCE_UMVAL(dst, uns_int_num);
			break;
		case gtm_int_star:
			s_int_num = *((gtm_int_t *)src);
			MV_FORCE_MVAL(dst, s_int_num);
			break;
		case gtm_uint_star:
			uns_int_num = *((gtm_uint_t *)src);
			MV_FORCE_UMVAL(dst, uns_int_num);
			break;
		case gtm_long:
			s_long_num = (gtm_long_t)src;
			MV_FORCE_LMVAL(dst, s_long_num);
			break;
		case gtm_ulong:
			uns_long_num = (gtm_ulong_t)src;
			MV_FORCE_ULMVAL(dst, uns_long_num);
			break;
		case gtm_long_star:
			s_long_num = *((gtm_long_t *)src);
			MV_FORCE_LMVAL(dst, s_long_num);
			break;
		case gtm_ulong_star:
			uns_long_num = *((gtm_ulong_t *)src);
			MV_FORCE_ULMVAL(dst, uns_long_num);
			break;
		case gtm_string_star:
			sp = (struct extcall_string *)src;
			dst->mvtype = MV_STR;
			if (sp->len > MAX_STRLEN)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXSTRLEN);
			dst->str.len = (mstr_len_t)sp->len;
			if ((0 < sp->len) && (NULL != sp->addr))
			{
				dst->str.addr = sp->addr;
				s2pool(&dst->str);
			}
			break;
		case gtm_float_star:
			float2mval(dst, *((float *)src));
			break;
		case gtm_char_star:
			cp = (char *)src;
			assert(((INTPTR_T)cp < (INTPTR_T)stringpool.base) || ((INTPTR_T)cp > (INTPTR_T)stringpool.top));
			dst->mvtype = MV_STR;
			str_len = STRLEN(cp);
			if (str_len > MAX_STRLEN)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXSTRLEN);
			dst->str.len = (mstr_len_t)str_len;
			dst->str.addr = cp;
			s2pool(&dst->str);
			break;
		case gtm_char_starstar:
			if (!src)
				dst->mvtype = 0;
			else
				extarg2mval(*((char **)src), gtm_char_star, dst, java, starred);
			break;
		case gtm_double_star:
			double2mval(dst, *((double *)src));
			break;
		default:
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_UNIMPLOP);
			break;
	}
	return;
}

/* Subroutine to calculate stringpool requirements for an external argument */
STATICFNDEF int extarg_getsize(void *src, enum gtm_types typ, mval *dst)
{
	char			*cp, **cpp;
	struct extcall_string	*sp;

	if (!src)
		return 0;
	switch(typ)
	{	/* The following group of cases either return nothing or use the numeric part of the mval */
		case gtm_notfound:
		case gtm_void:
		case gtm_double_star:
		case gtm_status:
		case gtm_int:
		case gtm_uint:
		case gtm_long:
		case gtm_ulong:
		case gtm_float_star:
		case gtm_int_star:
		case gtm_uint_star:
		case gtm_long_star:
		case gtm_ulong_star:
		case gtm_jboolean:
		case gtm_jint:
		case gtm_jlong:
		case gtm_jfloat:
		case gtm_jdouble:
			return 0;
		case gtm_char_starstar:
			cpp = (char **)src;
			if (*cpp)
				return STRLEN(*cpp);
			else
				return 0;
		case gtm_char_star:
			cp = (char *)src;
			return STRLEN(cp);
		case gtm_jstring:
		case gtm_jbyte_array:
		case gtm_string_star:
			sp = (struct extcall_string *)src;
			if ((0 < sp->len)
			    && ((INTPTR_T)sp->addr < (INTPTR_T)stringpool.free)
			    && ((INTPTR_T)sp->addr >= (INTPTR_T)stringpool.base))
			{	/* The stuff is already in the stringpool */
				assert(dst->str.addr == sp->addr);
				dst->str.addr = sp->addr;
				sp->addr = NULL;	/* Prevent subsequent s2pool */
				return 0;
			} else  if (NULL == sp->addr)
				sp->len = 0;
			return (int)(sp->len);
			break;
		default:
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_UNIMPLOP);
			break;
	}

	return 0; /* This should never get executed, added to make compiler happy */
}

STATICFNDEF void op_fgnjavacal(mval *dst, mval *package, mval *extref, uint4 mask, int4 argcnt, int4 entry_argcnt,
    struct extcall_package_list *package_ptr, struct extcall_entry_list *entry_ptr, va_list var)
{
	boolean_t	error_in_xc = FALSE;
	char		*free_string_pointer, *free_string_pointer_start, jtype_char;
	char		str_buffer[MAX_NAME_LENGTH], *tmp_buff_ptr, *jni_err_buf;
	char		*types_descr_ptr, *types_descr_dptr, *xtrnl_table_name;
	gparam_list	*param_list;
	gtm_long_t	*free_space_pointer;
	int		i, j, save_mumps_status;
	int4 		m1, m2, n;
	INTPTR_T	status;
	mval		*v;
	va_list		var_copy;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	ifdef GTM_PTHREAD
	assert(gtm_jvm_process == gtm_main_thread_id_set);
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
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZCMAXPARAM);
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
	types_descr_dptr = types_descr_ptr;
	*types_descr_dptr = (char)entry_argcnt;
	types_descr_dptr++;
	if (dst)
	{	/* Record the expected return type. */
		switch (entry_ptr->return_type)
		{
			case gtm_status:
				*types_descr_dptr = 'i';
				break;
			case gtm_jlong:
				*types_descr_dptr = 'l';
				break;
			default:
				*types_descr_dptr = 'v';
		}
	} else
		*types_descr_dptr = 'v';
	types_descr_dptr++;
	assert(2 * gtm_jtype_count == SIZEOF(gtm_jtype_chars));
	for (i = argcnt + 2, j = -2, m1 = entry_ptr->input_mask, m2 = mask & entry_ptr->output_mask; 0 < i; i--, j++)
	{	/* Enforce mval values and record expected argument types. */
		v = va_arg(var, mval *);
		if (0 > j)
		{
			MV_FORCE_STR(v);
			n += v->str.len + SIZEOF(gtm_long_t) + 1;
			continue;
		}
		if (m1 & 1)
		{
			if ((gtm_jstring == entry_ptr->parms[j]) || (gtm_jbyte_array == entry_ptr->parms[j]))
			{
				MV_FORCE_STR(v);
				n += v->str.len + SIZEOF(gtm_long_t) + 1;
			} else
			{
				MV_FORCE_DEFINED(v);
			}
		}
		jtype_char = entry_ptr->parms[j] - gtm_jtype_start_idx;
		if ((0 > jtype_char) || (gtm_jtype_count <= jtype_char))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_UNIMPLOP);
		else
			*types_descr_dptr = gtm_jtype_chars[(m2 & 1) ? (gtm_jtype_count + jtype_char) : jtype_char];
		types_descr_dptr++;
		m1 >>= 1;
		m2 >>= 1;
	}
	va_end(var);
        /* Double the size, to take care of any alignments in the middle. */
	param_list = (gparam_list *)malloc(n * 2);
	param_list->arg[0] = (void *)types_descr_ptr;
	/* Adding 3 to account for type descriptions, class name, and method name arguments. */
	free_space_pointer = (gtm_long_t *)((char *)param_list + SIZEOF(intszofptr_t) + (SIZEOF(void *) * (argcnt + 3)));
	/* Adding 3 for the same reason as above and another 3 to account for the fact that each of type description, class name,
	 * and method name arguments require room in the free_space buffer, which comes ahead of free_string buffer in memory.
	 */
	free_string_pointer_start = free_string_pointer = (char *)param_list + entry_ptr->parmblk_size + (SIZEOF(void *) * (3 + 3));
	/* Load-up the parameter list */
	VAR_COPY(var, var_copy);
	/* We need to enter this loop even if argcnt == 0, so that the class and method arguments get set. */
	for (i = (0 == argcnt ? -1 : 0), j = 1, m1 = entry_ptr->input_mask, m2 = mask & entry_ptr->output_mask; i < argcnt; j++)
	{
		v = va_arg(var_copy, mval *);
		if (j < 3)
		{
			param_list->arg[j] = free_string_pointer;
			if (v->str.len)
				memcpy(free_string_pointer, v->str.addr, v->str.len);
			free_string_pointer += v->str.len;
			*free_string_pointer++ = 0;
			/* In case there are 0 arguments. */
			if (2 == j && 0 > i)
				i = 0;
			continue;
		}
		/* Verify that all input values are defined. */
		switch (entry_ptr->parms[i])
		{
			case gtm_jboolean:
				if (m2 & 1)
				{	/* Output expected. */
					param_list->arg[j] = free_space_pointer;
					*((gtm_int_t *)free_space_pointer) = (m1 & 1) ? ((gtm_int_t)(mval2i(v) ? 1 : 0)) : 0;
					free_space_pointer++;
				} else if (m1 & 1)
					param_list->arg[j] = (void *)((gtm_long_t)(mval2i(v) ? 1 : 0));
				break;
			case gtm_jint:
				if (m2 & 1)
				{	/* Output expected. */
					param_list->arg[j] = free_space_pointer;
					*((gtm_int_t *)free_space_pointer) = (m1 & 1) ? (gtm_int_t)mval2i(v) : 0;
					free_space_pointer++;
				} else if (m1 & 1)
					param_list->arg[j] = (void *)(gtm_long_t)mval2i(v);
				break;
			case gtm_jlong:
				if (m2 & 1)
				{	/* Output expected. */
#					ifndef GTM64
					free_space_pointer = (gtm_long_t *)(ROUND_UP2(((INTPTR_T)free_space_pointer),
						SIZEOF(gtm_int64_t)));
#					endif
					param_list->arg[j] = free_space_pointer;
					*((gtm_int64_t *)free_space_pointer) = (m1 & 1) ? (gtm_int64_t)mval2i8(v) : 0;
					free_space_pointer = (gtm_long_t *)((char *)free_space_pointer + SIZEOF(gtm_int64_t));
				} else if (m1 & 1)
				{
#					ifdef GTM64
					param_list->arg[j] = (void *)(gtm_int64_t)mval2i8(v);
#					else
					/* Only need to do this rounding on non-64 it platforms because this one type has a 64 bit
					 * alignment requirement on those platforms.
					 */
					free_space_pointer = (gtm_long_t *)(ROUND_UP2(((INTPTR_T)free_space_pointer),
						SIZEOF(gtm_int64_t)));
					param_list->arg[j] = free_space_pointer;
					*((gtm_int64_t *)free_space_pointer) = (gtm_int64_t)mval2i8(v);
					free_space_pointer = (gtm_long_t *)((char *)free_space_pointer + SIZEOF(gtm_int64_t));
#					endif
				}
				break;
			case gtm_jfloat:
				param_list->arg[j] = free_space_pointer;
				*((float *)free_space_pointer) = (m1 & 1) ? (float)mval2double(v) : (float)0.0;
				free_space_pointer++;
				break;
			case gtm_jdouble:
#				ifndef GTM64
				/* Only need to do this rounding on non-64 it platforms because this one type has a 64 bit
				 * alignment requirement on those platforms.
				 */
				free_space_pointer = (gtm_long_t *)(ROUND_UP2(((INTPTR_T)free_space_pointer), SIZEOF(double)));
#				endif
				param_list->arg[j] = free_space_pointer;
				*((double *)free_space_pointer) = (m1 & 1) ? (double)mval2double(v) : (double)0.0;
				free_space_pointer = (gtm_long_t *)((char *)free_space_pointer + SIZEOF(double));
				break;
			case gtm_jstring:
			case gtm_jbyte_array:
				param_list->arg[j] = free_space_pointer;
				*free_space_pointer++ = (gtm_long_t)v->str.len;
				*(char **)free_space_pointer = (char *)free_string_pointer;
				free_space_pointer++;
				if (m1 & 1)
				{
					if (v->str.len)
						memcpy(free_string_pointer, v->str.addr, v->str.len);
					free_string_pointer += v->str.len;
					*free_string_pointer++ = 0;
				}
				break;
			default:
				va_end(var_copy);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_UNIMPLOP);
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
	save_mumps_status = mumps_status; 	/* Save mumps_status as a callin from external call may change it. */
	status = callg((callgfnptr)entry_ptr->fcn, param_list);
	mumps_status = save_mumps_status;
	/* The first byte of the type description argument gets set to 0xFF in case error happened in JNI glue code,
	 * so check for that and act accordingly.
	 */
	if ((char)0xFF == *(char *)param_list->arg[0])
	{
		error_in_xc = TRUE;
		jni_err_buf = *(char **)((char *)param_list->arg[0] + SIZEOF(char *));
		if (NULL != jni_err_buf)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_JNI, 2, LEN_AND_STR(jni_err_buf));
		else
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZCSTATUSRET);
	}
	free(types_descr_ptr);
	/* Exit from the residual call-in environment(SFF_CI and base frames) which might still exist on M stack when the externally
	 * called function in turn called into an M routine.
	 */
	if (frame_pointer->flags & SFF_CI)
		ci_ret_code_quit();
	/* Only process the input-output and output-only arguments if the external call succeeded; otherwise, return -1
	 * if non-void return is expected. */
	if (!error_in_xc)
	{	/* Compute space requirement for return values. */
		n = 0;
		VAR_COPY(var_copy, var);
		for (i = 0, j = 1, m1 = mask & entry_ptr->output_mask; i < argcnt; j++)
		{
			v = va_arg(var, mval *);
			if (j < 3)
				continue;
			if (m1 & 1)
				n += extarg_getsize(param_list->arg[j], entry_ptr->parms[i], v);
			i++;
			m1 = m1 >> 1;
		}
		va_end(var);
		if (dst)
			n += extarg_getsize((void *)&status, gtm_status, dst);
		ENSURE_STP_FREE_SPACE(n);
		/* Convert return values. */
		VAR_COPY(var, var_copy);
		for (i = 0, j = 1, m1 = mask & entry_ptr->output_mask; i < argcnt; j++)
		{
			v = va_arg(var_copy, mval *);
			if (j < 3)
				continue;
			if (m1 & 1)
				extarg2mval((void *)param_list->arg[j], entry_ptr->parms[i], v, TRUE, TRUE);
			i++;
			m1 = m1 >> 1;
		}
		va_end(var);
		if (dst)
		{
			if (entry_ptr->return_type != gtm_void)
				extarg2mval((void *)status, entry_ptr->return_type, dst, TRUE, FALSE);
			else
			{
				memcpy(str_buffer, PACKAGE_ENV_PREFIX, SIZEOF(PACKAGE_ENV_PREFIX));
				tmp_buff_ptr = &str_buffer[SIZEOF(PACKAGE_ENV_PREFIX) - 1];
				if (package->str.len)
				{
					assert(package->str.len < MAX_NAME_LENGTH - SIZEOF(PACKAGE_ENV_PREFIX) - 1);
					*tmp_buff_ptr++ = '_';
					memcpy(tmp_buff_ptr, package->str.addr, package->str.len);
					tmp_buff_ptr += package->str.len;
				}
				*tmp_buff_ptr = 0;
				xtrnl_table_name = GETENV(str_buffer);
				if (NULL == xtrnl_table_name)
				{ 	/* Environment variable for the package not found. This part of code is for more safety.
					 * We should not come into this path at all.
					 */
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_ZCCTENV, 2, LEN_AND_STR(str_buffer));
				}
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_XCVOIDRET, 4,
					  LEN_AND_STR(entry_ptr->call_name.addr), LEN_AND_STR(xtrnl_table_name));
			}
		}
	} else if (dst && (gtm_void != entry_ptr->return_type))
		i2mval(dst, -1);
	free(param_list);
	check_for_timer_pops();
	return;
}

void op_fnfgncal (uint4 n_mvals, mval *dst, mval *package, mval *extref, uint4 mask, int4 argcnt, ...)
{
	boolean_t	java = FALSE;
	char		*free_string_pointer, *free_string_pointer_start;
	char		str_buffer[MAX_NAME_LENGTH], *tmp_buff_ptr, *xtrnl_table_name;
	int		i, pre_alloc_size, rslt, save_mumps_status;
	int4 		callintogtm_vectorindex, n;
	gparam_list	*param_list;
	gtm_long_t	*free_space_pointer;
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
	if ((NULL == entry_ptr) || (NULL == entry_ptr->fcn))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_ZCRTENOTF, 2, extref->str.len, extref->str.addr);
	/* Detect a call-out to Java. */
	if ((NULL != entry_ptr->call_name.addr) && !strncmp(entry_ptr->call_name.addr, "gtm_xcj", 7))
	{
		java = TRUE;
		argcnt -= 2;
	}
	/* It is an error to have more actual parameters than formal parameters */
	if (argcnt > entry_ptr->argcnt)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_ZCARGMSMTCH, 2, argcnt, entry_ptr->argcnt);
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
		/* If it is an input value of char* or char **, add the length. Also a good time to force it into string form */
		switch(entry_ptr->parms[i])
		{
			case gtm_char_star:
				n += (-1 != entry_ptr->param_pre_alloc_size[i]) ? entry_ptr->param_pre_alloc_size[i] : 0;
				/* Caution fall through */
			case gtm_char_starstar:
				if (m1 & 1)
				{
					MV_FORCE_STR(v);
					n += v->str.len + 1;
				}
				break;
			case gtm_string_star:
				if (m1 & 1)
				{
					MV_FORCE_STR(v);
					n += v->str.len + 1;
				} else
					n += (-1 != entry_ptr->param_pre_alloc_size[i]) ? entry_ptr->param_pre_alloc_size[i] : 0;
				break;
			case gtm_double_star:
				n += SIZEOF(double);
				if (m1 & 1)
					MV_FORCE_DEFINED(v);
				break;
			default:
				if (m1 & 1)
					MV_FORCE_DEFINED(v);
				break;
		}
	}
	va_end(var);
        /* Double the size, to take care of any alignments in the middle  */
	param_list = (gparam_list *)malloc(n * 2);
	free_space_pointer = (gtm_long_t *)((char *)param_list + SIZEOF(intszofptr_t) + (SIZEOF(void *) * argcnt));
	free_string_pointer_start = free_string_pointer = (char *)param_list + entry_ptr->parmblk_size;
	/* Load-up the parameter list */
	VAR_START(var, argcnt);
	for (i = 0, m1 = entry_ptr->input_mask; i < argcnt; i++, m1 = m1 >> 1)
	{
		v = va_arg(var, mval *);
		/* Note that even in this second pass at these mvals, we need to do the MV_FORCE processing because
		 * in a NOUNDEF environment, undefiend mvals were NOT modified in the first pass and thus reamin undefined
		 * in this pass.
		 */
		if (gtm_char_star != entry_ptr->parms[i] && (m1 & 1))
			MV_FORCE_DEFINED(v);	/* Redefine undef'd mvals */
		/* Verify that all input values are defined */
		pre_alloc_size = entry_ptr->param_pre_alloc_size[i];
		switch(entry_ptr->parms[i])
		{	/* Note the int/long types are handled separately here in anticipation of correct handling
			 * of "long" types on 64 bit hardware in the future. For the time being however, they are
			 * using the same mval2i interface routine so are both restricted to 32 bits.
			 */
			case gtm_uint:
				if (m1 & 1)
					param_list->arg[i] = (void *)(gtm_long_t)mval2ui(v);
				/* Note: output gtm_int and gtm_uint is an error (only "star" flavor can be modified) */
				break;
			case gtm_int:
				if (m1 & 1)
					param_list->arg[i] = (void *)(gtm_long_t)mval2i(v);
				break;
			case gtm_ulong:
				if (m1 & 1)
				{
					GTM64_ONLY(param_list->arg[i] = (void *)(gtm_uint64_t)mval2ui8(v));
					NON_GTM64_ONLY(param_list->arg[i] = (void *)(gtm_ulong_t)mval2ui(v));
				}
				/* Note: output xc_long and xc_ulong is an error as described above */
				break;
			case gtm_long:
				if (m1 & 1)
				{
					GTM64_ONLY(param_list->arg[i] = (void *)(gtm_int64_t)mval2i8(v));
					NON_GTM64_ONLY(param_list->arg[i] = (void *)(gtm_long_t)mval2i(v));
				}
				break;
			case gtm_char_star:
				param_list->arg[i] = free_string_pointer;
				if (m1 & 1)
				{
					if (v->str.len)
						memcpy(free_string_pointer, v->str.addr, v->str.len);
					free_string_pointer += v->str.len;
					*free_string_pointer++ = 0;
				} else if (-1 != pre_alloc_size)
					free_string_pointer += pre_alloc_size;
				else /* Output and no pre-allocation specified */
				{
					if (0 == package->str.len)
						/* Default package - do not display package name */
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_ZCNOPREALLOUTPAR, 5, i + 1,
							RTS_ERROR_LITERAL("<DEFAULT>"), extref->str.len, extref->str.addr);
					else
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_ZCNOPREALLOUTPAR, 5, i + 1,
							package->str.len, package->str.addr, extref->str.len, extref->str.addr);
				}
				break;
			case gtm_char_starstar:
				param_list->arg[i] = free_space_pointer;
				if (m1 & 1)
				{
					*(char **)free_space_pointer = free_string_pointer;
					if (v->str.len)
						memcpy(free_string_pointer, v->str.addr, v->str.len);
					free_string_pointer += v->str.len;
					*free_string_pointer++ = 0;
				} else
					*(char **)free_space_pointer = free_string_pointer++;
				free_space_pointer++;
				break;
			case gtm_int_star:
				param_list->arg[i] = free_space_pointer;
				*((gtm_int_t *)free_space_pointer) = (m1 & 1) ? (gtm_int_t)mval2i(v) : 0;
				free_space_pointer++;
				break;
			case gtm_uint_star:
				param_list->arg[i] = free_space_pointer;
				*((gtm_uint_t *)free_space_pointer) = (m1 & 1) ? (gtm_uint_t)mval2ui(v) : 0;
				free_space_pointer++;
				break;
			case gtm_long_star:
				param_list->arg[i] = free_space_pointer;
				GTM64_ONLY(*((gtm_int64_t *)free_space_pointer) = (m1 & 1) ? (gtm_int64_t)mval2i8(v) : 0);
				NON_GTM64_ONLY(*((gtm_long_t *)free_space_pointer) = (m1 & 1) ? (gtm_long_t)mval2i(v) : 0);
				free_space_pointer++;
				break;
			case gtm_ulong_star:
				param_list->arg[i] = free_space_pointer;
				GTM64_ONLY(*((gtm_uint64_t *)free_space_pointer) = (m1 & 1) ? (gtm_uint64_t)mval2ui8(v) : 0);
				NON_GTM64_ONLY(*((gtm_ulong_t *)free_space_pointer) = (m1 & 1) ? (gtm_ulong_t)mval2ui(v) : 0);
				free_space_pointer++;
				break;
			case gtm_string_star:
				param_list->arg[i] = free_space_pointer;
				*free_space_pointer++ = (gtm_long_t)v->str.len;
				*(char **)free_space_pointer = (char *)free_string_pointer;
				free_space_pointer++;
				if (m1 & 1)
				{
					if (v->str.len)
						memcpy(free_string_pointer, v->str.addr, v->str.len);
					free_string_pointer += v->str.len;
				} else if (-1 != pre_alloc_size)
					free_string_pointer += pre_alloc_size;
				else /* Output and no pre-allocation specified */
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
			case gtm_float_star:
				param_list->arg[i] = free_space_pointer;
				*((float *)free_space_pointer) = (m1 & 1) ? (float)mval2double(v) : (float)0.0;
				free_space_pointer++;
				break;
			case gtm_double_star:
				/* Only need to do this rounding on non-64 it platforms because this one type has a 64 bit
				 * alignment requirement on those platforms.
				 */
				NON_GTM64_ONLY(free_space_pointer = (gtm_long_t *)(ROUND_UP2(((INTPTR_T)free_space_pointer),
											    SIZEOF(double))));
				param_list->arg[i] = free_space_pointer;
				*((double *)free_space_pointer) = (m1 & 1) ? (double)mval2double(v) : (double)0.0;
				free_space_pointer = (gtm_long_t *)((char *)free_space_pointer + SIZEOF(double));
				break;
			case gtm_pointertofunc:
				if (((callintogtm_vectorindex = (int4)mval2i(v)) >= gtmfunc_unknown_function)
				    || (callintogtm_vectorindex < 0))
				{
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_ZCVECTORINDX, 1, callintogtm_vectorindex,
						ERR_TEXT, 2, RTS_ERROR_TEXT("Passing Null vector"));
					param_list->arg[i] = 0;
				} else
					param_list->arg[i] = (void *)callintogtm_vectortable[callintogtm_vectorindex];
				break;
			case gtm_pointertofunc_star:
				/* Cannot pass in a function address to be modified by the user program */
				free_space_pointer = (gtm_long_t *)ROUND_UP2(((INTPTR_T)free_space_pointer), SIZEOF(INTPTR_T));
				param_list->arg[i] = free_space_pointer;
				*((INTPTR_T *)free_space_pointer) = 0;
				free_space_pointer++;
				break;
			default:
				va_end(var);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_UNIMPLOP);
				break;
		}
	}
	assert((char *)free_space_pointer <= free_string_pointer_start);
	va_end(var);
	param_list->n = argcnt;
	save_mumps_status = mumps_status; /* Save mumps_status as a callin from external call may change it */
	status = callg((callgfnptr)entry_ptr->fcn, param_list);
	mumps_status = save_mumps_status;

	/* Exit from the residual call-in environment(SFF_CI and base frames) which might
	 * still exist on M stack when the externally called function in turn called
	 * into an M routine.
	 */
	if (frame_pointer->flags & SFF_CI)
		ci_ret_code_quit();
	/* NOTE: ADD RETURN STATUS CALCUATIONS HERE */
	/* Compute space requirement for return values */
	n = 0;
	VAR_START(var, argcnt);
	for (i = 0, m1 = mask & entry_ptr->output_mask; i < argcnt; i++, m1 = m1 >> 1)
	{
		v = va_arg(var, mval *);
		if (m1 & 1)
			n += extarg_getsize(param_list->arg[i], entry_ptr->parms[i], v);
	}
	va_end(var);
	if (dst)
		n += extarg_getsize((void *)&status, gtm_status, dst);
	ENSURE_STP_FREE_SPACE(n);
	/* Convert return values */
	VAR_START(var, argcnt);
	for (i = 0, m1 = mask & entry_ptr->output_mask; i < argcnt; i++, m1 = m1 >> 1)
	{
		v = va_arg(var, mval *);
		if (m1 & 1)
			extarg2mval((void *)param_list->arg[i], entry_ptr->parms[i], v, FALSE, TRUE);
	}
	va_end(var);
	if (dst)
	{
		if (entry_ptr->return_type != gtm_void)
			extarg2mval((void *)status, entry_ptr->return_type, dst, FALSE, FALSE);
		else
		{
			memcpy(str_buffer, PACKAGE_ENV_PREFIX, SIZEOF(PACKAGE_ENV_PREFIX));
			tmp_buff_ptr = &str_buffer[SIZEOF(PACKAGE_ENV_PREFIX) - 1];
			if (package->str.len)
			{
				assert(package->str.len < MAX_NAME_LENGTH - SIZEOF(PACKAGE_ENV_PREFIX) - 1);
				*tmp_buff_ptr++ = '_';
				memcpy(tmp_buff_ptr, package->str.addr, package->str.len);
				tmp_buff_ptr += package->str.len;
			}
			*tmp_buff_ptr = 0;
			xtrnl_table_name = GETENV(str_buffer);
			if (NULL == xtrnl_table_name)
			{
				/* Environment variable for the package not found.
				 * This part of code is for more safety. We should
				 * not come into this path at all.
				 */
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_ZCCTENV, 2, LEN_AND_STR(str_buffer));
			}
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_XCVOIDRET, 4,
				  LEN_AND_STR(entry_ptr->call_name.addr), LEN_AND_STR(xtrnl_table_name));
		}
	}
	free(param_list);
	check_for_timer_pops();
	return;
}
