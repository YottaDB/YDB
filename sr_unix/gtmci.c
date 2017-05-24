/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#ifdef GTM_PTHREAD
#  include "gtm_pthread.h"
#endif
#include "gtm_stat.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "gtm_limits.h"
#include "cli.h"
#include "stringpool.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "mvalconv.h"
#include "gtmxc_types.h"
#include "lv_val.h"
#include "fgncal.h"
#include "gtmci.h"
#include "error.h"
#include "startup.h"
#include "mv_stent.h"
#include "op.h"
#include "gtm_startup.h"
#include "gtmsecshr.h"
#include "job_addr.h"
#include "invocation_mode.h"
#include "gtmimagename.h"
#include "gtm_exit_handler.h"
#include "gtm_savetraps.h"
#include "code_address_type.h"
#include "push_lvval.h"
#include "send_msg.h"
#include "gtmmsg.h"
#include "common_startup_init.h"
#include "gtm_threadgbl_init.h"
#ifdef GTM_TRIGGER
# include "gdsroot.h"
# include "gtm_facility.h"
# include "fileinfo.h"
# include "gdsbt.h"
# include "gdsfhead.h"
# include "gv_trigger.h"
# include "gtm_trigger.h"
#endif
#ifdef UNICODE_SUPPORTED
# include "gtm_icu_api.h"
# include "gtm_utf8.h"
# include "gtm_conv.h"
GBLREF	u_casemap_t 		gtm_strToTitle_ptr;		/* Function pointer for gtm_strToTitle */
#endif
#include "hashtab.h"
#include "hashtab_str.h"
#include "compiler.h"
#include "have_crit.h"
#include "callg.h"
#include "min_max.h"

GBLREF	parmblk_struct 		*param_list;
GBLREF  stack_frame     	*frame_pointer;
GBLREF  unsigned char		*msp;
GBLREF  mv_stent         	*mv_chain;
GBLREF	int			mumps_status;
GBLREF 	void			(*restart)();
GBLREF 	boolean_t		gtm_startup_active;
GBLREF	volatile int 		*var_on_cstack_ptr;	/* volatile so that nothing gets optimized out */
GBLREF	rhdtyp			*ci_base_addr;
GBLREF  mval			dollar_zstatus;
GBLREF  unsigned char		*fgncal_stack;
GBLREF  uint4			dollar_tlevel;
GBLREF	int			process_exiting;
#ifdef GTM_PTHREAD
GBLREF	boolean_t		gtm_jvm_process;
GBLREF	pthread_t		gtm_main_thread_id;
GBLREF	boolean_t		gtm_main_thread_id_set;
#endif
GBLREF	char			gtm_dist[GTM_PATH_MAX];
GBLREF boolean_t		gtm_dist_ok_to_use;
GTMTRIG_DBG_ONLY(GBLREF ch_ret_type (*ch_at_trigger_init)();)
LITREF  gtmImageName            gtmImageNames[];

error_def(ERR_ACTLSTTOOLONG);
error_def(ERR_CALLINAFTERXIT);
error_def(ERR_CIMAXLEVELS);
error_def(ERR_CINOENTRY);
error_def(ERR_CIRCALLNAME);
error_def(ERR_CITPNESTED);
error_def(ERR_DISTPATHMAX);
error_def(ERR_GTMDISTUNDEF);
error_def(ERR_GTMSECSHRPERM);
error_def(ERR_INVGTMEXIT);
error_def(ERR_JOBLABOFF);
error_def(ERR_MAXACTARG);
error_def(ERR_MAXSTRLEN);
error_def(ERR_SYSCALL);

#define REVERT_AND_RETURN						\
{									\
	REVERT; /* gtmci_ch */						\
	return 0;							\
}

/* Unwind the M stack back to where the stack pointer (msp) was last saved */
#define FGNCAL_UNWIND							\
{									\
	if (msp < fgncal_stack)						\
		fgncal_unwind();					\
}

/* When passing arguments from Java, ensure that the expected types match the actual ones. If not,
 * use the arg_types array to pass back the information needed for a detailed error message.
 */
#define CHECK_FOR_TYPE_MISMATCH(INDEX, EXP_TYPE, ACT_TYPE)		\
{									\
	if (EXP_TYPE != ACT_TYPE)					\
	{								\
		arg_types[3] = ACT_TYPE;				\
		arg_types[2] = EXP_TYPE;				\
		arg_types[1] = INDEX;					\
		arg_types[0] = -1;					\
		REVERT_AND_RETURN;					\
	}								\
}

/* When passing arguments from Java, ensure that the either of the expected types matches the actual one.
 * If not, use the arg_types array to pass back the information needed for a detailed error message.
 */
#define CHECK_FOR_TYPES_MISMATCH(INDEX, EXP_TYPE1, EXP_TYPE2, ACT_TYPE)	\
{									\
	if ((EXP_TYPE1 != ACT_TYPE) && (EXP_TYPE2 != ACT_TYPE))		\
	{								\
		arg_types[4] = ACT_TYPE;				\
		arg_types[3] = EXP_TYPE1;				\
		arg_types[2] = EXP_TYPE2;				\
		arg_types[1] = INDEX;					\
		arg_types[0] = -1;					\
		REVERT_AND_RETURN;					\
	}								\
}

/* When returning a typed value, ensure that the declared type matches the expected one. If not,
 * use the arg_types array to pass back the information needed for a detailed error message.
 */
#define CHECK_FOR_RET_TYPE_MISMATCH(INDEX, EXP_TYPE, ACT_TYPE)	\
{								\
	if ((0 == INDEX) && (EXP_TYPE != ACT_TYPE))		\
	{							\
		arg_types[3] = ACT_TYPE;			\
		arg_types[2] = EXP_TYPE;			\
		arg_types[1] = 0;				\
		arg_types[0] = -1;				\
		REVERT_AND_RETURN;				\
	}							\
}

