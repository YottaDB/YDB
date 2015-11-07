/****************************************************************
 *								*
 *	Copyright 2010, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GV_TRIGGER_H_INCLUDED
#define GV_TRIGGER_H_INCLUDED

#include "gv_trigger_common.h"	/* ^#t related macros (common to both Unix and VMS) */

error_def(ERR_GVIS);
error_def(ERR_GVZTRIGFAIL);
error_def(ERR_TRIGREPLSTATE);

#define	HASHT_OPT_ISOLATION	"I"
#define	HASHT_OPT_NOISOLATION	"NOI"
#define	HASHT_OPT_CONSISTENCY	"C"
#define	HASHT_OPT_NOCONSISTENCY	"NOC"

typedef enum
{
#define	GV_TRIG_CMD_ENTRY(cmdtype, cmdlit, cmdmaskval)	cmdtype,
#include "gv_trig_cmd_table.h"
#undef GV_TRIG_CMD_ENTRY
	GVTR_CMDTYPES		/* Total number of command types related to triggers */
} gvtr_cmd_type_t;

#define GVTR_SUBS_STAR		1	/* Allow ANY value for this subscript */
#define GVTR_SUBS_POINT		2	/* Allow a FIXED value for this subscript */
#define GVTR_SUBS_RANGE		3	/* Allow a RANGE of values for this subscript */
#define GVTR_SUBS_PATTERN	4	/* Allow all values for subscript that match a specified pattern */

#define	GVTR_RANGE_OPEN_LEN	(uint4)-1	/* length assigned to open side of a range (e.g. "a": has an open right side);
					   	 * is considered an impossible value for the length of any valid subscript */

#define	GVTR_LIST_ELE_SIZE	8	/* size of each element in gv_trig_list buddy list (see comment in gv_trigger.c) */
#define	GV_TRIG_LIST_INIT_ALLOC	256	/* we anticipate 256 bytes to be used by each trigger so start the buddy list there */

#define MAX_TRIG_UTIL_LEN 	40	/* needed for storing the trigger index and the property (CMD, TRIGNAME.. etc.)
					 * in util_buff to be passed as the last parameter for TRIGDEFBAD error message.
					 * Both the index and the property name are guaranteed to be less than 20
					 * and hence MAX_TRIG_UTIL_LEN set to 40 should be enough
					 */
/* Miscellaneous structures needed to build the global variable trigger superstructures : gv_trigger_t and gvt_trigger_t */
typedef struct gvtr_subs_star_struct
{
        uint4   		gvtr_subs_type;
        uint4			filler_8byte_align;
        union gvtr_subs_struct	*next_range;
} gvtr_subs_star_t;

typedef struct gvtr_subs_point_struct
{
        uint4			gvtr_subs_type;
        uint4			len;
        union gvtr_subs_struct	*next_range;
        char			*subs_key;
} gvtr_subs_point_t;

typedef struct gvtr_subs_range_struct
{
        uint4			gvtr_subs_type;
        uint4			len1;
        union gvtr_subs_struct	*next_range;
        char			*subs_key1;
        uint4			len2;
        char			*subs_key2;
} gvtr_subs_range_t;

typedef struct gvtr_subs_pattern_struct
{
        uint4			gvtr_subs_type;
        uint4			filler_8byte_align;
        union gvtr_subs_struct	*next_range;
        mval    		pat_mval;
} gvtr_subs_pattern_t;

typedef union gvtr_subs_struct
{
        uint4                   gvtr_subs_type;
        gvtr_subs_star_t        gvtr_subs_star;
        gvtr_subs_point_t       gvtr_subs_point;
        gvtr_subs_range_t       gvtr_subs_range;
        gvtr_subs_pattern_t     gvtr_subs_pattern;
} gvtr_subs_t;

typedef gtm_num_range_t	gvtr_piece_t;

/* gv_trigger_t is the structure describing the static components of ONE trigger for a given global variable name. At the time of
 * trigger invocation, there is some process-context-specific information that needs to be passed in which is done in a separate
 * gtm_trigger_parms structure (defined in gtm_trigger.h). The below fields are arranged in order to ensure minimal structure
 * padding added by the compiler. This might not seem logical at first (for e.g. the related fields is_zdelim & delimiter are in
 * different sections).
 */
