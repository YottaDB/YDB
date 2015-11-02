/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
 *	op_fnfgncal(8, 0, mval*("package"), mval*("bessel"), 4, 3, mval*(p1), mval*(p2), mval*(p3))
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

STATICDEF int			call_table_initialized = 0;

STATICFNDCL void extarg2mval(void *src, enum xc_types typ, mval *dst);
STATICFNDCL int extarg_getsize(void *src, enum xc_types typ, mval *dst);

/* routine to convert external return values to mval's */
STATICFNDEF void extarg2mval(void *src, enum xc_types typ, mval *dst)
{
	xc_long_t		str_len;
	xc_int_t		s_int_num;
	xc_uint_t		uns_int_num;
	xc_long_t		s_long_num;
	xc_ulong_t		uns_long_num;
	char			*cp;
	struct extcall_string	*sp;

	switch(typ)
	{
		case xc_notfound:
			break;
		case xc_void:
			break;
		case xc_status:
			/* Note: reason for double cast is to first turn ptr to same sized int, then big int to little int
			   (on 64 bit platforms). This avoids a warning msg with newer 64 bit gcc compilers */
			s_int_num = (xc_int_t)(intszofptr_t)src;
			if (0 != s_int_num)
				dec_err(VARLSTCNT(1) ERR_ZCSTATUSRET, 0, s_int_num);
			MV_FORCE_MVAL(dst, s_int_num);
			break;
		case xc_int:
			s_int_num = (xc_int_t)(intszofptr_t)src;
			MV_FORCE_MVAL(dst, s_int_num);
			break;
		case xc_uint:
			uns_int_num = (xc_uint_t)(intszofptr_t)src;
			MV_FORCE_UMVAL(dst, uns_int_num);
			break;
		case xc_int_star:
			s_int_num = *((xc_int_t *)src);
			MV_FORCE_MVAL(dst, s_int_num);
			break;
		case xc_uint_star:
			uns_int_num = *((xc_uint_t *)src);
			MV_FORCE_UMVAL(dst, uns_int_num);
			break;
		case xc_long:
			s_long_num = (xc_long_t)src;
			MV_FORCE_LMVAL(dst, s_long_num);
			break;
		case xc_ulong:
			uns_long_num = (xc_ulong_t)src;
			MV_FORCE_ULMVAL(dst, uns_long_num);
			break;
		case xc_long_star:
			s_long_num = *((xc_long_t *)src);
			MV_FORCE_LMVAL(dst, s_long_num);
			break;
		case xc_ulong_star:
			uns_long_num = *((xc_ulong_t *)src);
			MV_FORCE_ULMVAL(dst, uns_long_num);
			break;
		case xc_string_star:
			sp = (struct extcall_string *)src;
			dst->mvtype = MV_STR;
			if (sp->len > MAX_STRLEN)
				rts_error(VARLSTCNT(1) ERR_MAXSTRLEN);
			dst->str.len = (mstr_len_t)sp->len;
			if ((0 < sp->len) && (NULL != sp->addr))
			{
				dst->str.addr = sp->addr;
				s2pool(&dst->str);
			}
			break;
		case xc_float_star:
			double2mval(dst, (double)*((float *)src));
			break;
		case xc_char_star:
			cp = (char *)src;
			assert(((INTPTR_T)cp < (INTPTR_T)stringpool.base) || ((INTPTR_T)cp > (INTPTR_T)stringpool.top));
			dst->mvtype = MV_STR;
			str_len = STRLEN(cp);
			if (str_len > MAX_STRLEN)
				rts_error(VARLSTCNT(1) ERR_MAXSTRLEN);
			dst->str.len = (mstr_len_t)str_len;
			dst->str.addr = cp;
			s2pool(&dst->str);
			break;
		case xc_char_starstar:
			if (!src)
				dst->mvtype = 0;
			else
				extarg2mval(*((char **)src), xc_char_star, dst);
			break;
		case xc_double_star:
			double2mval(dst, *((double *)src));
			break;
		default:
			rts_error(VARLSTCNT(1) ERR_UNIMPLOP);
			break;
	}
	return;
}