/* Call-ins uses the fgncal_stack global as a marker in the stack for where to unwind the stack back to. This preserves
 * the call-in base frame(s) but removes any other frames left on the stack as well as the parameter related mv_stents
 * and any other mv_stents no longer needed. This macro saves the current value of fgncal_stack on the M stack in an
 * MVST_STCK_SP type mv_stent. Note MVST_STCK_SP is chosen (instead of MVST_STCK) because MVST_STCK_SP doesn't get removed
 * if the frame is rewritten by a ZGOTO for instance.
 *
 * Note: If call-in's use of setjmp/longjmp for returns is changed to use the trigger method of actually unwinding the M
 * frames and returning "normally", then the usage of fgncal_stack likely becomes superfluous so should be looked at.
 */
# define SAVE_FGNCAL_STACK								\
{											\
	if (msp != fgncal_stack)							\
	{										\
		push_stck(fgncal_stack, 0, (void **)&fgncal_stack, MVST_STCK_SP);	\
		fgncal_stack = msp;							\
	}										\
}

static callin_entry_list* get_entry(const char* call_name)
{	/* Lookup in a hashtable for entry corresponding to routine name */
	ht_ent_str      *callin_entry;
	stringkey       symkey;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	symkey.str.addr = (char *)call_name;
	symkey.str.len = STRLEN(call_name);
	COMPUTE_HASH_STR(&symkey);
	callin_entry = lookup_hashtab_str(TREF(callin_hashtab), &symkey);
	return (callin_entry ? callin_entry->value : NULL);
}