typedef struct gv_trigger_struct
{
	struct gv_trigger_struct	/* 3 chains - one for each command type since a given trigger could be on all 3. */
			*next_set, 	/* Next SET type trigger for this global */
			*next_kill, 	/* Next KILL type trigger for this global */
			*next_ztrig;	/* Next ZTRIGGER type trigger for this global */
	uint4		cmdmask;	/* bitwise OR of all commands defining this trigger in ^#t(<GBL>,<index>,"CMD").
					 * e.g. if ^#t(..,"CMD") is "S,K", cmdmask will be "GVTR_OPER_SET | GVTR_OPER_KILL" */
	uint4		numsubs;	/* # of comma-separated subscripts specified for this particular trigger */
	uint4		numlvsubs;	/* # of subscripts for which the trigger requested a local variable name to be bound.
					 * i.e. numlvsubs <= numsubs is always true. */
	uint4		numpieces;	/* # of contiguous piece ranges specified in the trigger */
	gvtr_subs_t	*subsarray;	/* pointer to an array of "numsubs" number of gvtr_subs_t structures.
					 * NULL if no subscript specified in the trigger (i.e. numsubs = 0) and is very unusual. */
	uint4		*lvindexarray;	/* pointer to an array of "numlvsubs" number of uint4 type fields which contain the index
					 * in "subsarray" of the subscript whose local variable name binding this corresponds to. */
	mname_entry	*lvnamearray;	/* pointer to an array of "numlvsubs" number of mname_entry structures that contain the
					 * local variable names which need to be bound to each subscript at trigger invocation.
					 * If no subscripts have lvns specified, this is NULL.
					 * e.g. if the trigger specified "^GBL(lvn1=:,1,lvn3=:) ..." in this case,
					 * 	numsubs = 3, numlvsubs = 2, lvindexarray[0] = 0, lvindexarray[1] = 2,
					 * 	lvnamearray[0].var_name = "lvn1", lvnamearray[1].var_name = "lvn3", */
	gtm_num_range_t	*piecearray;	/* pointer to an array of "numpieces" ranges (range is the closed interval [min,max]).
					 * e.g, if the piece string specified in ^#t(<GBL>,<index>,"PIECES") is "4:6;8" then
					 *	numpieces = 2
					 *	piecearray[0].min=4, piecearray[0].max=6
					 *	piecearray[1].min=8, piecearray[1].max=8 */
	rtn_tabent	rtn_desc;	/* Compiled routine name/objcode; Inited by gvtr_db_read_hasht() */
	boolean_t	is_zdelim;	/* TRUE if "ZDELIM" was specified; FALSE if "DELIM" was specified */
	mstr		delimiter;	/* is a copy of ^#t(<GBL>,<index>,"DELIM") or ^#t(..."ZDELIM") whichever is defined */
	mstr		options;	/* is a copy of ^#t(<GBL>,<index>,"OPTIONS") */
	mval		xecute_str;	/* Trigger code to execute */
	struct gvt_trigger_struct	/* top of gvt_trigger block this trigger belongs to. Used in src lookup when we know */
			*gvt_trigger;	/* .. gv_trigger_t but not owning region or trigger#. Allows us to get both with no
					 * additional lookup */
} gv_trigger_t;

/* Structure describing ALL triggers for a given global variable name */
typedef struct gvt_trigger_struct
{
	uint4				gv_trigger_cycle;	/* copy of ^#t(<gbl>,"#CYCLE") */
	uint4				num_gv_triggers;	/* copy of ^#t(<gbl>,"#COUNT") */
	struct gv_trigger_struct	*set_triglist;		/* -> circular list of SET triggers (using next_set link) */
	struct gv_trigger_struct	*kill_triglist;		/* -> circular list of KILL type triggers (using next_kill link) */
	struct gv_trigger_struct	*ztrig_triglist;	/* -> circular list of ZTRIG triggers (using next_ztrig link) */
	gv_namehead			*gv_target;		/* gv_target that owns these triggers - used in trigr src lkup */
	gv_trigger_t			*gv_trig_top;		/* top of the array of triggers */
	struct buddy_list_struct	*gv_trig_list;		/* buddy list that maintains mallocs done inside gv_trig_array */
	gv_trigger_t			*gv_trig_array;		/* array of triggers read in from ^#t(<gbl>,...) */
} gvt_trigger_t;

/* Structure describing parameters passed (from gvcst_put/gvcst_kill) to trigger invocation routine */
typedef struct gvtr_invoke_parms_struct
{
	gvt_trigger_t	*gvt_trigger;		/* Input parameter */
	gvtr_cmd_type_t	gvtr_cmd;		/* Input parameter */
	int		num_triggers_invoked;	/* Output parameter : # of triggers invoked by an update */
} gvtr_invoke_parms_t;

