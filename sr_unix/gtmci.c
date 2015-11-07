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
#include <stdarg.h>
#include "gtm_stdio.h"
#include <errno.h>
#ifdef GTM_PTHREAD
#  include <pthread.h>
#endif
#include "gtm_stdlib.h"
#include "gtm_string.h"
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
#include "job_addr.h"
#include "invocation_mode.h"
#include "gtmimagename.h"
#include "gtm_exit_handler.h"
#include "gtm_savetraps.h"
#include "gtm_env_init.h"	/* for gtm_env_init() prototype */
#include "code_address_type.h"
#include "push_lvval.h"
#include "send_msg.h"
#include "gtmmsg.h"
#include "gtm_imagetype_init.h"
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
#include "gt_timer.h"
#include "have_crit.h"
#include "callg.h"
#include "min_max.h"
#include "gtm_limits.h"

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
GTMTRIG_DBG_ONLY(GBLREF ch_ret_type (*ch_at_trigger_init)();)
LITREF  gtmImageName            gtmImageNames[];

error_def(ERR_ACTLSTTOOLONG);
error_def(ERR_CALLINAFTERXIT);
error_def(ERR_CIMAXLEVELS);
error_def(ERR_CINOENTRY);
error_def(ERR_CIRCALLNAME);
error_def(ERR_CITPNESTED);
error_def(ERR_INVGTMEXIT);
error_def(ERR_JOBLABOFF);
error_def(ERR_MAXACTARG);
error_def(ERR_MAXSTRLEN);