/* Java-specific version of call-in handler. */
int gtm_cij(const char *c_rtn_name, char **arg_blob, int count, int *arg_types, unsigned int *io_vars_mask,
		unsigned int *has_ret_value)
{
	boolean_t		need_rtnobj_shm_free;
	callin_entry_list	*entry;
	mstr			label, routine;
	int			has_return, i, len;
	rhdtyp          	*base_addr;
	uint4			inp_mask, out_mask, mask;
	mval			arg_mval, *arg_ptr;
	enum gtm_types		arg_type;
	gtm_string_t		*mstr_parm;
	parmblk_struct 		param_blk;
	void 			op_extcall(), op_extexfun(), flush_pio(void);
	volatile int		*save_var_on_cstack_ptr;	/* Volatile to match global var type */
	int			status;
	boolean_t 		added;
	stringkey       	symkey;
	ht_ent_str		*syment;
	intrpt_state_t		old_intrpt_state;
	char			**arg_blob_ptr;
	int			*java_arg_type;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	added = FALSE;
	/* A prior invocation of gtm_exit would have set process_exiting = TRUE. Use this to disallow gtm_ci to be
	 * invoked after a gtm_exit
	 */
	if (process_exiting)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CALLINAFTERXIT);
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CALLINAFTERXIT);
		return ERR_CALLINAFTERXIT;
	}
	if (!gtm_startup_active || !(frame_pointer->flags & SFF_CI))
	{
		if ((status = gtm_init()) != 0)		/* Note - sets fgncal_stack */
			return status;
	}
	GTM_PTHREAD_ONLY(assert(gtm_main_thread_id_set && pthread_equal(gtm_main_thread_id, pthread_self())));
	assert(NULL == TREF(temp_fgncal_stack));
	FGNCAL_UNWIND;		/* note - this is outside the establish since gtmci_ch calso calls fgncal_unwind() which,
				 * if this failed, would lead to a nested error which we'd like to avoid */
	ESTABLISH_RET(gtmci_ch, mumps_status);
	if (!c_rtn_name)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CIRCALLNAME);
	if (!TREF(ci_table))	/* Load the call-in table only once from env variable GTMCI. */
	{
		TREF(ci_table) = citab_parse();
		if (!TREF(callin_hashtab))
		{
			TREF(callin_hashtab) = (hash_table_str *)malloc(SIZEOF(hash_table_str));
			(TREF(callin_hashtab))->base = NULL;
			/* Need to initialize hash table. */
			init_hashtab_str(TREF(callin_hashtab), CALLIN_HASHTAB_SIZE,
				HASHTAB_NO_COMPACT, HASHTAB_NO_SPARE_TABLE);
			assert((TREF(callin_hashtab))->base);
		}
		for (entry = TREF(ci_table); NULL != entry; entry = entry->next_entry)
		{	/* Loop over the list and populate the hash table. */
			symkey.str.addr = entry->call_name.addr;
			symkey.str.len = entry->call_name.len;
			COMPUTE_HASH_STR(&symkey);
			added = add_hashtab_str(TREF(callin_hashtab), &symkey, entry, &syment);
			assert(added);
			assert(syment->value == entry);
		}
	}
	if (!(entry = get_entry(c_rtn_name)))	/* c_rtn_name not found in the table. */
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CINOENTRY, 2, LEN_AND_STR(c_rtn_name));
	lref_parse((unsigned char*)entry->label_ref.addr, &routine, &label, &i);
	/* The 3rd argument is NULL because we will get lnr_adr via TABENT_PROXY. */
	/* See comment in ojstartchild.c about "need_rtnobj_shm_free". It is not used here because we will
	 * decrement rtnobj reference counts at exit time in relinkctl_rundown (called by gtm_exit_handler).
	 */
	if (!job_addr(&routine, &label, 0, (char **)&base_addr, NULL, &need_rtnobj_shm_free))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JOBLABOFF);
	memset(&param_blk, 0, SIZEOF(param_blk));
	param_blk.rtnaddr = (void *)(ARLINK_ONLY(0) NON_ARLINK_ONLY(base_addr));
	/* lnr_entry below is a pointer to the code offset for this label from the
	 * beginning of text base(on USHBIN platforms) or from the beginning of routine
	 * header (on NON_USHBIN platforms).
	 * On NON_USHBIN platforms -- 2nd argument to EXTCALL is this pointer
	 * On USHBIN -- 2nd argument to EXTCALL is the pointer to this pointer (&lnr_entry)
	 */
	/* Assign the address for line number entry storage, so that the adjacent address holds has_parms value. */
	param_blk.labaddr = (void *)(ARLINK_ONLY(-1) NON_ARLINK_ONLY(&(TABENT_PROXY).LABENT_LNR_OFFSET));
	if (MAX_ACTUALS < entry->argcnt)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXACTARG);
	if (entry->argcnt < count)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_ACTLSTTOOLONG, 2, (int)label.len, label.addr);
	param_blk.argcnt = count;
	has_return = (gtm_void != entry->return_type);
	if (has_return)
	{	/* Create mval slot for return value */
		MV_INIT(&arg_mval);
		param_blk.retaddr = (void *)push_lvval(&arg_mval);
		arg_blob_ptr = &arg_blob[0] + GTM64_ONLY(1) NON_GTM64_ONLY(2);
		java_arg_type = arg_types + 1;
	} else
	{
		param_blk.retaddr = 0;
		arg_blob_ptr = &arg_blob[0];
		java_arg_type = arg_types;
	}
	inp_mask = entry->input_mask;
	out_mask = entry->output_mask;
	*io_vars_mask = out_mask;
	if (*has_ret_value != has_return)
	{
		*has_ret_value = has_return;
		REVERT_AND_RETURN;
	}
	*has_ret_value = has_return;
	for (i = 0, mask = inp_mask; i < count; ++i, mask >>= 1, java_arg_type++, arg_blob_ptr += GTM64_ONLY(1) NON_GTM64_ONLY(2))
	{	/* Copy the arguments' values into mval containers. Since some arguments might be declared as output-only,
		 * we need to go over all of them unconditionally, but only do the copying for the ones that are used for
		 * the input direction (I or IO). The integer values passed to CHECK_FOR_TYPE_MISMATCH as a second argument
		 * indicate the types to expect according to the call-in table definition, and are in correspondence with the
		 * constants declared in GTMContainerType class in gtmji.jar: 0 for GTMBoolean, 1 for GTMInteger, and so on.
		 */
		arg_mval.mvtype = MV_XZERO;
		switch (entry->parms[i])
		{
			case gtm_jboolean:
				CHECK_FOR_TYPE_MISMATCH(i + 1, 0, *java_arg_type);
				if (MASK_BIT_ON(mask))
					i2mval(&arg_mval, *(int *)arg_blob_ptr);
				break;
			case gtm_jint:
				CHECK_FOR_TYPE_MISMATCH(i + 1, 1, *java_arg_type);
				if (MASK_BIT_ON(mask))
					i2mval(&arg_mval, *(int *)arg_blob_ptr);
				break;
			case gtm_jlong:
				CHECK_FOR_TYPE_MISMATCH(i + 1, 2, *java_arg_type);
				if (MASK_BIT_ON(mask))
				i82mval(&arg_mval, *(gtm_int64_t *)arg_blob_ptr);
				break;
			case gtm_jfloat:
				CHECK_FOR_TYPE_MISMATCH(i + 1, 3, *java_arg_type);
				if (MASK_BIT_ON(mask))
					float2mval(&arg_mval, *(float *)arg_blob_ptr);
				break;
			case gtm_jdouble:
				CHECK_FOR_TYPE_MISMATCH(i + 1, 4, *java_arg_type);
				if (MASK_BIT_ON(mask))
					double2mval(&arg_mval, *(double *)arg_blob_ptr);
				break;
			case gtm_jstring:
				CHECK_FOR_TYPES_MISMATCH(i + 1, 7, 5, *java_arg_type);
				if (MASK_BIT_ON(mask))
				{
					mstr_parm = *(gtm_string_t **)arg_blob_ptr;
					arg_mval.mvtype = MV_STR;
					if (MAX_STRLEN < (uint4)mstr_parm->length)
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXSTRLEN);
					arg_mval.str.len = (mstr_len_t)mstr_parm->length;
					arg_mval.str.addr = mstr_parm->address;
					s2pool(&arg_mval.str);
				}
				break;
			case gtm_jbyte_array:
				CHECK_FOR_TYPES_MISMATCH(i + 1, 8, 6, *java_arg_type);
				if (MASK_BIT_ON(mask))
				{
					mstr_parm = *(gtm_string_t **)arg_blob_ptr;
					arg_mval.mvtype = MV_STR;
					if (MAX_STRLEN < (uint4)mstr_parm->length)
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXSTRLEN);
					arg_mval.str.len = (mstr_len_t)mstr_parm->length;
					arg_mval.str.addr = mstr_parm->address;
					s2pool(&arg_mval.str);
				}
				break;
			case gtm_jbig_decimal:
				CHECK_FOR_TYPE_MISMATCH(i + 1, 9, *java_arg_type);
				if (MASK_BIT_ON(mask))
				{
					mstr_parm = *(gtm_string_t **)arg_blob_ptr;
					arg_mval.mvtype = MV_STR;
					if (MAX_STRLEN < (uint4)mstr_parm->length)
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXSTRLEN);
					arg_mval.str.len = (mstr_len_t)mstr_parm->length;
					arg_mval.str.addr = mstr_parm->address;
					s2pool(&arg_mval.str);
				}
				break;
			default:			/* Indicate an invalid type. */
				arg_types[1] = i + 1;
				arg_types[0] = -2;
				REVERT_AND_RETURN;
		}
		param_blk.args[i] = push_lvval(&arg_mval);
	}
	param_blk.mask = out_mask;
	param_blk.ci_rtn = (!has_return && param_blk.argcnt <= 0)
		? (void (*)())CODE_ADDRESS_TYPE(op_extcall)
		: (void (*)())CODE_ADDRESS_TYPE(op_extexfun);
	/* The params block needs to be saved and restored across multiple GT.M environments. So, instead of storing it
	 * explicitely, setting the global param_list to point to local param_blk will do the job.
	 */
	param_list = &param_blk;
	old_intrpt_state = intrpt_ok_state;
	intrpt_ok_state = INTRPT_OK_TO_INTERRUPT;	/* Reset interrupt state for the new M session. */
	save_var_on_cstack_ptr = var_on_cstack_ptr;
	var_on_cstack_ptr = NULL;			/* Reset var_on_cstack_ptr for the new M environment. */
	assert(frame_pointer->flags & SFF_CI);
	frame_pointer->mpc = frame_pointer->ctxt = PTEXT_ADR(frame_pointer->rvector);
	REVERT;						/* Revert gtmci_ch. */
	/*				*/
	/* Drive the call_in routine	*/
	/*				*/
	ESTABLISH_RET(stop_image_conditional_core, mumps_status);
	dm_start(); 	/* Kick off execution */
	REVERT;
	/*				*/
	/* Return value processing	*/
	/*				*/
	intrpt_ok_state = old_intrpt_state;		/* Restore the old interrupt state. */
	var_on_cstack_ptr = save_var_on_cstack_ptr;	/* Restore the old environment's var_on_cstack_ptr. */
	if (1 != mumps_status)
	{
		/* dm_start() initializes mumps_status to 1 before execution. If mumps_status is not 1,
		 * it is either the unhandled error code propaged by $ZT/$ET (from mdb_condition_handler)
		 * or zero on returning from ZGOTO 0 (ci_ret_code_quit).
		 */
		return mumps_status;
	}
	ESTABLISH_RET(gtmci_ch, mumps_status);
	/* Convert mval args designated for output-only or input-output use to C types. */
	arg_blob_ptr = &arg_blob[0];
	for (i = 0; i <= count; ++i, arg_blob_ptr += GTM64_ONLY(1) NON_GTM64_ONLY(2))
	{
		if (0 == i)				/* Special case for return value. */
		{
			if (!has_return)
			{
				arg_blob_ptr -= GTM64_ONLY(1) NON_GTM64_ONLY(2);
				continue;
			}
			arg_ptr = &((lv_val *)(param_blk.retaddr))->v;
			mask = 1;
			arg_type = entry->return_type;
		} else
		{
			arg_ptr = &param_blk.args[i - 1]->v;
			mask = out_mask;
			arg_type = entry->parms[i - 1];
			out_mask >>= 1;
		}
		/* Do not process parameters that are either input-only(I) or output(O/IO)
		 * parameters that are not modified by the M routine.
		 */
		if (MV_ON(mask, arg_ptr))
		{	/* Process all output (O/IO) parameters modified by the M routine */
			switch (arg_type)
			{
				case gtm_jboolean:
					CHECK_FOR_RET_TYPE_MISMATCH(i, 0, *arg_types);
					*(gtm_int_t *)arg_blob_ptr = mval2double(arg_ptr) ? 1 : 0;
					break;
                                case gtm_jint:
					CHECK_FOR_RET_TYPE_MISMATCH(i, 1, *arg_types);
					*(gtm_int_t *)arg_blob_ptr = mval2i(arg_ptr);
					break;
				case gtm_jlong:
					CHECK_FOR_RET_TYPE_MISMATCH(i, 2, *arg_types);
					*(gtm_int64_t *)arg_blob_ptr = mval2i8(arg_ptr);
					break;
				case gtm_jfloat:
					CHECK_FOR_RET_TYPE_MISMATCH(i, 3, *arg_types);
					*(gtm_float_t *)arg_blob_ptr = mval2double(arg_ptr);
					break;
				case gtm_jdouble:
					CHECK_FOR_RET_TYPE_MISMATCH(i, 4, *arg_types);
					*(gtm_double_t *)arg_blob_ptr = mval2double(arg_ptr);
					break;
				case gtm_jstring:
					CHECK_FOR_RET_TYPE_MISMATCH(i, 7, *arg_types);
					MV_FORCE_STR(arg_ptr);
					/* Since the ci_gateway.c code temporarily switches the character following the string's
					 * content in memory to '\0' (for generation of a proper Unicode string), ensure that the
					 * whole string resides in the stringpool, and that we do have that one byte to play with.
					 */
					if (!IS_IN_STRINGPOOL(arg_ptr->str.addr, arg_ptr->str.len))
						s2pool(&arg_ptr->str);
					ENSURE_STP_FREE_SPACE(1);
					(*(gtm_string_t **)arg_blob_ptr)->address = arg_ptr->str.addr;
					(*(gtm_string_t **)arg_blob_ptr)->length = arg_ptr->str.len;
					break;
				case gtm_jbyte_array:
					CHECK_FOR_RET_TYPE_MISMATCH(i, 8, *arg_types);
					MV_FORCE_STR(arg_ptr);
					(*(gtm_string_t **)arg_blob_ptr)->address = arg_ptr->str.addr;
					(*(gtm_string_t **)arg_blob_ptr)->length = arg_ptr->str.len;
					break;
				case gtm_jbig_decimal:	/* We currently do not support output for big decimal. */
					break;
				default:
					assertpro((arg_type >= gtm_jboolean) && (arg_type <= gtm_jbig_decimal));
			}
		}
	}
	REVERT;
	assert(NULL == TREF(temp_fgncal_stack));
	FGNCAL_UNWIND;
	return 0;
}