/* Requires #include of op.h, tp_set_sgm.h, t_begin.h in the caller of this macro */
#define	GVTR_INIT_AND_TPWRAP_IF_NEEDED(CSA, CSD, GVT, GVT_TRGR, LCL_TSTART, IS_TPWRAP, T_ERR)					\
{																\
	GBLREF	boolean_t		skip_dbtriggers;	/* see gbldefs.c for description of this global */		\
	GBLREF	sgmnt_data_ptr_t	cs_data;										\
	GBLREF	uint4			dollar_tlevel;										\
	GBLREF	jnl_gbls_t		jgbl;											\
	GBLREF	uint4			t_err;											\
	GBLREF	trans_num		local_tn;										\
																\
	DEBUG_ONLY(GBLREF gv_namehead	*reset_gv_target;)									\
	DEBUG_ONLY(GBLREF boolean_t	donot_INVOKE_MUMTSTART;)								\
	DEBUG_ONLY(GBLREF tp_frame	*tp_pointer;)										\
	DEBUG_ONLY(GBLREF stack_frame	*frame_pointer;)									\
	DEBUG_ONLY(GBLREF mv_stent	*mv_chain;)										\
	DEBUG_ONLY(GBLREF unsigned char	*msp;)											\
	DEBUG_ONLY(GBLREF int		tprestart_state;)									\
	DEBUG_ONLY(GBLREF sgm_info	*sgm_info_ptr;)										\
	DEBUG_ONLY(gv_namehead		*save_reset_gv_target;)									\
	DEBUG_ONLY(boolean_t		was_nontp;)										\
																\
	LITREF	mval			literal_batch;										\
	uint4				cycle;											\
	boolean_t			set_upd_trans_t_err, cycle_mismatch, db_trigger_cycle_mismatch, ztrig_cycle_mismatch;	\
																\
	assert(TPRESTART_STATE_NORMAL == tprestart_state);									\
	assert(!skip_dbtriggers);												\
	/* If start of transaction, read in GVT's triggers from ^#t global if not already done.	If restart and if it was due to	\
	 * GVT's triggers being out-of-date re-read them. Note theoretically either CSD or CSA can be used to get the cycle for	\
	 * comparison with GVT but while using CSD can relatively reduce the # of times "gvtr_init" is invoked, it can also	\
	 * cause an issue where a nested trigger for the same global can cause a restart which unloads a running trigger	\
	 * causing problems (see trigthrash subtest in triggers test suite specifically trigthrash3.m). For that reason, we	\
	 * stick wish CSA.													\
	 */															\
	cycle = CSA->db_trigger_cycle;												\
	assert(CSD == cs_data);													\
	/* triggers can be invoked only by updates currently */									\
	assert(!dollar_tlevel || sgm_info_ptr);											\
	assert((dollar_tlevel && sgm_info_ptr->update_trans) || (!dollar_tlevel && update_trans)); 				\
	set_upd_trans_t_err = FALSE;												\
	ztrig_cycle_mismatch = (CSA->db_dztrigger_cycle && (GVT->db_dztrigger_cycle != CSA->db_dztrigger_cycle));		\
	db_trigger_cycle_mismatch = (GVT->db_trigger_cycle != cycle);								\
	cycle_mismatch = (db_trigger_cycle_mismatch || ztrig_cycle_mismatch);							\
	/* Set up wrapper even if no triggers if this is for ZTRIGGER command */						\
	if (cycle_mismatch || (NULL != GVT->gvt_trigger) || (ERR_GVZTRIGFAIL == T_ERR))						\
	{	/* Create TP wrap if needed */											\
		if (!dollar_tlevel)												\
		{	/* need to create implicit TP wrap */									\
			DEBUG_ONLY(was_nontp = TRUE;)										\
			assert(!LCL_TSTART);											\
			/* Set a debug-only global variable to indicate that from now onwards, until the completion of this	\
			 * tp-wrapped non-tp update, we dont expect "t_retry" to be called while this gvcst_put is in the	\
			 * C-call-stack. This is because we have not set up the TP transaction using opp_tstart (like what M	\
			 * code does) and so there is no point to go back to generated code (mdb_condition_handler invoked from	\
			 * t_retry does this transfer of control using the MUM_TSTART macro). We instead expect to handle	\
			 * retries internally in gvcst_put. We also expect any restarts occurring in nested trigger code to	\
			 * eventually end up as a RESTART return code from "gtm_trigger" so we get to choose how to handle the	\
			 * restart for this implicit TSTART.									\
			 */													\
			assert(!donot_INVOKE_MUMTSTART);									\
			DEBUG_ONLY(donot_INVOKE_MUMTSTART = TRUE;)								\
			/* With journal recovery, we expect it to play non-TP journal records as non-TP transactions and ZTP	\
			 * journal records as ZTP transactions so we dont expect an implicit TP wrap to be done inside recovery	\
			 * due to a trigger (as this means GT.M and recovery have different values for GVT->gvt_trigger which	\
			 * is not possible). Assert that.									\
			 */													\
			assert(!jgbl.forw_phase_recovery);									\
			LCL_TSTART = TRUE;											\
			 /* 0 ==> save no locals but RESTART OK */ 								\
			op_tstart((IMPLICIT_TSTART + IMPLICIT_TRIGGER_TSTART), TRUE, &literal_batch, 0);			\
			/* Ensure that the op_tstart done above has set up the TP frame and that the first entry is		\
			 * of MVST_TPHOLD type.											\
			 */													\
			assert((tp_pointer->fp == frame_pointer) && (MVST_TPHOLD == mv_chain->mv_st_type)			\
				&& (msp == (unsigned char *)mv_chain));								\
			IS_TPWRAP = TRUE;											\
			assert(!CSA->sgm_info_ptr->tp_set_sgm_done && !CSA->sgm_info_ptr->update_trans);			\
			tp_set_sgm();												\
			/* tp_set_sgm above could modify CSA->db_trigger_cycle (from CSD->db_trigger_cycle). Set local variable	\
			 * cycle to match CSA->db_trigger_cycle so as to pass the updated value to gvtr_init. Also in that	\
			 * case recompute cycle_mismatch (and related variables) now that CSA->db_trigger_cycle changed.	\
			 */													\
			if (cycle != CSA->db_trigger_cycle)									\
			{													\
				cycle = CSA->db_trigger_cycle;									\
				/* Assert that if db_trigger_cycle mismatch was TRUE above,					\
				 * it better be TRUE after 'cycle' update as well.						\
				 */												\
				assert(!db_trigger_cycle_mismatch || (GVT->db_trigger_cycle != cycle));				\
				db_trigger_cycle_mismatch = (GVT->db_trigger_cycle != cycle);					\
				cycle_mismatch = (db_trigger_cycle_mismatch || ztrig_cycle_mismatch);				\
			}													\
			/* An implicit TP wrap is created for an explicit TP update. tp_set_sgm call done above will initalize	\
			 * sgm_info_ptr for this TP transaction and will have sgm_info_ptr->update_trans set to zero. If the 	\
			 * op_tstart done above is for ^#t read, then set_upd_trans_t_err set to TRUE just after gvtr_init will \
			 * take care of resetting si->update_trans and t_err at macro exit. However, if this op_tstart is for	\
			 * the actual transaction (for eg., triggers for this global is already read for this process and an	\
			 * explicit non-tp update is in progress) then we need to set si->update_trans and t_err to correct 	\
			 * values before the macro exit. 									\
			 */													\
			set_upd_trans_t_err = TRUE;										\
			assert(((0 < dollar_tlevel) || (ERR_GVZTRIGFAIL != T_ERR)) && (1)) ;	/* &&(1) idents assert */	\
		} else														\
		{														\
			/* Already in TP */											\
			DEBUG_ONLY(was_nontp = FALSE;)										\
			assert(sgm_info_ptr == CSA->sgm_info_ptr);								\
		}														\
		if (cycle_mismatch)												\
		{	/* Process' trigger view changed. Re-read triggers */							\
			assert(GVT->gd_csa == CSA);										\
			if ((local_tn == GVT->trig_local_tn) && db_trigger_cycle_mismatch)					\
			{	/* Already dispatched trigger for this gvn in this transaction - must restart. But do so ONLY	\
				 * if the process' trigger view changed because of a concurrent trigger load/unload and NOT	\
				 * because of $ZTRIGGER as part of this transaction as that could cause unintended restarts.	\
				 */												\
				assert(!LCL_TSTART);										\
				assert(CDB_STAGNATE > t_tries);									\
				DBGTRIGR((stderr, "GVTR_INIT_AND_TPWRAP_IF_NEEDED: throwing TP restart\n"));			\
				t_retry(cdb_sc_triggermod);									\
			}													\
			DEBUG_ONLY(save_reset_gv_target = reset_gv_target); 							\
			gvtr_init(GVT, cycle, LCL_TSTART, T_ERR);								\
			/* ^#t reads done via gvtr_init will cause t_err to be set to GVGETFAIL which needs to be reset to 	\
			 * T_ERR (incoming parameter to this macro) before leaving the macro. Also, in case of an implicit	\
			 * tstart (LCL_TSTART = TRUE), if a restart happens while reading ^#t global, gvtr_tpwrap_ch will be	\
			 * invoked which inturn will invoke tp_restart that would reset sgm_info_ptr->update_trans to zero. The	\
			 * caller of the macro (gvcst_put and gvcst_kill) relies on sgm_info_ptr->update_trans being non-zero. 	\
			 */													\
			assert(((0 < dollar_tlevel) || (ERR_GVZTRIGFAIL != T_ERR)) && (2)) ;	/* &&(2) idents assert */	\
			set_upd_trans_t_err = TRUE;										\
			/* Check that gvtr_init does not play with "reset_gv_target" a global variable that callers of this	\
			 * function (e.g. gvcst_put) might have set to a non-default value.					\
			 */													\
			assert(reset_gv_target == save_reset_gv_target);							\
			CSD = cs_data; /* if MM and db extension occurred, reset CSD to cs_data to avoid stale value */		\
		}														\
	} 															\
	GVT_TRGR = GVT->gvt_trigger;												\
	assert((NULL == GVT_TRGR) || dollar_tlevel);										\
	if ((NULL == GVT_TRGR) && IS_TPWRAP && !dollar_tlevel)									\
	{															\
		assert(set_upd_trans_t_err);											\
		assert(was_nontp);												\
		assert(0 == t_tries);												\
		/* We came in as a non-tp update and initiated op_tstart to do the ^#t reads. However, no triggers were defined	\
		 * because of which we did the corresponding op_tcommit in gvtr_db_tpwrap_helper. Now the TP wrap is complete	\
		 * restore any global variables the wrap messed with. 								\
		 */														\
		IS_TPWRAP = LCL_TSTART = FALSE;											\
		DEBUG_ONLY(donot_INVOKE_MUMTSTART = FALSE;)									\
	} 															\
	if (set_upd_trans_t_err) /* Reset update_trans/si->update_trans and t_err */						\
	{															\
		/* If non-tp, the below macro will invoke t_begin which will set up the necessary structures for the Non-TP 	\
		 * update													\
		 */														\
		T_BEGIN_SETORKILL_NONTP_OR_TP(T_ERR);										\
	}															\
}

