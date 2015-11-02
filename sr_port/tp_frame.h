/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#define TP_MAX_NEST	127

/* Macro to put a given lv_val on the TP local var restore list for the current
   tp frame.
*/
#define TP_SAVE_RESTART_VAR(lv, tf, mnamekey)									\
	{													\
		lv_val *var;											\
		tp_var *restore_ent;										\
		var = lv_getslot((lv)->ptrs.val_ent.parent.sym);						\
		assert(var);											\
		restore_ent = (tp_var *)malloc(sizeof(tp_var));							\
		restore_ent->current_value = (lv);								\
		restore_ent->save_value = var;									\
		memcpy(&restore_ent->key, (mnamekey), sizeof(mname_entry));					\
		restore_ent->var_cloned = FALSE;								\
		restore_ent->next = (tf)->vars;									\
		assert(NULL == (lv)->tp_var);									\
		(lv)->tp_var = restore_ent;									\
		*var = *(lv);											\
		INCR_CREFCNT(lv);	/* First increment for restore_ent ref to prevent deletion */		\
		INCR_TREFCNT(lv);										\
		assert(1 < (lv)->stats.trefcnt);								\
		assert(0 < (lv)->stats.crefcnt);								\
		(tf)->vars = restore_ent;									\
	}

/* tp_var_struct moved to lv_val.h for include heirarchy purposes */

typedef struct tp_frame_struct
{
	unsigned int			serial : 1;
	unsigned int			restartable : 1;
	unsigned int			old_locks : 1;
	unsigned int			dlr_t : 1;
	unsigned int			tp_save_all_flg : 1;
	unsigned int			filler : 27;
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