/* Common work-routine for gtm_ci() and gtm_cip() to drive callin */
int gtm_ci_exec(const char *c_rtn_name, void *callin_handle, int populate_handle, va_list temp_var)
{
	boolean_t		need_rtnobj_shm_free;
	va_list			var;
	callin_entry_list	*entry;
	mstr			label, routine;
	int			has_return, i;
	rhdtyp          	*base_addr;
	uint4			inp_mask, out_mask, mask;
	mval			arg_mval, *arg_ptr;
	enum gtm_types		arg_type;
	gtm_string_t		*mstr_parm;
	char			*gtm_char_ptr;
	parmblk_struct 		param_blk;
	void 			op_extcall(), op_extexfun(), flush_pio(void);
	volatile int		*save_var_on_cstack_ptr;	/* Volatile to match global var type */
	int			status;
	boolean_t 		added;
	stringkey       	symkey;
	ht_ent_str		*syment;
	intrpt_state_t		old_intrpt_state;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	VAR_COPY(var, temp_var);
	added = FALSE;
	/* A prior invocation of gtm_exit would have set process_exiting = TRUE. Use this to disallow gtm_ci to be
	 * invoked after a gtm_exit
	 */
	if (process_exiting)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CALLINAFTERXIT);
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CALLINAFTERXIT);
		return ERR_CALLINAFTERXIT;
	}
	if (!gtm_startup_active || !(frame_pointer->flags & SFF_CI))
	{
		if ((status = gtm_init()) != 0)		/* Note - sets fgncal_stack */
			return status;
	}
	assert(NULL == TREF(temp_fgncal_stack));
	FGNCAL_UNWIND;		/* note - this is outside the establish since gtmci_ch calso calls fgncal_unwind() which,
				 * if this failed, would lead to a nested error which we'd like to avoid */
	ESTABLISH_RET(gtmci_ch, mumps_status);
	if (!c_rtn_name)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CIRCALLNAME);
	if (!TREF(ci_table))	/* load the call-in table only once from env variable GTMCI  */
	{
		TREF(ci_table) = citab_parse();
		if (!TREF(callin_hashtab))
		{
			TREF(callin_hashtab) = (hash_table_str *)malloc(SIZEOF(hash_table_str));
			(TREF(callin_hashtab))->base = NULL;
			/* Need to initialize hash table */
			init_hashtab_str(TREF(callin_hashtab), CALLIN_HASHTAB_SIZE,
				HASHTAB_NO_COMPACT, HASHTAB_NO_SPARE_TABLE);
			assert((TREF(callin_hashtab))->base);
		}
		for (entry = TREF(ci_table); NULL != entry; entry = entry->next_entry)
		{	/* Loop over the list and populate the hash table */
			symkey.str.addr = entry->call_name.addr;
			symkey.str.len = entry->call_name.len;
			COMPUTE_HASH_STR(&symkey);
			added = add_hashtab_str(TREF(callin_hashtab), &symkey, entry, &syment);
			assert(added);
			assert(syment->value == entry);
		}
	}
	if (NULL == callin_handle)
	{
		if (!(entry = get_entry(c_rtn_name)))	/* c_rtn_name not found in the table */
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CINOENTRY, 2, LEN_AND_STR(c_rtn_name));
		if (populate_handle)
			callin_handle = entry;
	} else
		entry = callin_handle;
	lref_parse((unsigned char*)entry->label_ref.addr, &routine, &label, &i);
	/* 3rd argument is NULL because we will get lnr_adr via TABENT_PROXY */
	/* See comment in ojstartchild.c about "need_rtnobj_shm_free". It is not used here because we will
	 * decrement rtnobj reference counts at exit time in relinkctl_rundown (called by gtm_exit_handler).
	 */
	if (!job_addr(&routine, &label, 0, (char **)&base_addr, NULL, &need_rtnobj_shm_free))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JOBLABOFF);
	memset(&param_blk, 0, SIZEOF(param_blk));
	param_blk.rtnaddr = (void *)(ARLINK_ONLY(0) NON_ARLINK_ONLY(base_addr));
	/* lnr_entry below is a pointer to the code offset for this label from the
	 * beginning of text base(on USHBIN platforms) or from the beginning of routine
	 * header (on NON_USHBIN platforms).
	 * On NON_USHBIN platforms -- 2nd argument to EXTCALL is this pointer
	 * On USHBIN -- 2nd argument to EXTCALL is the pointer to this pointer (&lnr_entry)
	 *
	 * Assign the address for line number entry storage, so that the adjacent address holds has_parms value.
	 */
	param_blk.labaddr = (void *)(ARLINK_ONLY(-1) NON_ARLINK_ONLY(&(TABENT_PROXY).LABENT_LNR_OFFSET));
	param_blk.argcnt = entry->argcnt;
	if (MAX_ACTUALS < param_blk.argcnt)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXACTARG);
	has_return = (gtm_void == entry->return_type) ? 0 : 1;
	if (has_return)
	{	/* Create mval slot for return value */
		param_blk.retaddr = (void *)push_lvval(&arg_mval);
		va_arg(var, void *);	/* advance va_arg */
	} else
		param_blk.retaddr = NULL;
	inp_mask = entry->input_mask;
	out_mask = entry->output_mask;
	for (i = 0, mask = ~inp_mask; i < entry->argcnt; ++i, mask >>= 1)
	{	/* Copy pass-by-value arguments - since only first MAX_ACTUALS could be O/IO,
		 * any additional params will be treated as Input-only (I).
		 * inp_mask is inversed to achieve this.
		 */
		arg_mval.mvtype = MV_XZERO;
		if (MASK_BIT_ON(mask))
		{ 	/* Output-only(O) params : advance va_arg pointer */
			switch (entry->parms[i])
			{
				case gtm_int:
					va_arg(var, gtm_int_t);
					break;
				case gtm_uint:
					va_arg(var, gtm_uint_t);
					break;
				case gtm_long:
					va_arg(var, gtm_long_t);
					break;
				case gtm_ulong:
					va_arg(var, gtm_ulong_t);
					break;
				case gtm_int_star:
					va_arg(var, gtm_int_t *);
					break;
				case gtm_uint_star:
					va_arg(var, gtm_uint_t *);
					break;
				case gtm_long_star:
					va_arg(var, gtm_long_t *);
					break;
				case gtm_ulong_star:
					va_arg(var, gtm_ulong_t *);
					break;
				case gtm_float:
				case gtm_double:
					va_arg(var, gtm_double_t);
					break;
				case gtm_float_star:
					va_arg(var, gtm_float_t *);
					break;
				case gtm_double_star:
					va_arg(var, gtm_double_t *);
					break;
				case gtm_char_star:
					va_arg(var, gtm_char_t *);
					break;
				case gtm_string_star:
					va_arg(var, gtm_string_t *);
					break;
				default:
					va_end(var);
					assertpro(FALSE);
			}
		} else
		{ 	/* I/IO params: create mval for each native type param */
			switch (entry->parms[i])
			{
                                case gtm_int:
                                        i2mval(&arg_mval, va_arg(var, gtm_int_t));
                                        break;
                                case gtm_uint:
                                        i2usmval(&arg_mval, va_arg(var, gtm_uint_t));
                                        break;
				case gtm_long:
#					ifdef GTM64
					i82mval(&arg_mval, (gtm_int64_t)va_arg(var, gtm_long_t));
#					else
					i2mval(&arg_mval, (int)va_arg(var, gtm_long_t));
#					endif
					break;
				case gtm_ulong:
#					ifdef GTM64
					ui82mval(&arg_mval, (gtm_uint64_t)va_arg(var, gtm_ulong_t));
#					else
					i2usmval(&arg_mval, (int)va_arg(var, gtm_ulong_t));
#					endif
					break;
                                case gtm_int_star:
                                        i2mval(&arg_mval, *va_arg(var, gtm_int_t *));
                                        break;
                                case gtm_uint_star:
                                        i2usmval(&arg_mval, *va_arg(var, gtm_uint_t *));
                                        break;
				case gtm_long_star:
#					ifdef GTM64
					i82mval(&arg_mval, (gtm_int64_t)*va_arg(var, gtm_long_t *));
#					else
					i2mval(&arg_mval, (int)*va_arg(var, gtm_long_t *));
#					endif
					break;
				case gtm_ulong_star:
#					ifdef GTM64
					ui82mval(&arg_mval, (gtm_uint64_t)*va_arg(var, gtm_ulong_t *));
#					else
					i2usmval(&arg_mval, (int)*va_arg(var, gtm_ulong_t *));
#					endif
					break;
				case gtm_float:
					float2mval(&arg_mval, (gtm_float_t)va_arg(var, gtm_double_t));
					break;
				case gtm_double:
					double2mval(&arg_mval, va_arg(var, gtm_double_t));
					break;
				case gtm_float_star:
					float2mval(&arg_mval, *va_arg(var, gtm_float_t *));
					break;
				case gtm_double_star:
					double2mval(&arg_mval, *va_arg(var, gtm_double_t *));
					break;
				case gtm_char_star:
					arg_mval.mvtype = MV_STR;
					arg_mval.str.addr = va_arg(var, gtm_char_t *);
					arg_mval.str.len = STRLEN(arg_mval.str.addr);
					if (MAX_STRLEN < arg_mval.str.len)
					{
						va_end(var);
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXSTRLEN);
					}
					s2pool(&arg_mval.str);
					break;
				case gtm_string_star:
					mstr_parm = va_arg(var, gtm_string_t *);
					arg_mval.mvtype = MV_STR;
					if (MAX_STRLEN < (uint4)mstr_parm->length)
					{
						va_end(var);
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXSTRLEN);
					}
					arg_mval.str.len = (mstr_len_t)mstr_parm->length;
					arg_mval.str.addr = mstr_parm->address;
					s2pool(&arg_mval.str);
					break;
				default:
					va_end(var);
					assertpro(FALSE); /* should have been caught by citab_parse */
			}
		}
		param_blk.args[i] = push_lvval(&arg_mval);
	}
	va_end(var);
	param_blk.mask = out_mask;
	param_blk.ci_rtn = (!has_return && param_blk.argcnt <= 0)
		? (void (*)())CODE_ADDRESS_TYPE(op_extcall)
		: (void (*)())CODE_ADDRESS_TYPE(op_extexfun);
	/* The params block needs to be stored & restored across multiple
	 * gtm environments. So instead of storing explicitely, setting the
	 * global param_list to point to local param_blk will do the job.
	 */
	param_list = &param_blk;
	old_intrpt_state = intrpt_ok_state;
	intrpt_ok_state = INTRPT_OK_TO_INTERRUPT; /* reset interrupt state for the new M session */
	save_var_on_cstack_ptr = var_on_cstack_ptr;
	var_on_cstack_ptr = NULL; /* reset var_on_cstack_ptr for the new M environment */
	assert(frame_pointer->flags & SFF_CI);
	frame_pointer->mpc = frame_pointer->ctxt = PTEXT_ADR(frame_pointer->rvector);
	REVERT; /* gtmci_ch */
	/*				*/
	/* Drive the call_in routine	*/
	/*				*/
	ESTABLISH_RET(stop_image_conditional_core, mumps_status);
	dm_start(); 	/* Kick off execution */
	REVERT;
	/*				*/
	/* Return value processing	*/
	/*				*/
	intrpt_ok_state = old_intrpt_state; /* restore the old interrupt state */
	var_on_cstack_ptr = save_var_on_cstack_ptr; /* restore the old environment's var_on_cstack_ptr */
	if (1 != mumps_status)
	{
		/* dm_start() initializes mumps_status to 1 before execution. If mumps_status is not 1,
		 * it is either the unhandled error code propaged by $ZT/$ET (from mdb_condition_handler)
		 * or zero on returning from ZGOTO 0 (ci_ret_code_quit).
		 */
		return mumps_status;
	}
	ESTABLISH_RET(gtmci_ch, mumps_status);
	/* Convert mval args passed by reference to C types */
	for (i = 0; i <= entry->argcnt; ++i)
	{
		if (0 == i) /* Special case for return value */
		{
			if (!has_return)
				continue;
			arg_ptr = &((lv_val *)(param_blk.retaddr))->v;
			mask = 1;
			arg_type = entry->return_type;
		} else
		{
			arg_ptr = &param_blk.args[i - 1]->v;
			mask = out_mask;
			arg_type = entry->parms[i - 1];
			out_mask >>= 1;
		}
		/* Do not process parameters that are either input-only(I) or output(O/IO)
		 * parameters that are not modified by the M routine.
		 */
		if (MV_ON(mask, arg_ptr))
		{	/* Process all output (O/IO) parameters modified by the M routine */
			switch (arg_type)
			{
                                case gtm_int_star:
                                        *va_arg(temp_var, gtm_int_t *) = mval2i(arg_ptr);
					break;
                                case gtm_uint_star:
                                        *va_arg(temp_var, gtm_uint_t *) = mval2ui(arg_ptr);
					break;
				case gtm_long_star:
					*va_arg(temp_var, gtm_long_t *) =
						GTM64_ONLY(mval2i8(arg_ptr)) NON_GTM64_ONLY(mval2i(arg_ptr));
					break;
				case gtm_ulong_star:
					*va_arg(temp_var, gtm_ulong_t *) =
						GTM64_ONLY(mval2ui8(arg_ptr)) NON_GTM64_ONLY(mval2ui(arg_ptr));
					break;
				case gtm_float_star:
					*va_arg(temp_var, gtm_float_t *) = mval2double(arg_ptr);
					break;
				case gtm_double_star:
					*va_arg(temp_var, gtm_double_t *) = mval2double(arg_ptr);
					break;
				case gtm_char_star:
					gtm_char_ptr = va_arg(temp_var, gtm_char_t *);
					MV_FORCE_STR(arg_ptr);
					memcpy(gtm_char_ptr, arg_ptr->str.addr, arg_ptr->str.len);
					gtm_char_ptr[arg_ptr->str.len] = 0; /* trailing null */
					break;
				case gtm_string_star:
					mstr_parm = va_arg(temp_var, gtm_string_t *);
					MV_FORCE_STR(arg_ptr);
					mstr_parm->length = arg_ptr->str.len;
					memcpy(mstr_parm->address, arg_ptr->str.addr, mstr_parm->length);
					break;
				default:
					va_end(temp_var);
					assertpro(FALSE);
			}
		} else
		{
			switch (arg_type)
			{
                                case gtm_int_star:
                                        va_arg(temp_var, gtm_int_t *);
					break;
                                case gtm_uint_star:
                                        va_arg(temp_var, gtm_uint_t *);
					break;
				case gtm_long_star:
					va_arg(temp_var, gtm_long_t *);
					break;
				case gtm_ulong_star:
					va_arg(temp_var, gtm_ulong_t *);
					break;
				case gtm_float_star:
					va_arg(temp_var, gtm_float_t *);
					break;
				case gtm_double_star:
					va_arg(temp_var, gtm_double_t *);
					break;
				case gtm_char_star:
					va_arg(temp_var, gtm_char_t *);
					break;
				case gtm_string_star:
					va_arg(temp_var, gtm_string_t *);
					break;
                                case gtm_int:
                                        va_arg(temp_var, gtm_int_t);
					break;
                                case gtm_uint:
                                        va_arg(temp_var, gtm_uint_t);
					break;
 				case gtm_long:
					va_arg(temp_var, gtm_long_t);
					break;
				case gtm_ulong:
					va_arg(temp_var, gtm_ulong_t);
					break;
				case gtm_float:
				case gtm_double:
					va_arg(temp_var, gtm_double_t);
					break;
				default:
					va_end(temp_var);
					assertpro(FALSE);
			}
		}
	}
	va_end(temp_var);
	REVERT;
	assert(NULL == TREF(temp_fgncal_stack));
	FGNCAL_UNWIND;
	return 0;
}

