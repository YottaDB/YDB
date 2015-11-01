/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "gdskill.h"
#include "cdb_sc.h"
#include "hashdef.h"
#include "lv_val.h"
#include "jnl.h"
#include "mlkdef.h"
#include "mv_stent.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "tp_frame.h"
#include "hashtab.h"          /* needed for tp.h */
#include "buddy_list.h"               /* needed for tp.h */
#include "tp.h"
#include "tp_timeout.h"
#include "op.h"
#include "have_crit.h"
#include <varargs.h>

error_def(ERR_STACKCRIT);
error_def(ERR_STACKOFLOW);
error_def(ERR_TPMIXUP);
error_def(ERR_TPSTACKCRIT);
error_def(ERR_TPSTACKOFLOW);
error_def(ERR_TPTOODEEP);

GBLDEF        char                    jn_tid[8];

GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF	short			dollar_tlevel;
GBLREF	mval			dollar_zgbldir;
GBLREF	gd_addr			*gd_header;
GBLREF	gv_key			*gv_currkey;
GBLREF	gv_namehead		*gv_target;
GBLREF	stack_frame		*frame_pointer;
GBLREF	tp_frame		*tp_pointer;
GBLREF	mv_stent		*mv_chain;
GBLREF	mlk_pvtblk		*mlk_pvt_root;
GBLREF	symval			*curr_symval;
GBLREF	unsigned char		*msp, *stacktop, *stackwarn, *tpstackbase, *tpstacktop, *tpstackwarn,
				*tp_sp, t_fail_hist[CDB_MAX_TRIES];
GBLREF	volatile bool		run_time;
GBLREF  unsigned int		t_tries;
GBLREF	tp_region		*tp_reg_list, *tp_reg_free_list;
GBLREF	trans_num		local_tn;	/* transaction number for THIS PROCESS */
GBLREF	bool			is_standalone;
GBLREF 	void			(*tp_timeout_start_timer_ptr)(int4 tmout_sec);
GBLREF  sgm_info                *first_sgm_info;
GBLREF  uint4			cumul_jnl_rec_len;
GBLREF  global_tlvl_info	*global_tlvl_info_head;
GBLREF  buddy_list		*global_tlvl_info_list;
GBLREF  sgmnt_data_ptr_t	cs_data;
GBLREF  sgmnt_addrs		*cs_addrs;
GBLREF  gd_region		*gv_cur_region;
DEBUG_ONLY(
GBLREF  uint4			cumul_index;
GBLREF  uint4			cu_jnl_index;
)

#define NORESTART -1
#define ALLLOCAL  -2

/* Note gv_orig_key[i] is assigned to tp_pointer->orig_key which then tries to dereference the "begin", "end", "prev", "top"
 * 	fields like it were a gv_currkey pointer. Since these members are 2-byte fields, we need atleast 2 byte alignment.
 * We want to be safer and hence give 4-byte alignment by declaring the array as an array of integers.
 */
static	int4	gv_orig_key[TP_MAX_NEST + 1][DIVIDE_ROUND_UP((sizeof(gv_key) + MAX_KEY_SZ + 1), sizeof(int4))];

/*** temporary ***/
GBLREF int4	    		dollar_zmaxtptime;