#define	GVTR_OP_TCOMMIT(STATUS)										\
{													\
	GBLREF	boolean_t		skip_INVOKE_RESTART;						\
													\
	DEBUG_ONLY(GBLREF boolean_t	donot_INVOKE_MUMTSTART;)					\
	DEBUG_ONLY(GBLREF tp_frame	*tp_pointer;)							\
	DEBUG_ONLY(GBLREF stack_frame	*frame_pointer;)						\
	DEBUG_ONLY(GBLREF mv_stent	*mv_chain;)							\
	DEBUG_ONLY(GBLREF unsigned char	*msp;)								\
	DEBUG_ONLY(GBLREF uint4		dollar_tlevel;)							\
													\
	assert(dollar_tlevel);										\
	assert((tp_pointer->fp == frame_pointer) && (MVST_TPHOLD == mv_chain->mv_st_type)		\
		&& (msp == (unsigned char *)mv_chain));							\
	assert(!skip_INVOKE_RESTART);									\
	skip_INVOKE_RESTART = TRUE;	/* causes op_tcommit to return code if restart situation */	\
	STATUS = op_tcommit();										\
	assert(!skip_INVOKE_RESTART);	/* should have been reset by op_tcommit at very beginning */	\
	DEBUG_ONLY(if (cdb_sc_normal == STATUS) donot_INVOKE_MUMTSTART = FALSE;)			\
}