#define REVERT_AND_RETURN						\
{									\
	REVERT; /* gtmci_ch */						\
	TREF(in_gtmci) = FALSE;						\
	return 0;							\
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

int gtm_is_main_thread()
{
# 	ifdef GTM_PTHREAD
	if (!gtm_main_thread_id_set)
		return -1;
	if (pthread_equal(gtm_main_thread_id, pthread_self()))
		return 1;
	return 0;
#	else
	return -1;
#	endif
}

/* Java-specific version of call-in handler. */
int gtm_cij(const char *c_rtn_name, char **arg_blob, int count, int *arg_types, unsigned int *io_vars_mask,
		unsigned int *has_ret_value)
{
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
	set_blocksig();
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
	TREF(in_gtmci) = TRUE;
	if (!gtm_startup_active || !(frame_pointer->flags & SFF_CI))
	{
		if ((status = gtm_init()) != 0)
		{
			TREF(in_gtmci) = FALSE;
			return status;
		}
	}
	GTM_PTHREAD_ONLY(assert(gtm_main_thread_id_set && pthread_equal(gtm_main_thread_id, pthread_self())));
	ESTABLISH_RET(gtmci_ch, mumps_status);
	if (msp < fgncal_stack)	/* Unwind all arguments left on the stack by previous gtm_cij. */
		fgncal_unwind();
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
	/* The 3rd argument is NULL because we will get lnr_adr via lab_proxy. */
	if(!job_addr(&routine, &label, 0, (char **)&base_addr, NULL))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JOBLABOFF);
	memset(&param_blk, 0, SIZEOF(param_blk));
	param_blk.rtnaddr = (void *)base_addr;
	/* lnr_entry below is a pointer to the code offset for this label from the
	 * beginning of text base(on USHBIN platforms) or from the beginning of routine
	 * header (on NON_USHBIN platforms).
	 * On NON_USHBIN platforms -- 2nd argument to EXTCALL is this pointer
	 * On USHBIN -- 2nd argument to EXTCALL is the pointer to this pointer (&lnr_entry)
	 */
	/* Assign the address for line number entry storage, so that the adjacent address holds has_parms value. */
	param_blk.labaddr = &(TREF(lab_proxy)).LABENT_LNR_OFFSET;
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
	for (i = 0, mask = ~inp_mask; i < count; ++i, mask >>= 1, java_arg_type++, arg_blob_ptr += GTM64_ONLY(1) NON_GTM64_ONLY(2))
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
				if (!(mask & 1))
					i2mval(&arg_mval, *(int *)arg_blob_ptr);
				break;
			case gtm_jint:
				CHECK_FOR_TYPE_MISMATCH(i + 1, 1, *java_arg_type);
				if (!(mask & 1))
					i2mval(&arg_mval, *(int *)arg_blob_ptr);
				break;
			case gtm_jlong:
				CHECK_FOR_TYPE_MISMATCH(i + 1, 2, *java_arg_type);
				if (!(mask & 1))
				i82mval(&arg_mval, *(gtm_int64_t *)arg_blob_ptr);
				break;
			case gtm_jfloat:
				CHECK_FOR_TYPE_MISMATCH(i + 1, 3, *java_arg_type);
				if (!(mask & 1))
					float2mval(&arg_mval, *(float *)arg_blob_ptr);
				break;
			case gtm_jdouble:
				CHECK_FOR_TYPE_MISMATCH(i + 1, 4, *java_arg_type);
				if (!(mask & 1))
					double2mval(&arg_mval, *(double *)arg_blob_ptr);
				break;
			case gtm_jstring:
				CHECK_FOR_TYPES_MISMATCH(i + 1, 7, 5, *java_arg_type);
				if (!(mask & 1))
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
				if (!(mask & 1))
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
				if (!(mask & 1))
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

	ESTABLISH_RET(stop_image_conditional_core, mumps_status);
	dm_start();					/* Kick off execution. */
	REVERT;

	intrpt_ok_state = old_intrpt_state;		/* Restore the old interrupt state. */
	var_on_cstack_ptr = save_var_on_cstack_ptr;	/* Restore the old environment's var_on_cstack_ptr. */
	if (1 != mumps_status)
	{
		TREF(in_gtmci) = FALSE;
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
		if ((mask & 1) && MV_DEFINED(arg_ptr))
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
					(*(gtm_string_t **)arg_blob_ptr)->address = arg_ptr->str.addr;
					(*(gtm_string_t **)arg_blob_ptr)->length = arg_ptr->str.len;
					if (((unsigned char *)arg_ptr->str.addr + arg_ptr->str.len) == stringpool.top) /*BYPASSOK*/
					{	/* Since the ci_gateway.c code temporarily switches the character following the
						 * string's content in memory to '\n' (for generation of a proper Unicode string),
						 * ensure that this character is in the stringpool and not elsewhere.
						 */
						ENSURE_STP_FREE_SPACE(1);
					}
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
					GTMASSERT;
			}
		}
	}
	REVERT;
	TREF(in_gtmci) = FALSE;
	return 0;
}

int gtm_ci_exec(const char *c_rtn_name, void *callin_handle, int populate_handle, va_list temp_var)
{
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
	set_blocksig();
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
	TREF(in_gtmci) = TRUE;
	if (!gtm_startup_active || !(frame_pointer->flags & SFF_CI))
	{
		if ((status = gtm_init()) != 0)
		{
			TREF(in_gtmci) = FALSE;
			return status;
		}
	}
	ESTABLISH_RET(gtmci_ch, mumps_status);
	if (msp < fgncal_stack)	/* unwind all arguments left on the stack by previous gtm_ci */
		fgncal_unwind();
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
	/* 3rd argument is NULL because we will get lnr_adr via lab_proxy */
	if(!job_addr(&routine, &label, 0, (char **)&base_addr, NULL))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JOBLABOFF);
	memset(&param_blk, 0, SIZEOF(param_blk));
	param_blk.rtnaddr = (void *)base_addr;
	/* lnr_entry below is a pointer to the code offset for this label from the
	 * beginning of text base(on USHBIN platforms) or from the beginning of routine
	 * header (on NON_USHBIN platforms).
	 * On NON_USHBIN platforms -- 2nd argument to EXTCALL is this pointer
	 * On USHBIN -- 2nd argument to EXTCALL is the pointer to this pointer (&lnr_entry)
	 *
	 * Assign the address for line number entry storage, so that the adjacent address holds has_parms value.
	 */
	param_blk.labaddr = &(TREF(lab_proxy)).LABENT_LNR_OFFSET;
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
		if (mask & 1)
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

	ESTABLISH_RET(stop_image_conditional_core, mumps_status);
	dm_start(); /* kick off execution */
	REVERT;

	intrpt_ok_state = old_intrpt_state; /* restore the old interrupt state */
	var_on_cstack_ptr = save_var_on_cstack_ptr; /* restore the old environment's var_on_cstack_ptr */
	if (1 != mumps_status)
	{
		TREF(in_gtmci) = FALSE;
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
		if (!(mask & 1) || !MV_DEFINED(arg_ptr))
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
		} else
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
		}
	}
	va_end(temp_var);
	REVERT;
	TREF(in_gtmci) = FALSE;
	return 0;
}