/* Initial call-in driver version - does name lookup on each call */
int gtm_ci(const char *c_rtn_name, ...)
{
	va_list var;

	VAR_START(var, c_rtn_name);
	return gtm_ci_exec(c_rtn_name, NULL, FALSE, var);
}

/* Fast path call-in driver version - Adds a struct parm that contains name resolution info after first call
 * to speed up dispatching.
 */
int gtm_cip(ci_name_descriptor* ci_info, ...)
{
	va_list var;

	VAR_START(var, ci_info);
	return gtm_ci_exec(ci_info->rtn_name.address, ci_info->handle, TRUE, var);
}

#ifdef GTM_PTHREAD
/* Java flavor of gtm_init() */
int gtm_jinit()
{
	gtm_jvm_process = TRUE;
	return gtm_init();
}
#endif

/* Initialization routine - can be called directly by call-in caller or can be driven by gtm_ci*() implicitly. But
 * if other GT.M services are to be used prior to a gtm_ci*() call (like timers, gtm_malloc/free, etc), this routine
 * should be called first.
 */
int gtm_init()
{
	rhdtyp          	*base_addr;
	unsigned char   	*transfer_addr;
	char			*dist;
	int			dist_len;
	char			gtmsecshr_path[GTM_PATH_MAX];
	int			gtmsecshr_path_len;
	struct stat		stat_buf;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (NULL == lcl_gtm_threadgbl)
	{	/* This will likely need some attention before going to a threaded model */
		assert(!gtm_startup_active);
		GTM_THREADGBL_INIT;
	}
	/* A prior invocation of gtm_exit would have set process_exiting = TRUE. Use this to disallow gtm_init to be
	 * invoked after a gtm_exit
	 */
	if (process_exiting)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CALLINAFTERXIT);
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CALLINAFTERXIT);
		return ERR_CALLINAFTERXIT;
	}
	if (!gtm_startup_active)
	{	/* call-in invoked from C as base. GT.M hasn't been started up yet. */
		common_startup_init(GTM_IMAGE);
		err_init(stop_image_conditional_core);
		UNICODE_ONLY(gtm_strToTitle_ptr = &gtm_strToTitle);
		GTM_ICU_INIT_IF_NEEDED;	/* Note: should be invoked after err_init (since it may error out) and before CLI parsing */
		/* Ensure that $gtm_dist exists */
		if (NULL == (dist = (char *)GETENV(GTM_DIST)))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_GTMDISTUNDEF);
		/* Ensure that $gtm_dist is non-zero and does not exceed GTM_DIST_PATH_MAX */
		dist_len = STRLEN(dist);
		if (!dist_len)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_GTMDISTUNDEF);
		else if (GTM_DIST_PATH_MAX <= dist_len)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_DISTPATHMAX, 1, GTM_DIST_PATH_MAX);
		/* Verify that $gtm_dist/gtmsecshr is available with setuid root */
		memcpy(gtmsecshr_path, gtm_dist, dist_len);
		gtmsecshr_path[dist_len] =  '/';
		memcpy(gtmsecshr_path + dist_len + 1, GTMSECSHR_EXECUTABLE, STRLEN(GTMSECSHR_EXECUTABLE));
		gtmsecshr_path_len = dist_len + 1 + STRLEN(GTMSECSHR_EXECUTABLE);
		assertpro(GTM_PATH_MAX > gtmsecshr_path_len);
		gtmsecshr_path[gtmsecshr_path_len] = '\0';
		if (-1 == Stat(gtmsecshr_path, &stat_buf))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
					LEN_AND_LIT("stat for $gtm_dist/gtmsecshr"), CALLFROM, errno);
		/* Ensure that the call-in can execute $gtm_dist/gtmsecshr. This not sufficient for security purposes */
		if ((ROOTUID != stat_buf.st_uid) || !(stat_buf.st_mode & S_ISUID))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_GTMSECSHRPERM);
		else
		{	/* $gtm_dist validated */
			gtm_dist_ok_to_use = TRUE;
			memcpy(gtm_dist, dist, dist_len);
		}
		cli_lex_setup(0, NULL);
		/* Initialize msp to the maximum so if errors occur during GT.M startup below,
		 * the unwind logic in gtmci_ch() will get rid of the whole stack.
		 */
		msp = (unsigned char *)-1L;
		GTMTRIG_DBG_ONLY(ch_at_trigger_init = &mdb_condition_handler);
	}
	ESTABLISH_RET(gtmci_ch, mumps_status);
	if (!gtm_startup_active)
	{	/* GT.M is not active yet. Create GT.M startup environment */
		invocation_mode = MUMPS_CALLIN;
		init_gtm();			/* Note - this initializes fgncal_stackbase */
		gtm_savetraps(); /* nullify default $ZTRAP handling */
		assert(IS_VALID_IMAGE && (n_image_types > image_type));	/* assert image_type is initialized */
		assert(gtm_startup_active);
		assert(frame_pointer->flags & SFF_CI);
		TREF(gtmci_nested_level) = 1;
		/* Now that GT.M is initialized. Mark the new stack pointer (msp) so that errors
		 * while executing an M routine do not unwind stack below this mark. It important that
		 * the call-in frames (SFF_CI) that hold nesting information (eg. $ECODE/$STACK data
		 * of the previous stack) are kept from being unwound.
		 */
		SAVE_FGNCAL_STACK;
	} else if (!(frame_pointer->flags & SFF_CI))
	{	/* Nested call-in: setup a new CI environment (SFF_CI frame on top of base-frame).
		 * Temporarily mark the beginning of the new stack so that initialization errors in
		 * call-in frame do not unwind entries of the previous stack (see gtmci_ch). For the
		 * duration that temp_fgncal_stack has a non-NULL value, it overrides fgncal_stack.
		 */
		TREF(temp_fgncal_stack) = msp;
		/* Generate CIMAXLEVELS error if gtmci_nested_level > CALLIN_MAX_LEVEL */
		if (CALLIN_MAX_LEVEL < TREF(gtmci_nested_level))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_CIMAXLEVELS, 1, TREF(gtmci_nested_level));
		/* Disallow call-ins within a TP boundary since TP restarts are not supported
		 * currently across nested call-ins. When we implement TP restarts across call-ins,
		 * this error needs be changed to a Warning or Notification
		 */
		if (dollar_tlevel)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CITPNESTED);
		base_addr = make_cimode();
		transfer_addr = PTEXT_ADR(base_addr);
		gtm_init_env(base_addr, transfer_addr);
		SET_CI_ENV(ci_ret_code_exit);
		gtmci_isv_save();
		(TREF(gtmci_nested_level))++;
		/* Now that the base-frames for this call-in level have been created, we can create the mv_stent
		 * to save the previous call-in level's fgncal_stack value and clear the override. When this call-in
		 * level pops, fgncal_stack will be restored to the value for the previous level. When a given call
		 * at *this* level finishes, this current value of fgncal_stack is where the stack is unrolled to to
		 * be ready for the next call.
		 */
		SAVE_FGNCAL_STACK;
		TREF(temp_fgncal_stack) = NULL;		/* Drop override */
	}
	REVERT;
	assert(NULL == TREF(temp_fgncal_stack));
	return 0;
}