#define	PUSH_ZTOLDMVAL_ON_M_STACK(ZTOLD_MVAL, SAVE_MSP, SAVE_MV_CHAIN)						\
{														\
	GBLREF	mv_stent		*mv_chain;								\
	GBLREF	unsigned char		*msp;									\
														\
	/* Create mval on the M-stack (thereby it is known to stp_gcol (BYPASSOK)) to save 			\
	 * pre-gvcst_put/gvcst_kill value. The memory needed to save the actual value will be obtained from the \
	 * stringpool later.											\
	 */													\
	assert(NULL == ZTOLD_MVAL);										\
	SAVE_MSP = msp;		/* Save current msp & mv_chain to restore finally */				\
	SAVE_MV_CHAIN = mv_chain;										\
	PUSH_MV_STENT(MVST_MVAL);	/* protect $ztoldval from stp_gcol (BYPASSOK) */			\
	ZTOLD_MVAL = &mv_chain->mv_st_cont.mvs_mval;								\
	ZTOLD_MVAL->mvtype = 0;	/* make sure mval is setup enough to protect stp_gcol (BYPASSOK)(if invoked 	\
				 * below) from	incorrectly reading its contents until it is fully initialized 	\
				 * later. */									\
}

/* The POP_MVALS_FROM_M_STACK_IF_NEEDED macro below pops some mvals pushed on the stack but pops them in an unusual way for
 * performance reasons (not really popping them but just restoring previous pointers). The debug version of that macro will use the
 * slightly longer but verifying form of unwind defined below to make sure we aren't popping something we need in an invisible and
 * tough to track fashion.
 */