int gtm_ci(const char *c_rtn_name, ...)
{
	va_list var;

	VAR_START(var, c_rtn_name);
	return gtm_ci_exec(c_rtn_name, NULL, FALSE, var);
}

/* Functionality is same as that of gtmci but accepts a struct containing information about the routine. */
int gtm_cip(ci_name_descriptor* ci_info, ...)
{
	va_list var;

	VAR_START(var, ci_info);
	return gtm_ci_exec(ci_info->rtn_name.address, ci_info->handle, TRUE, var);
}

#ifdef GTM_PTHREAD
int gtm_jinit()
{
	gtm_jvm_process = TRUE;
	return gtm_init();
}
#endif

int gtm_init()
{
	rhdtyp          	*base_addr;
	unsigned char   	*transfer_addr;
	char			*dist;
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
	if (!TREF(in_gtmci))
		return 0;
	if (!gtm_startup_active)
	{	/* call-in invoked from C as base. GT.M hasn't been started up yet. */
		gtm_imagetype_init(GTM_IMAGE);
		gtm_wcswidth_fnptr = gtm_wcswidth;
		gtm_env_init();	/* read in all environment variables */
		err_init(stop_image_conditional_core);
		UNICODE_ONLY(gtm_strToTitle_ptr = &gtm_strToTitle);
		GTM_ICU_INIT_IF_NEEDED;	/* Note: should be invoked after err_init (since it may error out) and before CLI parsing */
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
		init_gtm();
		gtm_savetraps(); /* nullify default $ZTRAP handling */
		if (NULL != (dist = (char *)GETENV(GTM_DIST)))
		{
			assert(IS_VALID_IMAGE && (n_image_types > image_type));	/* assert image_type is initialized */
			if ((GTM_PATH_MAX - 2) <= (STRLEN(dist) + gtmImageNames[image_type].imageNameLen))
				dist = NULL;
			else
				memcpy(gtm_dist, dist, STRLEN(dist));
		}
		assert(gtm_startup_active);
		assert(frame_pointer->flags & SFF_CI);
		TREF(gtmci_nested_level) = 1;
	} else if (!(frame_pointer->flags & SFF_CI))
	{	/* Nested call-in: setup a new CI environment (SFF_CI frame on top of base-frame).
		 * Mark the beginning of the new stack so that initialization errors in
		 * call-in frame do not unwind entries of the previous stack (see gtmci_ch).
		 */
		fgncal_stack = msp;
		/* generate CIMAXLEVELS error if gtmci_nested_level > CALLIN_MAX_LEVEL */
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
	}
	/* Now that GT.M is initialized. Mark the new stack pointer (msp) so that errors
	 * while executing an M routine do not unwind stack below this mark. It important that
	 * the call-in frames (SFF_CI), that hold nesting information (eg. $ECODE/$STACK data
	 * of the previous stack), are kept from being unwound.
	 */
	fgncal_stack = msp;
	REVERT;
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
#ifdef _AIX
	unatexit(gtm_exit_handler);
#endif
	REVERT;
	gtm_startup_active = FALSE;
	return 0;
}

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
