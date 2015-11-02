/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Definitions for implicit_flag passed to op_tstart. Combinations are or'd (or added) to form a bit-mask */
#define NORMAL_MCODE_TSTART	0	/* Nothing implicit about it - this is what the assembler opp_tstart()
					 * routines pass.
					 */
#define IMPLICIT_TSTART		1	/* For both trigger and non-trigger related implicit tstarts. Any direct
					 * op_tstart done in C code should at least use this flag. This flag
					 * is inherited by nested levels.
					 */
#define IMPLICIT_TRIGGER_TSTART	2	/* This TP frame was implicitly created for a trigger allowing slightly
					 * different error handling for TP restarts under some conditions. This
					 * flag is inherited by nested levels.
					 */

/* Macro to put a given lv_val on the TP local var restore list for the current tp frame. */
#define TP_SAVE_RESTART_VAR(lv, tf, mnamekey)							\
{												\
	lv_val	*var;										\
	tp_var	*restore_ent;									\
												\
	var = lv_getslot(LV_GET_SYMVAL(lv));							\
	assert(var);										\
	restore_ent = (tp_var *)malloc(SIZEOF(tp_var));						\
	restore_ent->current_value = (lv);							\
	restore_ent->save_value = var;								\
	memcpy(&restore_ent->key, (mnamekey), SIZEOF(mname_entry));				\
	restore_ent->var_cloned = FALSE;							\
	restore_ent->next = (tf)->vars;								\
	assert(NULL == (lv)->tp_var);								\
	(lv)->tp_var = restore_ent;								\
	*var = *(lv);										\
	LV_CHILD(var) = NULL;	/* initialize child to NULL until actual cloning occurs.	\
				 * this is needed so stp_gcol does not DOUBLE count subtree */	\
	/* Increment refcnts (due to "restore_ent->save_value") to prevent deletion		\
	 * of "lv" through a "KILL *". Necessary because we need the "lv_val" of "lv"		\
	 * untouched (i.e. not freed and/or reused after a "kill *").				\
	 */											\
	INCR_CREFCNT(lv);									\
	INCR_TREFCNT(lv);									\
	assert(1 < (lv)->stats.trefcnt);							\
	assert(0 < (lv)->stats.crefcnt);							\
	(tf)->vars = restore_ent;								\
}

/* tp_var_struct moved to lv_val.h for include heirarchy purposes */

typedef struct tp_frame_struct
{
	unsigned int			serial : 1;
	unsigned int			restartable : 1;
	unsigned int			old_locks : 1;
	unsigned int			dlr_t : 1;
	unsigned int			tp_save_all_flg : 1;
	unsigned int			implicit_tstart : 1;	/* TRUE if op_tstart was invoked by gvcst_put/gvcst_kill as part of
								 * trigger processing. Field is inherited across nested op_tstarts
								 */
	unsigned int			cannot_commit : 1;	/* TRUE if at least one explicit (i.e. triggering) update in this
								 * transaction ended up dropping back through the trigger boundary
								 * with an unhandled error. In this case, we might have a bunch of
								 * partly completed database work that, if committed, would break
								 * the atomicity of triggers (an update and its triggers should go
								 * either all together or none). It is ok to rollback such partial
								 * transactions. Only commit is disallowed. This field is currently
								 * maintained only on trigger supported platforms.
								 */
	unsigned int			implicit_trigger : 1;	/* The implicit tstart is for a trigger which means some minor
								 * deviations in dealing with TP restarts in some circumstances.
								 */
	unsigned int			filler : 24;
	unsigned char 			*restart_pc;
	struct stack_frame_struct	*fp;
	struct mv_stent_struct		*mvc;
	struct gv_namehead_struct	*orig_gv_target;
	struct gv_key_struct		*orig_key;
	struct gd_addr_struct		*gd_header;
	struct gd_region_struct		*gd_reg;
	struct symval_struct		*sym;
	struct tp_var_struct		*vars;
	mval				zgbldir;
	mval				trans_id;
	struct tp_frame_struct 		*old_tp_frame;
	unsigned char			*restart_ctxt;
} tp_frame;