/* subroutine to calculate stringpool requirements for an external argument */
STATICFNDEF int extarg_getsize(void *src, enum xc_types typ, mval *dst)
{
	int4			n;
	char			*cp, **cpp;
	struct extcall_string	*sp;

	if (!src)
		return 0;
	switch(typ)
	{
		/* the following group of cases either return nothing or use the numeric part of the mval */
		case xc_notfound:
		case xc_void:
		case xc_double_star:
		case xc_status:
		case xc_int:
		case xc_uint:
		case xc_long:
		case xc_ulong:
		case xc_float_star:
		case xc_int_star:
		case xc_uint_star:
		case xc_long_star:
		case xc_ulong_star:
			return 0;
		case xc_char_starstar:
			cpp = (char **)src;
			if (*cpp)
				return STRLEN(*cpp);
			else
				return 0;
		case xc_char_star:
			cp = (char *)src;
			return STRLEN(cp);
		case xc_string_star:
			sp = (struct extcall_string *)src;
			if ((0 < sp->len)
			    && ((INTPTR_T)sp->addr < (INTPTR_T)stringpool.free)
			    && ((INTPTR_T)sp->addr >= (INTPTR_T)stringpool.base))
			{	/* the stuff is already in the stringpool */
				assert(dst->str.addr == sp->addr);
				dst->str.addr = sp->addr;
				sp->addr = NULL;	/* prevent subsequent s2pool */
				return 0;
			} else  if (NULL == sp->addr)
				sp->len = 0;
			return (int)(sp->len);
			break;
		default:
			rts_error(VARLSTCNT(1) ERR_UNIMPLOP);
			break;
	}

	return 0; /* This should never get executed, added to make compiler happy */
}