#ifdef DEBUG
#define UNW_MV_STENT_TO(prev_msp, prev_mv_chain)				\
{										\
	mv_stent *mvc;								\
	mvc = mv_chain;								\
	while (mvc < prev_mv_chain)						\
	{									\
		assert(MVST_MVAL == mvc->mv_st_type);				\
		mvc = (mv_stent *)(mvc->mv_st_next + (char *)mvc);		\
	}									\
	assert(prev_mv_chain == mvc);						\
	assert(prev_msp <= (unsigned char *)mvc);				\
	msp = prev_msp;								\
	mv_chain = mvc;								\
}
#else
#define UNW_MV_STENT_TO(prev_msp, prev_mv_chain)			\
{									\
	msp = prev_msp;							\
	mv_chain = prev_mv_chain;					\
}
#endif

#define	POP_MVALS_FROM_M_STACK_IF_NEEDED(ZTOLD_MVAL, SAVE_MSP, SAVE_MV_CHAIN)		\
{											\
	GBLREF	boolean_t		skip_dbtriggers;				\
	GBLREF	mv_stent		*mv_chain;					\
	GBLREF	unsigned char		*msp;						\
											\
	if (NULL != ZTOLD_MVAL)								\
	{	/* ZTOLD_MVAL & potentially a few other mvals have been pushed onto the	\
		 * M-stack. Pop them all in one restore of the M-stack.			\
		 */									\
		assert(!skip_dbtriggers);						\
		assert(SAVE_MSP > msp);							\
		if (SAVE_MSP > msp)							\
			UNW_MV_STENT_TO(SAVE_MSP, SAVE_MV_CHAIN);			\
		ZTOLD_MVAL = NULL;							\
	}										\
}

#define SETUP_TRIGGER_GLOBAL									\
{												\
	hasht_tree = csa->hasht_tree;								\
	if (NULL == hasht_tree)									\
	{	/* Allocate gv_target like structure for "^#t" global in this database file */	\
		gvent.var_name.addr = literal_hasht.str.addr;					\
		gvent.var_name.len = literal_hasht.str.len;					\
		gvent.marked = FALSE;								\
		COMPUTE_HASH_MNAME(&gvent);							\
		hasht_tree = targ_alloc(csa->hdr->max_key_size, &gvent, NULL);			\
		hasht_tree->gd_csa = csa;							\
		csa->hasht_tree = hasht_tree;							\
	}											\
	gv_target = hasht_tree;									\
}

#define INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED										\
{															\
	unsigned char		*key;											\
															\
	key = &gv_currkey->base[0];											\
	memcpy(key, HASHT_GBLNAME, HASHT_GBLNAME_FULL_LEN);	/* including terminating '\0' subscript */		\
	key += HASHT_GBLNAME_FULL_LEN;											\
	*key++ = '\0';		/* double '\0' for terminating key */							\
	gv_currkey->end = HASHT_GBLNAME_FULL_LEN;									\
	/* Determine root block of ^#t global in this database file. Need to use gvcst_root_search for this. It expects	\
	 * gv_currkey, gv_target, gv_cur_region, cs_addrs & cs_data to be set up appropriately. gv_currkey & gv_target	\
	 * are already set up. The remaining should be set up which is asserted below.					\
	 */														\
	assert(&FILE_INFO(gv_cur_region)->s_addrs == csa);								\
	assert(cs_addrs == csa);											\
	assert(cs_data == csa->hdr);											\
	/* Do the actual search for ^#t global in the directory tree */							\
	GVCST_ROOT_SEARCH;												\
}

#define	SWITCH_TO_DEFAULT_REGION			\
{							\
	GBLREF	uint4		dollar_tlevel;		\
							\
	gd_region		*default_region;	\
	gv_namehead		*hasht_tree;		\
	mname_entry		gvent;			\
							\
	assert(NULL != gd_header);			\
	default_region = gd_header->maps->reg.addr;	\
	if (!default_region->open)			\
		gv_init_reg(default_region);		\
	TP_CHANGE_REG_IF_NEEDED(default_region);	\
	csa = cs_addrs;					\
	SETUP_TRIGGER_GLOBAL;				\
	assert(NULL != gv_target);			\
	if (dollar_tlevel)				\
		tp_set_sgm();				\
}

GBLREF	uint4		dollar_tlevel;
GBLREF	int4		gtm_trigger_depth;
GBLREF	int4		tstart_trigger_depth;

/* This macro returns if the current update is an EXPLICIT update or not. Any update done as part of a trigger invocation is not
 * considered an explicit update. Note that it is possible to do a TROLLBACK while inside trigger code. In this case, any updates
 * done after the trollback while still inside the trigger code are considered explicit updates. Hence the seemingly complicated
 * check below. There is a version without the asserts for use ONLY WITH the IS_OK_TO_INVOKE_GVCST_KILL macro where nested asserts
 * dont work well with the C preprocessor.
 */
