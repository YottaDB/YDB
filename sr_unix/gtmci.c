/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>
#include "gtm_stdio.h"
#include <errno.h>
#include "gtm_stdlib.h"
#include "gtm_string.h"

#include "cli.h"
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
#include "op.h"
#include "gtm_startup.h"
#include "job_addr.h"
#include "invocation_mode.h"
#include "gtmimagename.h"
#include "gtm_exit_handler.h"
#include "gtm_savetraps.h"
#include "gtm_env_init.h"	/* for gtm_env_init() prototype */

#ifdef UNICODE_SUPPORTED
#include "gtm_icu_api.h"
#include "gtm_utf8.h"
#endif

GBLREF	parmblk_struct 		*param_list;
GBLREF  stack_frame     	*frame_pointer;
GBLREF  unsigned char		*msp;
GBLREF  mv_stent         	*mv_chain;
GBLREF	int			mumps_status;
GBLREF 	void			(*restart)();
GBLREF 	boolean_t		gtm_startup_active;
GBLREF	int * volatile		var_on_cstack_ptr;	/* volatile so that nothing gets optimized out */
GBLREF	rhdtyp			*ci_base_addr;
GBLDEF 	unsigned int		nested_level;		/* current nested depth of callin environments */

GBLREF  mval			dollar_zstatus;
GBLREF  unsigned char		*fgncal_stack;
GBLREF  short			dollar_tlevel;
GBLREF  enum gtmImageTypes	image_type;
GBLREF  boolean_t		gtm_utf8_mode;

static  callin_entry_list	*ci_table = NULL;

static callin_entry_list* get_entry(const char* call_name)
{
	callin_entry_list	*entry;
	int 			len;
	entry = ci_table;
	for (len = strlen(call_name); NULL != entry; entry = entry->next_entry)
	{
		while (NULL != entry && entry->call_name.len != len)
			entry = entry->next_entry;
		if (NULL == entry || !memcmp(call_name, entry->call_name.addr, len))
			break;
	}
	return entry;
}