/* routine exposed to call-in user to exit from active GT.M environment */
int gtm_exit()
{
        DCL_THREADGBL_ACCESS;

        SETUP_THREADGBL_ACCESS;
	if (!gtm_startup_active)
		return 0;		/* GT.M environment not setup yet - quietly return */
	ESTABLISH_RET(gtmci_ch, mumps_status);
	assert(NULL != frame_pointer);
	/* Do not allow gtm_exit() to be invoked from external calls */
	if (!(SFF_CI & frame_pointer->flags) || !(MUMPS_CALLIN & invocation_mode) || (1 < TREF(gtmci_nested_level)))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INVGTMEXIT);
	/* Now get rid of the whole M stack - end of GT.M environment */
	while (NULL != frame_pointer)
	{
		while ((NULL != frame_pointer) && !(frame_pointer->flags & SFF_CI))
		{
#			ifdef GTM_TRIGGER
			if (SFT_TRIGR & frame_pointer->type)
				gtm_trigger_fini(TRUE, FALSE);
			else
#			endif
				op_unwind();
		}
		if (NULL != frame_pointer)
		{	/* unwind the current invocation of call-in environment */
			assert(frame_pointer->flags & SFF_CI);
			ci_ret_code_quit();
		}
	}
	gtm_exit_handler(); /* rundown all open database resource */
	/* If libgtmshr was loaded via (or on account of) dlopen() and is later unloaded via dlclose()
	 * the exit handler on AIX and HPUX still tries to call the registered atexit() handler causing
	 * 'problems'. AIX 5.2 and later have the below unatexit() call to unregister the function if
	 * our exit handler has already been called. Linux and Solaris don't need this, looking at the
	 * other platforms we support to see if resolutions can be found. SE 05/2007
	 */