#define	IS_EXPLICIT_UPDATE	(DBG_ASSERT(!dollar_tlevel || (tstart_trigger_depth <= gtm_trigger_depth))	\
					IS_EXPLICIT_UPDATE_NOASSERT)

#define	IS_EXPLICIT_UPDATE_NOASSERT	(!dollar_tlevel || (tstart_trigger_depth == gtm_trigger_depth))

/* Check if update is inside trigger (implicit update) and to a replicated database. If so check that corresponding triggering
 * update (explicit update) also occurred in a replicated database. If not this is an out-of-design situation as the replicating
 * secondary will see no journal records for this TP transaction (since the triggering update did not get replicated) and so cannot
 * keep the secondary in sync with the primary. In this case, issue an error.
 */
#define	TRIG_CHECK_REPLSTATE_MATCHES_EXPLICIT_UPDATE(REG, CSA)							\
{														\
	GBLREF	boolean_t	explicit_update_repl_state;							\
	GBLREF	gv_key		*gv_currkey;									\
														\
	if (!IS_EXPLICIT_UPDATE && !explicit_update_repl_state && REPL_ALLOWED(CSA))				\
	{													\
		unsigned char	buff[MAX_ZWR_KEY_SZ], *end;							\
														\
		if (0 == (end = format_targ_key(buff, MAX_ZWR_KEY_SZ, gv_currkey, TRUE)))			\
			end = &buff[MAX_ZWR_KEY_SZ - 1];							\
		rts_error_csa(CSA_ARG(CSA) VARLSTCNT(8) ERR_TRIGREPLSTATE, 2, DB_LEN_STR(REG), ERR_GVIS, 2,	\
				end - buff, buff);								\
	}													\
}

#define	TRIG_PROCESS_JNL_STR_NODEFLAGS(NODEFLAGS)			\
{									\
	GBLREF	boolean_t	skip_dbtriggers;			\
	GBLREF	mval		dollar_ztwormhole;			\
									\
	assert(!(JS_NOT_REPLICATED_MASK & NODEFLAGS));			\
	skip_dbtriggers = (NODEFLAGS & JS_SKIP_TRIGGERS_MASK);		\
	if (NODEFLAGS & JS_NULL_ZTWORM_MASK)				\
		dollar_ztwormhole.str.len = 0;				\
}

#define	JNL_FORMAT_ZTWORM_IF_NEEDED(CSA, WRITE_LOGICAL_JNLRECS, JNL_OP, KEY, VAL, ZTWORM_JFB, JFB, JNL_FORMAT_DONE)	\
{															\
	GBLREF	mval		dollar_ztwormhole;									\
	GBLREF	int4		gtm_trigger_depth;									\
	GBLREF	int4		tstart_trigger_depth;									\
	GBLREF	boolean_t	skip_dbtriggers;									\
	GBLREF	boolean_t	explicit_update_repl_state;								\
	GBLREF	uint4		dollar_tlevel;										\
	GBLREF	jnl_gbls_t	jgbl;											\
															\
	uint4			nodeflags;										\
															\
	/* No need to write ZTWORMHOLE journal records for updates inside trigger since those records are not		\
	 * replicated anyway.												\
	 */														\
	assert(dollar_tlevel);	/* tstart_trigger_depth is not usable otherwise */					\
	assert(tstart_trigger_depth <= gtm_trigger_depth);								\
	assert(!skip_dbtriggers); /* we ignore the JS_SKIP_TRIGGERS_MASK bit in nodeflags below because of this */	\
	ZTWORM_JFB = NULL;												\
	if (tstart_trigger_depth == gtm_trigger_depth)									\
	{	/* explicit update so need to write ztwormhole records */						\
		assert(WRITE_LOGICAL_JNLRECS == JNL_WRITE_LOGICAL_RECS(CSA));						\
		nodeflags = 0;												\
		explicit_update_repl_state = REPL_ALLOWED(CSA);								\
		/* Write ZTWORMHOLE records only if replicating since secondary is the only one that cares about it. */	\
		if (explicit_update_repl_state && dollar_ztwormhole.str.len)						\
		{	/* $ZTWORMHOLE is non-NULL. Journal that BEFORE the corresponding SET record. If it is found	\
			 * that the trigger invocation did not REFERENCE it, we will remove this from the list of	\
			 * formatted journal records.									\
			 */												\
			ZTWORM_JFB = jnl_format(JNL_ZTWORM, NULL, &dollar_ztwormhole, 0);				\
			/* Note : ztworm_jfb could be NULL if it was determined that this ZTWORMHOLE record is not	\
			 * needed i.e. if the exact same value of ZTWORMHOLE was already written as part of the		\
			 * previous update in this TP transaction.							\
			 */												\
		}													\
	} else														\
		nodeflags = JS_NOT_REPLICATED_MASK;									\
	assert(!JNL_FORMAT_DONE);											\
	/* Need to write logical SET or KILL journal records irrespective of trigger depth */				\
	if (WRITE_LOGICAL_JNLRECS)											\
	{														\
		nodeflags |= JS_HAS_TRIGGER_MASK;	/* gvt_trigger is non-NULL */					\
		if (!dollar_ztwormhole.str.len)										\
		{													\
			/* Set jgbl.prev_ztworm_ptr to NULL. This is needed so that any subsequent non-null ztwormhole	\
			 * assignment which matches with the ztwormhole value before the NULL ztwormhole SHOULD write	\
			 * ZTWORM records in the journal file. 								\
			 */												\
			jgbl.save_ztworm_ptr = jgbl.prev_ztworm_ptr;							\
			jgbl.prev_ztworm_ptr = NULL;									\
			nodeflags |= JS_NULL_ZTWORM_MASK;								\
		}													\
		/* Insert SET journal record now that ZTWORMHOLE (if any) has been inserted */				\
		JFB = jnl_format(JNL_OP, KEY, VAL, nodeflags);								\
		assert(NULL != JFB);											\
		JNL_FORMAT_DONE = TRUE;											\
	}														\
}

