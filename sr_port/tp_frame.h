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

#define TP_MAX_NEST	127

typedef struct tp_var_struct
{
	struct tp_var_struct	*next;
	struct lv_val_struct	*current_value;
	struct lv_val_struct	*save_value;
} tp_var;

typedef struct tp_frame_struct
{
	unsigned int	serial : 1;
	unsigned int	restartable : 1;
	unsigned int	old_locks : 1;
	unsigned int	dlr_t : 1;
	unsigned int	filler : 28;
	unsigned char *restart_pc;
	struct stack_frame_struct	*fp;
	struct mv_stent_struct		*mvc;
	struct gv_namehead_struct	*orig_gv_target;
	struct gv_key_struct		*orig_key;
	struct gd_addr_struct		*gd_header;
	struct symval_struct		*sym;
	tp_var		*vars;
	mval	zgbldir;
	mval	trans_id;
	struct tp_frame_struct *old_tp_frame;
	unsigned char			*restart_ctxt;
} tp_frame;
