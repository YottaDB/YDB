/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#ifdef EARLY_VARARGS
#include <varargs.h>
#endif
#include "gtm_stdio.h"
#ifndef EARLY_VARARGS
#include <varargs.h>
#endif
#include <errno.h>
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "stringpool.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "mvalconv.h"
#include "gtmxc_types.h"
#include "fgncal.h"
#include "gtmci.h"
#include "error.h"
#include "startup.h"
#include "mv_stent.h"
#include "gtmci_signals.h"
#include "gtm_startup.h"
#include "job_addr.h"
#include "invocation_mode.h"

GBLDEF	parmblk_struct 		*param_list;
GBLREF  stack_frame     	*frame_pointer;
GBLREF  mv_stent         	*mv_chain;
GBLREF	int			mumps_status;
GBLREF 	void			(*restart)();
GBLREF 	boolean_t		gtm_startup_active;
static  callin_entry_list	*ci_table = 0;

static callin_entry_list* get_entry(const char* call_name)
{
	callin_entry_list*	entry;
	entry = ci_table;
	while (entry && memcmp(call_name, entry->call_name.addr, entry->call_name.len))
		entry = entry->next_entry;
	return entry;
}

int gtm_ci (c_rtn_name, va_alist)
const char* c_rtn_name;
va_dcl
{
	va_list			var, save_var;
	callin_entry_list	*entry;
	mstr			label, routine;
	int			has_return, i;
	rhdtyp          	*base_addr;
	unsigned char   	*transfer_addr;
	uint4			inp_mask, out_mask, mask, label_offset;
	mval			arg_mval, *arg_ptr;
	enum xc_types		arg_type;
	gtm_string_t		*mstr_parm;
	parmblk_struct 		param_blk;
	void 			op_extcall(), op_extexfun(), flush_pio(void);

	error_def(ERR_CIRCALLNAME);
	error_def(ERR_CINOENTRY);

	/* set up start-up environment if not done already */
	if (!gtm_startup_active)
	{
		err_init(stop_image_conditional_core);
		invocation_mode = MUMPS_CALLIN;
		ESTABLISH_RET(gtmci_init_ch, -1);
		init_gtm();
		assert(gtm_startup_active);
		SET_CI_ENV(ci_ret_code_exit);
	} else {
		ESTABLISH_RET(gtmci_init_ch, -1);
		sig_switch_gtm(); /* save external signal context */
	}
	if (!c_rtn_name)
		rts_error(VARLSTCNT(1) ERR_CIRCALLNAME);

	/* load the call-in table only once from env variable GTMCI  */
	if (!ci_table)
		ci_table = citab_parse();
	if (!(entry = get_entry(c_rtn_name)))	/* c_rtn_name not found in the table */
		rts_error(VARLSTCNT(4) ERR_CINOENTRY, 2, LEN_AND_STR(c_rtn_name));
	lref_parse((uchar_ptr_t)entry->label_ref.addr, &routine, &label, &i);
	job_addr(&routine, &label, 0, (char**)&base_addr, (char**)&transfer_addr);
	if (!(frame_pointer->flags & SFF_CI))
	{ /* setup a CI environment (CI-frame on top of base-frame) */
		gtm_init_env(base_addr, transfer_addr);
		SET_CI_ENV(ci_ret_code_exit);
		save_intrinsic_var();
	}
	invocation_mode |= MUMPS_GTMCI;
	memset(&param_blk, sizeof(param_blk), 0);
	param_blk.rtnaddr = (void*)base_addr;
	label_offset = (int4)((char*)transfer_addr - (char*)base_addr);
	param_blk.labaddr = &label_offset;
	param_blk.argcnt = entry->argcnt;

	VAR_START(var);
	VAR_COPY(save_var, var);
	has_return = (xc_void == entry->return_type) ? 0 : 1;
	if (has_return)
	{ /* create mval slot for return value */
		param_blk.retaddr = (void*)push_mval(&arg_mval);
		va_arg(var, void*);	/* advance va_arg */
	}
	else
		param_blk.retaddr = 0;
	inp_mask = entry->input_mask;
	out_mask = entry->output_mask;
	for (i=0, mask = ~inp_mask; i < entry->argcnt; ++i, mask>>=1)
	{ /* pass by value - copy argument values.
	     Since only first MAXIMUM_PARAMETERS could be O/IO, any additional
	     params will be treated as Input-only (I). inp_mask is inversed to achieve this.
	   */
		arg_mval.mvtype = MV_XZERO;
		switch (entry->parms[i])
		{
			case xc_long: 	{
				gtm_long_t arg_val = va_arg(var, gtm_long_t);
				if (!(mask & 1))
					i2mval(&arg_mval, arg_val);
				break;	}
			case xc_ulong: 	{
				gtm_ulong_t arg_val = va_arg(var, gtm_ulong_t);
				if (!(mask & 1))
					i2usmval(&arg_mval, arg_val);
				break;	}
			case xc_long_star:	{
				gtm_long_t* arg_val = va_arg(var, gtm_long_t*);
				if (!(mask & 1))
					i2mval(&arg_mval, *arg_val);
				break;		}
			case xc_ulong_star:	{
				gtm_ulong_t* arg_val = va_arg(var, gtm_ulong_t*);
				if (!(mask & 1))
					i2usmval(&arg_mval, *arg_val);
				break;		}
			case xc_float:	{
				gtm_float_t arg_val = va_arg(var, gtm_float_t);
				if (!(mask & 1))
					double2mval(arg_val, &arg_mval);
				break;	}
			case xc_double:	{
				gtm_double_t arg_val = va_arg(var, gtm_double_t);
				if (!(mask & 1))
					double2mval(arg_val, &arg_mval);
				break;	}
			case xc_float_star:	{
				gtm_float_t* arg_val = va_arg(var, gtm_float_t*);
				if (!(mask & 1))
					double2mval(*arg_val, &arg_mval);
				break;		}
			case xc_double_star:	{
				gtm_double_t* arg_val = va_arg(var, gtm_double_t*);
				if (!(mask & 1))
					double2mval(*arg_val, &arg_mval);
				break;		}
			case xc_char_star:
				arg_mval.str.addr = va_arg(var, gtm_char_t*);
				if (!(mask & 1))
				{
					arg_mval.mvtype = MV_STR;
					arg_mval.str.len = strlen(arg_mval.str.addr);
					s2pool(&arg_mval.str);
				}
				break;
			case xc_string_star:
				mstr_parm = va_arg(var, gtm_string_t*);
				if (!(mask & 1))
				{
					arg_mval.mvtype = MV_STR;
					arg_mval.str.addr = mstr_parm->address;
					arg_mval.str.len = mstr_parm->length;
					s2pool(&arg_mval.str);
				}
				break;
			default:
				GTMASSERT; /* should have been caught by citab_parse */
		}
		param_blk.args[i] = push_mval(&arg_mval);
	}
	param_blk.mask = out_mask;
	param_blk.ci_rtn = (!has_return && param_blk.argcnt <= 0) ? &op_extcall : &op_extexfun;
	/* the params block needs to be stored & restored across multiple
	   gtm environments. So instead of storing explicitely, setting the
	   global param_list to point to local param_blk will do the job */
	param_list = &param_blk;
	REVERT;

	ESTABLISH_RET(stop_image_conditional_core, -1);
	ci_start(); /* kick off execution */
	REVERT;

	ESTABLISH_RET(gtmci_ch, -1);
	VAR_COPY(var, save_var);
	/* convert mval args passed by reference to C types */
	for (i=0; i <= entry->argcnt; ++i)
	{
		if (0 == i) /* special case for return value */
		{
			if (!has_return)
				continue;
			arg_ptr = (mval*)(param_blk.retaddr);
			mask = 1;
			arg_type = entry->return_type;
		}
		else
		{
			arg_ptr = param_blk.args[i-1];
			mask = out_mask;
			arg_type = entry->parms[i-1];
			out_mask >>= 1;
		}
		switch (arg_type)
		{
			case xc_long_star:	{
				gtm_long_t* ptr = va_arg(var, gtm_long_t*);
				if (mask & 1)
					*ptr = mval2i(arg_ptr);
				break;		}
			case xc_ulong_star:	{
				gtm_ulong_t* ptr = va_arg(var, gtm_ulong_t*);
				if (mask & 1)
					*ptr = mval2si(arg_ptr);
				break;		}
			case xc_float_star:	{
				gtm_float_t* ptr = va_arg(var, gtm_float_t*);
				if (mask & 1)
					*ptr = mval2double(arg_ptr);
				break;		}
			case xc_double_star:	{
				gtm_double_t* ptr = va_arg(var, gtm_double_t*);
				if (mask & 1)
					*ptr = mval2double(arg_ptr);
				break;		}
			case xc_char_star: 	{
				gtm_char_t* ptr = va_arg(var, gtm_char_t*);
				if (mask & 1)
				{
					if (!MV_IS_STRING(arg_ptr))
						MV_FORCE_STR(arg_ptr);
					memcpy(ptr, arg_ptr->str.addr, arg_ptr->str.len);
					ptr[arg_ptr->str.len] = 0; /* trailing null */
				}
				break;		}
			case xc_string_star:
				mstr_parm = va_arg(var, gtm_string_t*);
				if (mask & 1)
				{
					if (!MV_IS_STRING(arg_ptr));
						MV_FORCE_STR(arg_ptr);
					mstr_parm->length = arg_ptr->str.len;
					memcpy(mstr_parm->address, arg_ptr->str.addr, mstr_parm->length);
				}
				break;
				/* following cases to advance va_arg */
			case xc_long:
				va_arg(var, gtm_long_t);
				break;
			case xc_ulong:
				va_arg(var, gtm_ulong_t);
				break;
			case xc_float:
				va_arg(var, gtm_float_t);
				break;
			case xc_double:
				va_arg(var, gtm_double_t);
				break;
			default:
				GTMASSERT;
		}
	}
	invocation_mode &= MUMPS_GTMCI_OFF;
	unw_mv_ent_n(has_return ? (entry->argcnt + 1) : entry->argcnt);
	flush_pio();
	sig_switch_ext();
	REVERT;
	return mumps_status;
}