#	ifdef _AIX
	unatexit(gtm_exit_handler);
#	endif
	REVERT;
	gtm_startup_active = FALSE;
	return 0;
}

/* Routine to fetch $ZSTATUS after an error has been raised */
void gtm_zstatus(char *msg, int len)
{
	int msg_len;
	msg_len = (len <= dollar_zstatus.str.len) ? len - 1 : dollar_zstatus.str.len;
	memcpy(msg, dollar_zstatus.str.addr, msg_len);
	msg[msg_len] = 0;
}

#ifdef _AIX
/* If libgtmshr was loaded via (or on account of) dlopen() and is later unloaded via dlclose()
 * the exit handler on AIX and HPUX still tries to call the registered atexit() handler causing
 * 'problems'. AIX 5.2 and later have the below unatexit() call to unregister the function if
 * our exit handler has already been called. Linux and Solaris don't need this, looking at the
 * other platforms we support to see if resolutions can be found. This routine will be called
 * by the OS when libgtmshr is unloaded. Specified with the -binitfini loader option on AIX
 * to be run when the shared library is unloaded. 06/2007 SE
 */
void gtmci_cleanup(void)
{	/* This code is only for callin cleanup */
	if (MUMPS_CALLIN != invocation_mode)
		return;
	/* If we have already run the exit handler, no need to do so again */
	if (gtm_startup_active)
	{
		gtm_exit_handler();
		gtm_startup_active = FALSE;
	}
	/* Unregister exit handler .. AIX only for now */
	unatexit(gtm_exit_handler);
}
#endif