int gtm_ci (const char *c_rtn_name, ...)
{
	va_list			var;
	callin_entry_list	*entry;
	mstr			label, routine;
	int			has_return, i;
	rhdtyp          	*base_addr;
	unsigned char   	*transfer_addr;
	uint4			inp_mask, out_mask, mask;
	uint4			label_offset, *lnr_entry;
	mval			arg_mval, *arg_ptr;
	enum xc_types		arg_type;
	gtm_string_t		*mstr_parm;
	char			*xc_char_ptr;
	parmblk_struct 		param_blk;
	void 			op_extcall(), op_extexfun(), flush_pio(void);
	int			*save_var_on_cstack_ptr;
	int			status;
	error_def(ERR_MAXSTRLEN);
	error_def(ERR_CIRCALLNAME);
	error_def(ERR_CINOENTRY);

	if (!gtm_startup_active || !(frame_pointer->flags & SFF_CI))
	{
		if ((status = gtm_init()) != 0)
			return status;
	}
	ESTABLISH_RET(gtmci_ch, mumps_status);
	if (msp < fgncal_stack) /* unwind all arguments left on the stack by previous gtm_ci */
		fgncal_unwind();
	if (!ci_table) /* load the call-in table only once from env variable GTMCI  */
		ci_table = citab_parse();
	if (!c_rtn_name)
		rts_error(VARLSTCNT(1) ERR_CIRCALLNAME);
	if (!(entry = get_entry(c_rtn_name)))	/* c_rtn_name not found in the table */
		rts_error(VARLSTCNT(4) ERR_CINOENTRY, 2, LEN_AND_STR(c_rtn_name));
	lref_parse((unsigned char*)entry->label_ref.addr, &routine, &label, &i);
	job_addr(&routine, &label, 0, (char**)&base_addr, (char**)&transfer_addr);

	memset(&param_blk, sizeof(param_blk), 0);
	param_blk.rtnaddr = (void*)base_addr;
	/* lnr_entry below is a pointer to the code offset for this label from the
	 * beginning of text base(on USHBIN platforms) or from the beginning of routine
	 * header (on NON_USHBIN platforms).
	 * On NON_USHBIN platforms -- 2nd argument to EXTCALL is this pointer
	 * On USHBIN -- 2nd argument to EXTCALL is the pointer to this pointer (&lnr_entry) */
	lnr_entry = &label_offset;
	*lnr_entry = CODE_OFFSET(base_addr, transfer_addr);
	param_blk.labaddr = USHBIN_ONLY(&)lnr_entry;
	param_blk.argcnt = entry->argcnt;

	VAR_START(var, c_rtn_name);
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
	{ /* copy pass-by-value arguments - since only first MAXIMUM_PARAMETERS could be O/IO,
	     any additional params will be treated as Input-only (I).
	     inp_mask is inversed to achieve this. */
		arg_mval.mvtype = MV_XZERO;
		if (mask & 1)
		{ /* output-only(O) params : advance va_arg pointer */
			switch (entry->parms[i])
			{
				case xc_long:
					va_arg(var, gtm_long_t); break;
				case xc_ulong:
					va_arg(var, gtm_ulong_t); break;
				case xc_long_star:
					va_arg(var, gtm_long_t*); break;
				case xc_ulong_star:
					va_arg(var, gtm_ulong_t*); break;
				case xc_float:
				case xc_double:
					va_arg(var, gtm_double_t); break;
				case xc_float_star:
					va_arg(var, gtm_float_t*); break;
				case xc_double_star:
					va_arg(var, gtm_double_t*); break;
				case xc_char_star:
					va_arg(var, gtm_char_t*); break;
				case xc_string_star:
					va_arg(var, gtm_string_t*); break;
				default:
					va_end(var);
					GTMASSERT;
			}
		}
		else { /* I/IO params: create mval for each native type param */
			switch (entry->parms[i])
			{
				case xc_long:
					i2mval(&arg_mval, va_arg(var, gtm_long_t));
					break;
				case xc_ulong:
					i2usmval(&arg_mval, va_arg(var, gtm_ulong_t));
					break;
				case xc_long_star:
					i2mval(&arg_mval, *va_arg(var, gtm_long_t*));
					break;
				case xc_ulong_star:
					i2usmval(&arg_mval, *va_arg(var, gtm_ulong_t*));
					break;
				case xc_float: /* fall through */
				case xc_double:
					double2mval(&arg_mval, va_arg(var, gtm_double_t));
					break;
				case xc_float_star:
					double2mval(&arg_mval, *va_arg(var, gtm_float_t*));
					break;
				case xc_double_star:
					double2mval(&arg_mval, *va_arg(var, gtm_double_t*));
					break;
				case xc_char_star:
					arg_mval.mvtype = MV_STR;
					arg_mval.str.addr = va_arg(var, gtm_char_t*);
					arg_mval.str.len = strlen(arg_mval.str.addr);
					if (MAX_STRLEN < arg_mval.str.len)
					{
						va_end(var);
						rts_error(VARLSTCNT(1) ERR_MAXSTRLEN);
					}
					s2pool(&arg_mval.str);
					break;
				case xc_string_star:
					mstr_parm = va_arg(var, gtm_string_t*);
					arg_mval.mvtype = MV_STR;
					if (MAX_STRLEN < (uint4)mstr_parm->length)
					{
						va_end(var);
						rts_error(VARLSTCNT(1) ERR_MAXSTRLEN);
					}
					arg_mval.str.len = mstr_parm->length;
					arg_mval.str.addr = mstr_parm->address;
					s2pool(&arg_mval.str);

					break;
				default:
					va_end(var);
					GTMASSERT; /* should have been caught by citab_parse */
			}
		}
		param_blk.args[i] = push_mval(&arg_mval);
	}
	va_end(var);
	param_blk.mask = out_mask;
	param_blk.ci_rtn = (!has_return && param_blk.argcnt <= 0) ? &op_extcall : &op_extexfun;
	/* the params block needs to be stored & restored across multiple
	   gtm environments. So instead of storing explicitely, setting the
	   global param_list to point to local param_blk will do the job */
	param_list = &param_blk;

	save_var_on_cstack_ptr = var_on_cstack_ptr;
	var_on_cstack_ptr = NULL; /* reset var_on_cstack_ptr for the new M environment */
	assert(frame_pointer->flags & SFF_CI);
	frame_pointer->mpc = frame_pointer->ctxt = PTEXT_ADR(frame_pointer->rvector);
	REVERT; /* gtmci_ch */

	ESTABLISH_RET(stop_image_conditional_core, mumps_status);
	dm_start(); /* kick off execution */
	REVERT;

	var_on_cstack_ptr = save_var_on_cstack_ptr; /* restore the old environment's var_on_cstack_ptr */
	if (1 != mumps_status)
	{ /* dm_start() initializes mumps_status to 1 before execution. If mumps_status is not 1,
	     it is either the unhandled error code propaged by $ZT/$ET (from mdb_condition_handler)
	     or zero on returning from ZGOTO 0 (ci_ret_code_quit) */
		return mumps_status;
	}
	ESTABLISH_RET(gtmci_ch, mumps_status);
	VAR_START(var, c_rtn_name);
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
		/* Do not process parameters that are either input-only(I) or output(O/IO)
		 * parameters that are not modified by the M routine. */
		if (!(mask & 1) || !MV_DEFINED(arg_ptr))
		{
			switch (arg_type)
			{
				case xc_long_star:
					va_arg(var, gtm_long_t*); break;
				case xc_ulong_star:
					va_arg(var, gtm_ulong_t*); break;
				case xc_float_star:
					va_arg(var, gtm_float_t*); break;
				case xc_double_star:
					va_arg(var, gtm_double_t*); break;
				case xc_char_star:
					va_arg(var, gtm_char_t*); break;
				case xc_string_star:
					va_arg(var, gtm_string_t*); break;
				case xc_long:
					va_arg(var, gtm_long_t); break;
				case xc_ulong:
					va_arg(var, gtm_ulong_t); break;
				case xc_float:
				case xc_double:
					va_arg(var, gtm_double_t); break;
				default:
					va_end(var);
					GTMASSERT;
			}

		} else
		{ /* Process all output (O/IO) parameters modified by the M routine */
			switch (arg_type)
			{
				case xc_long_star:
					*va_arg(var, gtm_long_t*) = mval2i(arg_ptr); break;
				case xc_ulong_star:
					*va_arg(var, gtm_ulong_t*) = mval2ui(arg_ptr); break;
				case xc_float_star:
					*va_arg(var, gtm_float_t*) = mval2double(arg_ptr); break;
				case xc_double_star:
					*va_arg(var, gtm_double_t*) = mval2double(arg_ptr); break;
				case xc_char_star:
					xc_char_ptr = va_arg(var, gtm_char_t*);
					if (!MV_IS_STRING(arg_ptr))
						MV_FORCE_STR(arg_ptr);
					memcpy(xc_char_ptr, arg_ptr->str.addr, arg_ptr->str.len);
					xc_char_ptr[arg_ptr->str.len] = 0; /* trailing null */
					break;
				case xc_string_star:
					mstr_parm = va_arg(var, gtm_string_t*);
					if (!MV_IS_STRING(arg_ptr));
						MV_FORCE_STR(arg_ptr);
					mstr_parm->length = arg_ptr->str.len;
					memcpy(mstr_parm->address, arg_ptr->str.addr, mstr_parm->length);
					break;
				default:
					va_end(var);
					GTMASSERT;
			}
		}
	}
	va_end(var);
	REVERT;
	return 0;
}