void op_fnfgncal (uint4 n_mvals, mval *dst, mval *package, mval *extref, uint4 mask, int4 argcnt, ...)
{
	va_list		var;
	int		i, rslt;
	int4 		callintogtm_vectorindex;
	mval		*arg, *v;
	int4		n;
	xc_long_t	*free_space_pointer;
	uint4		m1;
	INTPTR_T	status;
	char		*cp, *free_string_pointer, *free_string_pointer_start;
	int		pre_alloc_size;
	int		save_mumps_status;
	char		*gtmvectortable_temp, *tmp_buff_ptr, str_buffer[MAX_NAME_LENGTH];
	char		*xtrnl_table_name;
	struct extcall_package_list	*package_ptr;
	struct extcall_entry_list	*entry_ptr;
	gparam_list	*param_list;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(n_mvals == argcnt + 5);
	assert(MV_IS_STRING(package));	/* package and routine are literal strings */
	assert(MV_IS_STRING(extref));
	if (MAX_ACTUALS < argcnt)
		rts_error(VARLSTCNT(1) ERR_ZCMAXPARAM);
	assert(INTRPT_OK_TO_INTERRUPT == intrpt_ok_state); /* interrupts should be enabled for external calls */
	/* find package */
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
		rts_error(VARLSTCNT(1) errno);
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
	/* entry not found */
	if ((NULL == entry_ptr) || (NULL == entry_ptr->fcn))
		rts_error(VARLSTCNT(4) ERR_ZCRTENOTF, 2, extref->str.len, extref->str.addr);
	/* It is an error to have more actual parameters than formal parameters */
	if (argcnt > entry_ptr->argcnt)
		rts_error(VARLSTCNT(4) ERR_ZCARGMSMTCH, 2, argcnt, entry_ptr->argcnt);
	VAR_START(var, argcnt);
	/* compute size of parameter block */
	n = entry_ptr->parmblk_size;	/* This is enough for the parameters and the fixed length entries */
	/* Now, add enough for the char *'s and the char **'s and string *'s */
	for (i = 0, m1 = entry_ptr->input_mask; i < argcnt; i++, m1 = m1 >> 1)
	{
		v = va_arg(var, mval *);
		/* if it is an input value of char* or char **, add the length */
		/* also a good time to force it into string form */
		switch(entry_ptr->parms[i])
		{
			case xc_char_star:
				n += (-1 != entry_ptr->param_pre_alloc_size[i]) ? entry_ptr->param_pre_alloc_size[i] : 0;
				/* Caution fall through */
			case xc_char_starstar:
				if (m1 & 1)
				{
					MV_FORCE_STR(v);
					n += v->str.len + 1;
				}
				break;
			case xc_string_star:
				if (m1 & 1)
				{
					MV_FORCE_STR(v);
					n += v->str.len + 1;
				} else
					n += (-1 != entry_ptr->param_pre_alloc_size[i]) ? entry_ptr->param_pre_alloc_size[i] : 0;
				break;
			case xc_double_star:
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
	free_space_pointer = (xc_long_t *)((char *)param_list + SIZEOF(intszofptr_t) + (SIZEOF(void *) * argcnt));
	free_string_pointer_start = free_string_pointer = (char *)param_list + entry_ptr->parmblk_size;
	/* load-up the parameter list */
	VAR_START(var, argcnt);
	for (i = 0, m1 = entry_ptr->input_mask; i < argcnt; i++, m1 = m1 >> 1)
	{
		v = va_arg(var, mval *);
		/* Note that even in this second pass at these mvals, we need to do the MV_FORCE processing because
		   in a NOUNDEF environment, undefiend mvals were NOT modified in the first pass and thus reamin undefined
		   in this pass.
		*/
		if (xc_char_star != entry_ptr->parms[i] && (m1 & 1))
			MV_FORCE_DEFINED(v);	/* Redefine undef'd mvals */
		/* Verify that all input values are defined */
		pre_alloc_size = entry_ptr->param_pre_alloc_size[i];
		switch(entry_ptr->parms[i])
		{	/* Note the int/long types are handled separately here in anticipation of correct handling
			   of "long" types on 64 bit hardware in the future. For the time being however, they are
			   using the same mval2i interface routine so are both restricted to 32 bits.
			*/
			case xc_uint:
				if (m1 & 1)
					param_list->arg[i] = (void *)(xc_long_t)mval2ui(v);
				/* Note: output xc_int and xc_uint is an error (only "star" flavor can be modified) */
				break;
			case xc_int:
				if (m1 & 1)
					param_list->arg[i] = (void *)(xc_long_t)mval2i(v);
				break;
			case xc_ulong:
				if (m1 & 1)
					param_list->arg[i] = (void *)(xc_long_t)mval2ui(v);
				/* Note: output xc_long and xc_ulong is an error as described above */
				break;
			case xc_long:
				if (m1 & 1)
					param_list->arg[i] = (void *)(xc_ulong_t)mval2i(v);
				break;
			case xc_char_star:
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
						/* default package - do not display package name */
						rts_error(VARLSTCNT(7) ERR_ZCNOPREALLOUTPAR, 5, i+1, RTS_ERROR_LITERAL("<DEFAULT>"),
							  extref->str.len, extref->str.addr);
					else
						rts_error(VARLSTCNT(7) ERR_ZCNOPREALLOUTPAR, 5, i+1, package->str.len,
							  package->str.addr, extref->str.len, extref->str.addr);
				}
				break;
			case xc_char_starstar:
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
			case xc_int_star:
				param_list->arg[i] = free_space_pointer;
				*((xc_int_t *)free_space_pointer) = (m1 & 1) ? (xc_int_t)mval2i(v) : 0;
				free_space_pointer++;
				break;
			case xc_uint_star:
				param_list->arg[i] = free_space_pointer;
				*((xc_uint_t *)free_space_pointer) = (m1 & 1) ? (xc_uint_t)mval2ui(v) : 0;
				free_space_pointer++;
				break;
			case xc_long_star:
				param_list->arg[i] = free_space_pointer;
				*((xc_long_t *)free_space_pointer) = (m1 & 1) ? (xc_long_t)mval2i(v) : 0;
				free_space_pointer++;
				break;
			case xc_ulong_star:
				param_list->arg[i] = free_space_pointer;
				*((xc_ulong_t *)free_space_pointer) = (m1 & 1) ? (xc_ulong_t)mval2ui(v) : 0;
				free_space_pointer++;
				break;
			case xc_string_star:
				param_list->arg[i] = free_space_pointer;
				*free_space_pointer++ = (xc_long_t)v->str.len;
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
						/* default package - do not display package name */
						rts_error(VARLSTCNT(7) ERR_ZCNOPREALLOUTPAR, 5, i + 1,
							  RTS_ERROR_LITERAL("<DEFAULT>"),
							  extref->str.len, extref->str.addr);
					else
						rts_error(VARLSTCNT(7) ERR_ZCNOPREALLOUTPAR, 5, i + 1,
							  package->str.len, package->str.addr,
							  extref->str.len, extref->str.addr);
				}
				break;
			case xc_float_star:
				param_list->arg[i] = free_space_pointer;
				*((float *)free_space_pointer) = (m1 & 1) ? (float)mval2double(v) : (float)0.0;
				free_space_pointer++;
				break;
			case xc_double_star:
				/* Only need to do this rounding on non-64 it platforms because this one type has a 64 bit
				   alignment requirement on those platforms.
				*/
				NON_GTM64_ONLY(free_space_pointer = (xc_long_t *)(ROUND_UP2(((INTPTR_T)free_space_pointer),
											    SIZEOF(double))));
				param_list->arg[i] = free_space_pointer;
				*((double *)free_space_pointer) = (m1 & 1) ? (double)mval2double(v) : (double)0.0;
				free_space_pointer = (xc_long_t *)((char *)free_space_pointer + SIZEOF(double));
				break;
			case xc_pointertofunc:
				if (((callintogtm_vectorindex = (int4)mval2i(v)) >= xc_unknown_function)
				    || (callintogtm_vectorindex < 0))
				{
					rts_error(VARLSTCNT(7) ERR_ZCVECTORINDX, 1, callintogtm_vectorindex, ERR_TEXT, 2,
						  RTS_ERROR_TEXT("Passing Null vector"));
					param_list->arg[i] = 0;
				} else
					param_list->arg[i] = (void *)callintogtm_vectortable[callintogtm_vectorindex];
				break;
			case xc_pointertofunc_star:
				/* cannot pass in a function address to be modified by the user program */
				free_space_pointer = (xc_long_t *)ROUND_UP2(((INTPTR_T)free_space_pointer), SIZEOF(INTPTR_T));
				param_list->arg[i] = free_space_pointer;
				*((INTPTR_T *)free_space_pointer) = 0;
				free_space_pointer++;
				break;
			default:
				va_end(var);
				rts_error(VARLSTCNT(1) ERR_UNIMPLOP);
				break;
		}
	}
	assert((char *)free_space_pointer <= free_string_pointer_start);
	va_end(var);
	param_list->n = argcnt;
	save_mumps_status = mumps_status; /* save mumps_status as a callin from external call may change it */
	status = callg((callgfnptr)entry_ptr->fcn, param_list);
	mumps_status = save_mumps_status;

	/* Exit from the residual call-in environment(SFF_CI and base frames) which might
	 * still exist on M stack when the externally called function in turn called
	 * into an M routine */
	if (frame_pointer->flags & SFF_CI)
		ci_ret_code_quit();

	/* NOTE: ADD RETURN STATUS CALCUATIONS HERE */
	/* compute space requirement for return values */
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
		n += extarg_getsize((void *)&status, xc_status, dst);
	ENSURE_STP_FREE_SPACE(n);
	/* convert return values */
	VAR_START(var, argcnt);
	for (i = 0, m1 = mask & entry_ptr->output_mask; i < argcnt; i++, m1 = m1 >> 1)
	{
		v = va_arg(var, mval *);
		if (m1 & 1)
			extarg2mval((void *)param_list->arg[i], entry_ptr->parms[i], v);
	}
	va_end(var);
	if (dst)
	{
		if (entry_ptr->return_type != xc_void)
			extarg2mval((void *)status, entry_ptr->return_type, dst);
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
				rts_error(VARLSTCNT(4) ERR_ZCCTENV, 2, LEN_AND_STR(str_buffer));
			}
			rts_error(VARLSTCNT(6) ERR_XCVOIDRET, 4,
				  LEN_AND_STR(entry_ptr->call_name.addr), LEN_AND_STR(xtrnl_table_name));
		}
	}
	free(param_list);
	check_for_timer_pops();
	return;
}