void	op_tstart(va_alist)
va_dcl
{
	bool			serial,			/* whether SERIAL keyword was present */
				new;
	ht_entry		*hte, *sym, *symtop;
	int			dollar_t,		/* value of $T when TSTART */
				prescnt,		/* number of names to save, -1 = no restart, -2 = preserve all */
				pres, shift;
	lv_val			*lv, *var;
	mident			vname;
	mlk_pvtblk		*pre_lock;
	mlk_tp			*lck_tp;
	mval			*preserve,		/* list of names to save */
				*tid,			/* transaction id */
				*mvname;
	mv_stent		*mv_st_ent, *mvst_tmp, *mvst_prev;
	stack_frame		*fp, *fp_fix;
	tp_frame		*tf;
	tp_var			*restore_ent;
	unsigned char		*old_sp, *top, *tstack_ptr;
	va_list			varlst, lvname;
	tp_region		*tr, *tr_next;
	sgm_info		*si;
	int4			tmout_sec;
	tlevel_info		*tli, *new_tli, *prev_tli;
	global_tlvl_info	*gtli, *new_gtli, *prev_gtli;
	kill_set		*ks, *prev_ks;
	jnl_format_buffer 	*jfb, *prev_jfb;

	if (0 != jnl_fence_ctl.level)
		rts_error(VARLSTCNT(4) ERR_TPMIXUP, 2, "An M", "a fenced logical");
	if (dollar_tlevel + 1 >= TP_MAX_NEST)
		rts_error(VARLSTCNT(1) ERR_TPTOODEEP);
	if (0 == dollar_tlevel)
	{
		jnl_fence_ctl.region_count = 0;
		jnl_fence_ctl.fence_list = (sgmnt_addrs *)-1;
		cumul_jnl_rec_len = 0;
		DEBUG_ONLY(cumul_index = cu_jnl_index = 0;)
		t_tries = (FALSE == is_standalone) ? 0 : CDB_STAGNATE;
		t_fail_hist[t_tries] = cdb_sc_normal;
		/* ensure that we don't have crit on any region at the beginning of a TP transaction (be it GT.M or MUPIP) */
		assert(0 == have_crit(CRIT_HAVE_ANY_REG));
		for (tr = tp_reg_list; NULL != tr; tr = tr_next)
		{	/* start with empty list, place all existing entries on free list */
			tp_reg_list = tr_next = tr->fPtr;	/* Remove from queue */
			tr->fPtr = tp_reg_free_list;
			tp_reg_free_list = tr; 			/* Place on free queue */
		}
		++local_tn;					/* Begin new local transaction */
	}
	va_start(varlst);
	dollar_t = va_arg(varlst, int);
	serial = va_arg(varlst, int);
	tid = va_arg(varlst, mval *);

/* To take the case of transaction id in extracting journal file */
	if ((0 == dollar_tlevel) && (0 != tid->str.len))
	{

		if (tid->str.len >= sizeof(jn_tid))
			tid->str.len = sizeof(jn_tid) - 1;
		memcpy(jn_tid, (char *)tid->str.addr, tid->str.len);
		jn_tid[tid->str.len] = '\0';
	}

	prescnt = va_arg(varlst, int);
	MV_FORCE_STR(tid);
	if (prescnt > 0)
	{
		VAR_COPY(lvname, varlst);
		for (pres = 0;  pres < prescnt;  ++pres)
		{
			preserve = va_arg(lvname, mval *);
			MV_FORCE_STR(preserve);
		}
	}
	if (NULL == gd_header)
		gvinit();
	if (frame_pointer->old_frame_pointer->type & SFT_DM)
	{	/* Put a TPHOLD underneath dmode frame */
		assert(frame_pointer->old_frame_pointer->old_frame_pointer);
		fp = frame_pointer->old_frame_pointer->old_frame_pointer;
		top = (unsigned char *)(frame_pointer->old_frame_pointer + 1);
		old_sp = msp;
		msp -= mvs_size[MVST_TPHOLD];
		if (msp <= stackwarn)
		{
			if (msp <= stacktop)
			{
				msp = old_sp;
				rts_error(VARLSTCNT(1) ERR_STACKOFLOW);
			} else
				rts_error(VARLSTCNT(1) ERR_STACKCRIT);
		}
		memcpy(msp, old_sp, top - (unsigned char *)old_sp);
		mv_st_ent = (mv_stent *)(top - mvs_size[MVST_TPHOLD]);
		mv_st_ent->mv_st_type = MVST_TPHOLD;
		frame_pointer = (stack_frame *)((char *)frame_pointer - mvs_size[MVST_TPHOLD]);
		for (fp_fix = frame_pointer;  fp_fix != fp;  fp_fix = fp_fix->old_frame_pointer)
		{
			if ((unsigned char *)fp_fix->l_symtab < top  &&  (unsigned char *)fp_fix->l_symtab > stacktop)
				fp_fix->l_symtab = (mval **)((char *)fp_fix->l_symtab - mvs_size[MVST_TPHOLD]);
			if (fp_fix->temps_ptr < top  &&  fp_fix->temps_ptr > stacktop)
				fp_fix->temps_ptr -= mvs_size[MVST_TPHOLD];
			if (fp_fix->vartab_ptr < (char *)top  &&  fp_fix->vartab_ptr > (char *)stacktop)
				fp_fix->vartab_ptr -= mvs_size[MVST_TPHOLD];
			if ((unsigned char *)fp_fix->old_frame_pointer < top  &&
			   (char *)fp_fix->old_frame_pointer > (char *)stacktop)
				fp_fix->old_frame_pointer = (stack_frame *)((char *)fp_fix->old_frame_pointer
								- mvs_size[MVST_TPHOLD]);
		}
		if ((unsigned char *)mv_chain >= top)
		{
			mv_st_ent->mv_st_next = (char *)mv_chain - (char *)mv_st_ent;
			mv_chain = mv_st_ent;
		} else
		{
			top -= mvs_size[MVST_TPHOLD] + sizeof(stack_frame);
			mv_chain = (mv_stent *)((char *)mv_chain - mvs_size[MVST_TPHOLD]);
			mvst_tmp = mv_chain;
			mvst_prev = (mv_stent *)((char *)mvst_tmp + mvst_tmp->mv_st_next);
			while (mvst_prev < (mv_stent *)top)
			{
				mvst_tmp = mvst_prev;
				mvst_prev = (mv_stent *)((char *)mvst_tmp + mvst_tmp->mv_st_next);
			}
			mvst_tmp->mv_st_next = (char *)mv_st_ent - (char *)mvst_tmp;
			mv_st_ent->mv_st_next = (char *)mvst_prev - (char *)mv_st_ent + mvs_size[MVST_TPHOLD];
		}
		shift = mvs_size[MVST_TPHOLD];
	} else
	{
		PUSH_MV_STENT(MVST_TPHOLD);
		mv_st_ent = mv_chain;
		fp = frame_pointer;
		shift = 0;
	}
	mv_st_ent->mv_st_cont.mvs_tp_holder = dollar_tlevel;
	if (NULL == tpstackbase)
	{
		tstack_ptr = (unsigned char *)malloc(32768);
		tp_sp = tpstackbase
		      = tstack_ptr + 32764;
		tpstacktop = tstack_ptr;
		tpstackwarn = tpstacktop + 1024;
		tp_pointer = NULL;
	}
	pres = prescnt;
	if ((0 > pres) || (0 != dollar_tlevel))
		/* If no saves or a TSTART is already active, don't save names */
		pres = 0;
	tf = (tp_frame *)(tp_sp -= sizeof(tp_frame) + sizeof(mident) * pres);
	if (tp_sp < tpstackwarn)
		rts_error(VARLSTCNT(1) tp_sp < tpstacktop ? ERR_TPSTACKOFLOW : ERR_TPSTACKCRIT);
	tf->dlr_t = dollar_t;
	tf->restart_pc = fp->mpc;
	tf->restart_ctxt = fp->ctxt;
	tf->fp = fp;
	tf->serial = serial;
	tf->trans_id = *tid;
	tf->restartable = (NORESTART != prescnt);
	tf->old_locks = (NULL != mlk_pvt_root);
	tf->orig_gv_target = gv_target;
	tf->orig_key = (gv_key *)&gv_orig_key[dollar_tlevel][0];
	memcpy(tf->orig_key, gv_currkey, sizeof(gv_key) + gv_currkey->end);
	tf->gd_header = gd_header;
	tf->zgbldir = dollar_zgbldir;
	tf->sym = (symval *)NULL;
	tf->vars = (tp_var *)NULL;
	tf->old_tp_frame = tp_pointer;
	tp_pointer = tf;
	if (prescnt > 0)
	{
		VAR_COPY(lvname, varlst);
		for (pres = 0;  pres < prescnt;  ++pres)
		{
			preserve = va_arg(lvname, mval *);
			mvname = (mval *)((char *)preserve - shift);
			if (0 != mvname->str.len)
			{	/* Convert mval to mident and see if it's in the symbol table */
				memset(&vname, 0, sizeof(vname));
				memcpy(vname.c, mvname->str.addr,
				       mvname->str.len < sizeof(vname) ? mvname->str.len : sizeof(vname));
				hte = ht_put(&curr_symval->h_symtab, (mname *)&vname, &new);
				if (new)
					lv_newname(hte, curr_symval);
				lv = (lv_val *)hte->ptr;
				/* In order to allow restart of a sub-transaction, this should chain rather than back stop,
				   with appropriate changes to tp_var_clone and tp_unwind */
				if (NULL == lv->tp_var)
				{
					var = lv_getslot(lv->ptrs.val_ent.parent.sym);
					restore_ent = (tp_var *)malloc(sizeof(tp_var));
					restore_ent->current_value = lv;
					restore_ent->save_value = var;
					restore_ent->next = tf->vars;
					lv->tp_var = var;
					*var = *lv;
					tf->vars = restore_ent;
				}
			}
		}
	}
	else if (ALLLOCAL == prescnt)
	{	/* Preserve all variables */
		tf->sym = curr_symval;
		++curr_symval->tp_save_all;
		for (sym = curr_symval->h_symtab.base, symtop = sym + curr_symval->h_symtab.size;  sym < symtop;  ++sym)
		{
			if ('\0' != sym->nb.txt[0])
			{
				lv = (lv_val *)sym->ptr;
				if (NULL == lv->tp_var)
				{
					var = lv_getslot(lv->ptrs.val_ent.parent.sym);
					restore_ent = (tp_var *)malloc(sizeof(tp_var));
					restore_ent->current_value = lv;
					restore_ent->save_value = var;
					restore_ent->next = tf->vars;
					lv->tp_var = var;
					*var = *lv;
					tf->vars = restore_ent;
				}
			}
		}
	}
	/* Store existing state of locks */
	for (pre_lock = mlk_pvt_root;  NULL != pre_lock;  pre_lock = pre_lock->next)
	{
		if ((NULL == pre_lock->tp) || (pre_lock->tp->level != pre_lock->level)
			|| (pre_lock->tp->zalloc != pre_lock->zalloc))
		{	/* Only stack locks that have changed since last TSTART */
			lck_tp = (mlk_tp *)malloc(sizeof(mlk_tp));
			lck_tp->tplevel = dollar_tlevel;
			lck_tp->level = pre_lock->level;
			lck_tp->zalloc = pre_lock->zalloc;
			lck_tp->next = pre_lock->tp;
			pre_lock->tp = lck_tp;
		}
	}
	++dollar_tlevel;
	/* Store the global (across all segments) dollar_tlevel specific information. Curently, it holds only jnl related info. */
	if ((sgmnt_addrs *)-1 != jnl_fence_ctl.fence_list)
	{
		for (prev_gtli = NULL, gtli = global_tlvl_info_head; gtli; gtli = gtli->next_global_tlvl_info)
			prev_gtli = gtli;
		new_gtli = (global_tlvl_info *)get_new_element(global_tlvl_info_list, 1);
		new_gtli->global_tlvl_fence_info = jnl_fence_ctl.fence_list;
		new_gtli->t_level = dollar_tlevel;
		new_gtli->next_global_tlvl_info = NULL;
		if (prev_gtli)
		{
			assert(prev_gtli->t_level + 1 == dollar_tlevel);
			prev_gtli->next_global_tlvl_info = new_gtli;
		} else
			global_tlvl_info_head = new_gtli;
	}
	/* Store the dollar_tlevel specific information */
	for (si = first_sgm_info; NULL != si; si = si->next_sgm_info)
	{
		for (prev_tli = NULL, tli = si->tlvl_info_head; tli; tli = tli->next_tlevel_info)
			prev_tli = tli;
		new_tli = (tlevel_info *)get_new_element(si->tlvl_info_list, 1);
		new_tli->t_level = dollar_tlevel;
		for (prev_ks = NULL, ks = si->kill_set_head; ks; ks = ks->next_kill_set)
			prev_ks = ks;
		new_tli->tlvl_kill_set = prev_ks;
		if (prev_ks)
			new_tli->tlvl_kill_used = prev_ks->used;
		else
			new_tli->tlvl_kill_used = 0;
		new_tli->tlvl_tp_hist_info = si->last_tp_hist;
		if (JNL_ENABLED(&FILE_INFO(si->gv_cur_region)->s_addrs))
		{
			for (prev_jfb = NULL, jfb = si->jnl_head; jfb; jfb = jfb->next)
				prev_jfb = jfb;
			new_tli->tlvl_jfb_info = prev_jfb;
			new_tli->tlvl_cumul_jrec_len = cumul_jnl_rec_len;
			DEBUG_ONLY(new_tli->tlvl_cumul_index = cumul_index;)
		}
		new_tli->next_tlevel_info = NULL;
		if (prev_tli)
			prev_tli->next_tlevel_info = new_tli;
		else
			si->tlvl_info_head = new_tli;
	}
	/* ------------------------------------------------------------------
	 * Start TP timer if set to non-default value
	 * ------------------------------------------------------------------
	 */
	tmout_sec = dollar_zmaxtptime;
	assert(0 <= tmout_sec);
	if (0 < tmout_sec)
	{
		/* ------------------------------------------------------------------
		 * Only start timer at outer level
		 * ------------------------------------------------------------------
		 */
		if (1 == dollar_tlevel)
		{
			(*tp_timeout_start_timer_ptr)(tmout_sec);
		} else
		{
			/* Confirm that timeout already pending, else warning */
		}
	}
}