#define	REMOVE_ZTWORM_JFB_IF_NEEDED(ZTWORM_JFB, JFB, SI)								\
{															\
	GBLREF	boolean_t	ztwormhole_used;	/* TRUE if $ztwormhole was used by trigger code */		\
	GBLREF	jnl_gbls_t	jgbl;											\
															\
	jnl_format_buffer	*tmpjfb;										\
															\
	if ((NULL != ZTWORM_JFB) && !ztwormhole_used)									\
	{ 	/* $ZTWORMHOLE was non-zero before the trigger invocation and was never used inside the	trigger. We	\
		 * need to remove the corresponding formatted journal record. We dont free up the memory occupied by	\
		 * ZTWORM_JFB as it is not easy to free up memory in the middle of a buddy list. This memory will	\
		 * anyway be freed up eventually at tp_clean_up time.							\
		 * NOTE: Trigger code that does NOT use the $ZTWORMHOLE is equivalent to a trigger code that has	\
		 * $ZTWORMHOLE set to NULL and hence should have the JS_NULL_ZTWORM_MASK set in the nodeflags. But for	\
		 * that to happen, checksum of the SET/KILL/ZTRIG records need to be recomputed. Since this is a costly	\
		 * operation, we don't touch the nodeflags as there is no known correctness issue.		\
		 */													\
		assert(NULL != JFB);											\
		tmpjfb = ZTWORM_JFB->prev;										\
		if (NULL != tmpjfb)											\
		{													\
			tmpjfb->next = JFB;										\
			JFB->prev = tmpjfb;										\
		} else													\
		{													\
			assert(SI->jnl_head == ZTWORM_JFB);								\
			SI->jnl_head = JFB;										\
			assert(IS_UUPD(JFB->rectype));									\
			assert(((jnl_record *)JFB->buff)->prefix.jrec_type == JFB->rectype);				\
			JFB->rectype--;											\
			assert(IS_TUPD(JFB->rectype));									\
			((jnl_record *)JFB->buff)->prefix.jrec_type = JFB->rectype;					\
		}													\
		jgbl.prev_ztworm_ptr = jgbl.save_ztworm_ptr;								\
		assert(0 < jgbl.cumul_index);										\
		DEBUG_ONLY(jgbl.cumul_index--;)										\
		jgbl.cumul_jnl_rec_len -= ZTWORM_JFB->record_size;							\
	}														\
	jgbl.save_ztworm_ptr = NULL;											\
}

#define SET_PARAM_STRING(UTIL_BUFF, UTIL_LEN, TRIGIDX, PARAM)				\
{											\
	uchar_ptr_t		util_ptr;						\
											\
	util_ptr = i2asc(&UTIL_BUFF[0], TRIGIDX);	/* UTIL_BUFF = 1 */		\
	assert(MAX_TRIG_UTIL_LEN >= STR_LIT_LEN(PARAM));				\
	MEMCPY_LIT(util_ptr, PARAM);			/* UTIL_BUFF = 1,"CMD" */	\
	util_ptr += STR_LIT_LEN(PARAM);							\
	UTIL_LEN = UINTCAST(util_ptr - &UTIL_BUFF[0]);					\
	assert(MAX_TRIG_UTIL_LEN >= UTIL_LEN);						\
}

#endif