int gtm_init()
{
	rhdtyp          	*base_addr;
	unsigned char   	*transfer_addr;
	error_def(ERR_CITPNESTED);
	error_def(ERR_CIMAXLEVELS);

	if (!gtm_startup_active)
	{ /* call-in invoked from C as base. GT.M hasn't been started up yet. */
		image_type = GTM_IMAGE;
		gtm_wcswidth_fnptr = gtm_wcswidth;
		gtm_env_init();	/* read in all environment variables */
		err_init(stop_image_conditional_core);
		if (gtm_utf8_mode)
			gtm_icu_init(); /* Note: should be invoked after err_init (since it may error out) and before CLI parsing */
		cli_lex_setup(0, NULL);
		/* Initialize msp to the maximum so if errors occur during GT.M startup below,
		 * the unwind logic in gtmci_ch() will get rid of the whole stack. */
		msp = (unsigned char*)-1;
	}
	ESTABLISH_RET(gtmci_ch, mumps_status);
	if (!gtm_startup_active)
	{ /* GT.M is not active yet. Create GT.M startup environment */
		invocation_mode = MUMPS_CALLIN;
		init_gtm();
		gtm_savetraps(); /* nullify default $ZTRAP handling */
		assert(gtm_startup_active);
		assert(frame_pointer->flags & SFF_CI);
		nested_level = 1;
	}
	else if (!(frame_pointer->flags & SFF_CI))
	{ /* Nested call-in: setup a new CI environment (SFF_CI frame on top of base-frame) */
		/* Mark the beginning of the new stack so that initialization errors in
		 * call-in frame do not unwind entries of the previous stack (see gtmci_ch).*/
		fgncal_stack = msp;
		/* Report if condition handlers stack may overrun during this callin level.
		 * Every underlying level can not have more than 2 active condition handlers,
		 * plus extra MAX_HANDLERS are reserved for this level. */
		if (chnd_end - ctxt <= MAX_HANDLERS)
			rts_error(VARLSTCNT(3) ERR_CIMAXLEVELS, 1, nested_level);
		/* Disallow call-ins within a TP boundary since TP restarts are not supported
		 * currently across nested call-ins. When we implement TP restarts across call-ins,
		 * this error needs be changed to a Warning or Notification */
		if (0 < dollar_tlevel)
			rts_error(VARLSTCNT(1) ERR_CITPNESTED);
		base_addr = make_cimode();
		transfer_addr = PTEXT_ADR(base_addr);
		gtm_init_env(base_addr, transfer_addr);
		SET_CI_ENV(ci_ret_code_exit);
		gtmci_isv_save();
		nested_level++;
	}
	/* Now that GT.M is initialized. Mark the new stack pointer (msp) so that errors
	 * while executing an M routine do not unwind stack below this mark. It important that
	 * the call-in frames (SFF_CI), that hold nesting information (eg. $ECODE/$STACK data
	 * of the previous stack), are kept from being unwound. */
	fgncal_stack = msp;
	REVERT;
	return 0;
}
/* routine exposed to call-in user to exit from active GT.M environment */
int gtm_exit()
{
	error_def(ERR_INVGTMEXIT);

	if (!gtm_startup_active)
		return 0;		/* GT.M environment not setup yet - quietly return */
	ESTABLISH_RET(gtmci_ch, mumps_status);

	assert(NULL != frame_pointer);
	/* Do not allow gtm_exit() to be invoked from external calls */
	if (!(SFF_CI & frame_pointer->flags) || !(MUMPS_CALLIN & invocation_mode) || (1 < nested_level))
		rts_error(VARLSTCNT(1) ERR_INVGTMEXIT);
	/* Now get rid of the whole M stack - end of GT.M environment */
	while (NULL != frame_pointer)
	{
		while (NULL != frame_pointer && !(frame_pointer->flags & SFF_CI))
			op_unwind();
		if (NULL != frame_pointer)
		{ /* unwind the current invocation of call-in environment */
			assert(frame_pointer->flags & SFF_CI);
			ci_ret_code_quit();
		}
	}
	gtm_exit_handler(); /* rundown all open database resource */
	REVERT;
	gtm_startup_active = FALSE;
	return 0;
}

void gtm_zstatus(char* msg, int len)
{
	int msg_len;

	msg_len = (len <= dollar_zstatus.str.len) ? len - 1 : dollar_zstatus.str.len;
	memcpy(msg, dollar_zstatus.str.addr, msg_len);
	msg[msg_len] = 0;
}
