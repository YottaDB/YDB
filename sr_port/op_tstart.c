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

#include <stdarg.h>
#include <stddef.h>		/* for OFFSETOF macro */
#include "gtm_string.h"
#include "gtm_stdio.h"

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
#include "lv_val.h"
#include "jnl.h"
#include "mlkdef.h"
#include <rtnhdr.h>
#include "mv_stent.h"
#include "stack_frame.h"
#include "tp_frame.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h and cws_insert.h */
#include "tp.h"
#include "tp_timeout.h"
#include "op.h"
#include "have_crit.h"
#include "gtm_caseconv.h"
#include "gvcst_protos.h"	/* for gvcst_tp_init prototype */
#include "dpgbldir.h"
#include "longset.h"		/* needed for cws_insert.h */
#include "cws_insert.h"		/* for cw_stagnate_reinitialized */
#include "alias.h"
#ifdef GTM_TRIGGER
#include "gtm_trigger_trc.h"
#endif

#ifdef UNICODE_SUPPORTED
#include "gtm_icu_api.h"	/* needed by *TYPEMASK* macros defined in gtm_utf8.h */
#include "gtm_utf8.h"
#endif

error_def(ERR_STACKCRIT);
error_def(ERR_STACKOFLOW);
error_def(ERR_TPMIXUP);
error_def(ERR_TPTOODEEP);

GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF	uint4			dollar_tlevel;
GBLREF	boolean_t		dollar_truth;
GBLREF	mval			dollar_zgbldir;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	gd_addr			*gd_header;
GBLREF	gv_key			*gv_currkey;
GBLREF	gv_namehead		*gv_target;
GBLREF	stack_frame		*frame_pointer;
GBLREF	tp_frame		*tp_pointer;
GBLREF	mv_stent		*mv_chain;
GBLREF	mlk_pvtblk		*mlk_pvt_root;
GBLREF	symval			*curr_symval;
GBLREF	unsigned char		*msp, *stacktop, *stackwarn, *tpstackbase, *tpstacktop, *tp_sp, t_fail_hist[CDB_MAX_TRIES];
GBLREF  unsigned int		t_tries;
GBLREF	tp_region		*tp_reg_list, *tp_reg_free_list;
GBLREF	trans_num		local_tn;		/* transaction number for THIS PROCESS */
GBLREF	trans_num		tstart_local_tn;	/* copy of global variable "local_tn" at op_tstart time */
GBLREF	boolean_t		mupip_jnl_recover;
GBLREF 	void			(*tp_timeout_start_timer_ptr)(int4 tmout_sec);
GBLREF  sgm_info                *first_sgm_info;
GBLREF  global_tlvl_info	*global_tlvl_info_head;
GBLREF  buddy_list		*global_tlvl_info_list;
GBLREF  sgmnt_data_ptr_t	cs_data;
GBLREF  sgmnt_addrs		*cs_addrs;
GBLREF  gd_region		*gv_cur_region;
GBLREF	struct_jrec_tcom	tcom_record;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	boolean_t		tp_in_use;
GBLREF	boolean_t		gtm_utf8_mode;
GBLREF	uint4			tstartcycle;
GBLREF	char			*update_array_ptr;
GBLREF	ua_list			*curr_ua, *first_ua;
#ifdef GTM_TRIGGER
GBLREF	mval			dollar_ztwormhole;
GBLREF	int4			gtm_trigger_depth;
GBLREF	int4			tstart_trigger_depth;
GBLREF	mval			dollar_ztslate;
LITREF	mval			literal_null;
#endif
#ifdef VMS
GBLREF	boolean_t		tp_has_kill_t_cse; /* cse->mode of kill_t_write or kill_t_create got created in this transaction */
#endif

#define NORESTART -1
#define ALLLOCAL  -2
#define TP_STACK_SIZE ((TP_MAX_NEST + 1) * SIZEOF(tp_frame))	/* Size of TP stack frame with no-overflow pad */

void	op_tstart(int implicit_flag, ...) /* value of $T when TSTART */
{
	boolean_t		serial;			/* whether SERIAL keyword was present */
	int			prescnt,		/* number of names to save, -1 = no restart, -2 = preserve all */
				pres;
	lv_val			*lv;
	mlk_pvtblk		*pre_lock;
	mlk_tp			*lck_tp;
	mval			*preserve,		/* list of names to save */
				*tid,			/* transaction id */
				*mvname;
	mv_stent		*mv_st_ent, *mvst_tmp, *mvst_prev;
	stack_frame		*fp, *fp_fix;
	tp_frame		*tf;
	unsigned char		*old_sp, *top, *tstack_ptr, *ptrstart, *ptrend, *ptrinvalidbegin;
	va_list			varlst, lvname;
	tp_region		*tr, *tr_next;
	sgm_info		*si;
	tlevel_info		*tli, *new_tli, *prev_tli;
	global_tlvl_info	*gtli, *new_gtli, *prev_gtli;
	kill_set		*ks, *prev_ks;
	jnl_format_buffer 	*jfb, *prev_jfb;
	gd_region		*r_top, *r_local;
	gd_addr			*addr_ptr;
	unsigned char		tp_bat[TP_BATCH_LEN];
	mname_entry		tpvent;
	ht_ent_mname		*tabent, *curent, *topent;
	sgmnt_addrs		*csa;
	int4			shift_size;
	boolean_t		tphold_noshift = FALSE, implicit_tstart;
	GTMTRIG_ONLY(boolean_t	implicit_trigger;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	implicit_tstart = 0 != (implicit_flag & IMPLICIT_TSTART);
	GTMTRIG_ONLY(implicit_trigger = 0 != (implicit_flag & IMPLICIT_TRIGGER_TSTART));
	GTMTRIG_ONLY(assert(!implicit_trigger || (implicit_trigger && implicit_tstart)));
	GTMTRIG_ONLY(DBGTRIGR((stderr, "op_tstart: Entered - dollar_tlevel: %d, implicit_flag: %d\n",
			       dollar_tlevel, implicit_flag)));
	if (implicit_tstart)
		/* An implicit op_tstart is being done. In this case, even if we are in direct mode, we want to do
		 * regular TPHOLD processing (no setting of tphold in the parent frame and shifting of all mv_stents).
		 * This is ok because the life of the TP transaction will be done before the implicit operation()s are done
		 * done so it will not persist across M lines like it normally would in direct mode.
		 */
		tphold_noshift = TRUE;
	/* If we haven't done any TP until now, turn the flag on to tell gvcst_init to
	   initialize it in any regions it opens from now on and initialize it in any
	   regions that are already open.
	*/
	if (!tp_in_use)
	{
		tp_in_use = TRUE;
		for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
		{
			for (r_local = addr_ptr->regions, r_top = r_local + addr_ptr->n_regions; r_local < r_top; r_local++)
			{
				if (r_local->open && !r_local->was_open &&
				    (dba_bg == r_local->dyn.addr->acc_meth || dba_mm == r_local->dyn.addr->acc_meth))
				{	/* Let's initialize those regions but only if it came through gvcst_init_sysops
					   (being a bg or mm region)
					*/
					gvcst_tp_init(r_local);
				}
			}
		}
	}
	if (0 != jnl_fence_ctl.level)
		rts_error(VARLSTCNT(4) ERR_TPMIXUP, 2, "An M", "a fenced logical");
	if (dollar_tlevel + 1 >= TP_MAX_NEST)
		rts_error(VARLSTCNT(1) ERR_TPTOODEEP);
	va_start(varlst, implicit_flag);	/* no argument count first */
	serial = va_arg(varlst, int);
	tid = va_arg(varlst, mval *);
	prescnt = va_arg(varlst, int);
	MV_FORCE_STR(tid);
	if (!dollar_tlevel)
	{
		jnl_fence_ctl.fence_list = JNL_FENCE_LIST_END;
		jgbl.cumul_jnl_rec_len = 0;
#		ifdef DEBUG
		if (1 == jgbl.cumul_index)	/* when 1 == jgbl.cumul_index and  0 == jgbl.cumul_index, non-TP morphed into TP */
			jgbl.cumul_index = 0;
		else
			assert(0 == jgbl.cumul_index);
		TREF(tp_restart_dont_counts) = 0;
#		endif
		assert(0 == jgbl.cu_jnl_index);
		GTMTRIG_ONLY(memcpy(&dollar_ztslate, &literal_null, SIZEOF(mval)));	/* Zap $ZTSLate at start of lvl 1 trans */
		GTMTRIG_ONLY(if (!implicit_tstart || !implicit_trigger))
		{	/* This is the path for all non-implicit-trigger type TP fences including the implicit fences
			 * created by the update process and by mupip recover forward.
			 * Note: For recover/rollback, t_tries is set to CDB_STAGNATE because unlike Non-TP restarts, TP restarts
			 * need more context to determine the restart point which is non-trivial for recover/rollback since the
			 * updates are done with journal records extracted by sequentially reading the journal file.
			 */
			t_tries = (FALSE == mupip_jnl_recover) ? 0 : CDB_STAGNATE;
			t_fail_hist[t_tries] = cdb_sc_normal;
			/* ensure that we don't have crit on any region at the beginning of a TP transaction (be it GT.M or MUPIP).
			 * The only exception is ONLINE ROLLBACK which holds crit for the entire duration
			 */
			assert(0 == have_crit(CRIT_HAVE_ANY_REG) UNIX_ONLY(|| jgbl.onlnrlbk));
		}
#		ifdef GTM_TRIGGER
		else
		{
			/* This is an implicit TP wrap created for an explicit update. In such case, we do not want to reset
			 * t_tries for below reasons:
			 * (a) If an explicit non-tp update undergoes 3 restarts (t_tries = 3) and on the final retry gvcst_put
			 * sees that triggers are defined (due to concurrent $ZTRIGGER or MUPIP TRIGGER), it would have invoked
			 * op_tstart to create a TP wrap in final retry in which case we should NOT reset t_tries to zero as we
			 * would be holding crit at that time.
			 *
			 * (b) If an explicit non-tp update underwent 1 restart and on the next try(t_tries = 1), gvcst_put
			 * sees that triggers are defined (due to concurrent $ZTRIGGER or MUPIP TRIGGER), it would have invoked
			 * op_tstart to create a TP wrap in which case we should NOT reset t_tries. In the event, we did the
			 * reset of t_tries and no triggers were defined for the update in question, op_tcommit would be done
			 * and we will be back in non-TP BUT with t_tries reset to 0. The above can continue in a cycle causing
			 * a live spin-lock that does not let the final-retry optimistic -> pessimistic concurrency scheme from
			 * kicking in at all.
			 */
			 /* recovery logic does not invoke triggers */
			assert(!(SFF_IMPLTSTART_CALLD & frame_pointer->flags) || (FALSE == mupip_jnl_recover));
		}
#		endif
		for (tr = tp_reg_list; NULL != tr; tr = tr_next)
		{	/* start with empty list, place all existing entries on free list */
			tp_reg_list = tr_next = tr->fPtr;	/* Remove from queue */
			tr->fPtr = tp_reg_free_list;
			tp_reg_free_list = tr; 			/* Place on free queue */
		}
		++local_tn;					/* Begin new local transaction */
		tstart_local_tn = local_tn;
		/* In journal recovery forward phase, we set jgbl.tp_ztp_jnl_upd_num to whatever update_num the journal record
		 * has so it is ok for the global variable to be a non-zero value at the start of a TP transaction (possible if
		 * ZTP of one process is in progress when TP of another process starts in the journal file). But otherwise
		 * (in GT.M runtime) we expect it to be 0 at beginning of each TP or ZTP.
		 */
		assert((0 == jgbl.tp_ztp_jnl_upd_num) || jgbl.forw_phase_recovery);
		INCR_TSTARTCYCLE;
		jgbl.wait_for_jnl_hard = TRUE;
		GTMTRIG_ONLY(
			assert(NULL == jgbl.prev_ztworm_ptr);	/* should have been cleared by tp_clean_up of previous TP */
			assert(NULL == jgbl.save_ztworm_ptr);
				/* should have been NULL almost always except for a small window in gvcst_put/gvcst_kill */
			tstart_trigger_depth = gtm_trigger_depth; /* note down what trigger depth an outermost tstart occurs in */
		)
		memset(tcom_record.jnl_tid, 0, TID_STR_SIZE);
		if (0 != tid->str.len)
		{
			if (!gtm_utf8_mode)
			{
				if (tid->str.len > TID_STR_SIZE)
					tid->str.len = TID_STR_SIZE;
			}
#			ifdef UNICODE_SUPPORTED
			else
			{	/* In UTF8 mode, take only as many valid multi-byte characters as can fit in TID_STR_SIZE */
				if (gtm_utf8_mode)
				{
					MV_FORCE_LEN(tid); /* issues BADCHAR error if appropriate */
					if (tid->str.len > TID_STR_SIZE)
					{
						ptrstart = (unsigned char *)tid->str.addr;
						ptrend = ptrstart + TID_STR_SIZE;
						UTF8_LEADING_BYTE(ptrend, ptrstart, ptrinvalidbegin)
						tid->str.len = INTCAST(ptrinvalidbegin - ptrstart);
					}
				}
			}
#			endif
			assert(TID_STR_SIZE >= tid->str.len);
			memcpy(tcom_record.jnl_tid, (char *)tid->str.addr, tid->str.len);
			if ((TP_BATCH_SHRT == tid->str.len) || (TP_BATCH_LEN == tid->str.len))
			{
				lower_to_upper(tp_bat, (uchar_ptr_t)tid->str.addr, (int)tid->str.len);
				if (0 == memcmp(TP_BATCH_ID, tp_bat, tid->str.len))
					jgbl.wait_for_jnl_hard = FALSE;
			}
		}
		VMS_ONLY(tp_has_kill_t_cse = FALSE;)
		assert(!TREF(donot_commit));
	}
	/* either cw_stagnate has not been initialized at all or previous-non-TP or tp_hist should have done CWS_RESET */
	assert((0 == cw_stagnate.size) || cw_stagnate_reinitialized);
	if (prescnt > 0)
	{
		VAR_COPY(lvname, varlst);
		for (pres = 0;  pres < prescnt;  ++pres)
		{
			preserve = va_arg(lvname, mval *);
			assert(MV_IS_STRING(preserve));		/* Check if this loop can be eliminated */
			MV_FORCE_STR(preserve);
		}
		va_end(lvname);
	}
	if (NULL == gd_header)
		gvinit();
	assert(NULL != gd_header);
	if (!tphold_noshift && (SFT_DM & frame_pointer->old_frame_pointer->type))
	{	/* Put a TPHOLD underneath dmode frame */
		assert(frame_pointer->old_frame_pointer->old_frame_pointer);
		fp = frame_pointer->old_frame_pointer->old_frame_pointer;
		top = (unsigned char *)(frame_pointer->old_frame_pointer + 1);
		old_sp = msp;
		shift_size = mvs_size[MVST_TPHOLD];
		msp -= shift_size;
		if (msp <= stackwarn)
		{
			va_end(varlst);
			if (msp <= stacktop)
			{
				msp = old_sp;
				rts_error(VARLSTCNT(1) ERR_STACKOFLOW);
			} else
				rts_error(VARLSTCNT(1) ERR_STACKCRIT);
		}
		memmove(msp, old_sp, top - (unsigned char *)old_sp);	/* Shift stack w/possible overlapping ranges */
		mv_st_ent = (mv_stent *)(top - shift_size);
		mv_st_ent->mv_st_type = MVST_TPHOLD;
		ADJUST_FRAME_POINTER(frame_pointer, shift_size);
		for (fp_fix = frame_pointer;  fp_fix != fp;  fp_fix = fp_fix->old_frame_pointer)
		{
			if ((unsigned char *)fp_fix->l_symtab < top  &&  (unsigned char *)fp_fix->l_symtab > stacktop)
				fp_fix->l_symtab = (ht_ent_mname **)((char *)fp_fix->l_symtab - shift_size);
			if (fp_fix->temps_ptr < top  &&  fp_fix->temps_ptr > stacktop)
				fp_fix->temps_ptr -= shift_size;
			if (fp_fix->vartab_ptr < (char *)top  &&  fp_fix->vartab_ptr > (char *)stacktop)
				fp_fix->vartab_ptr -= shift_size;
			if ((unsigned char *)fp_fix->old_frame_pointer < top  &&
			   (char *)fp_fix->old_frame_pointer > (char *)stacktop)
			{
				ADJUST_FRAME_POINTER(fp_fix->old_frame_pointer, shift_size);
			}
		}
		if ((unsigned char *)mv_chain >= top)
		{
			mv_st_ent->mv_st_next = (unsigned int)((char *)mv_chain - (char *)mv_st_ent);
			mv_chain = mv_st_ent;
		} else
		{
			top -= shift_size + SIZEOF(stack_frame);
			mv_chain = (mv_stent *)((char *)mv_chain - shift_size);
			mvst_tmp = mv_chain;
			mvst_prev = (mv_stent *)((char *)mvst_tmp + mvst_tmp->mv_st_next);
			while (mvst_prev < (mv_stent *)top)
			{
				mvst_tmp = mvst_prev;
				mvst_prev = (mv_stent *)((char *)mvst_tmp + mvst_tmp->mv_st_next);
			}
			mvst_tmp->mv_st_next = (unsigned int)((char *)mv_st_ent - (char *)mvst_tmp);
			mv_st_ent->mv_st_next = (unsigned int)((char *)mvst_prev - (char *)mv_st_ent + shift_size);
		}
	} else
	{
		PUSH_MV_STENT(MVST_TPHOLD);
		mv_st_ent = mv_chain;
		fp = frame_pointer;
	}
	mv_st_ent->mv_st_cont.mvs_tp_holder.tphold_tlevel = dollar_tlevel;
#	ifdef GTM_TRIGGER
	if (!dollar_tlevel)
		/* We only save this on level 0 - Note if this is made further conditional, be sure to visit
		 * stp_gcol_src.h where it is GC'd and tp_restart() to adjust the conditions there as well.
		 */
		memcpy(&mv_st_ent->mv_st_cont.mvs_tp_holder.ztwormhole_save, &dollar_ztwormhole, SIZEOF(mval));
#	endif
	if (NULL == tpstackbase)
	{
		tstack_ptr = (unsigned char *)malloc(TP_STACK_SIZE);
		tp_sp = tpstackbase = tstack_ptr + TP_STACK_SIZE;
		tpstacktop = tstack_ptr;
		tp_pointer = NULL;
	}

	/* Add a new tp_frame in the TP stack */
	DBGRFCT((stderr, "\n*** op_tstart: *** Entering $TLEVEL = %d\n", dollar_tlevel + 1));
	tf = (tp_frame *)(tp_sp -= SIZEOF(tp_frame));
	assert((unsigned char *)tf > tpstacktop);	/* Block should lie entirely within tp stack area */
	tf->dlr_t = dollar_truth;
	tf->restart_pc = fp->mpc;
	tf->restart_ctxt = fp->ctxt;
	tf->fp = fp;
	tf->serial = serial;
	tf->trans_id = *tid;
	tf->restartable = (NORESTART != prescnt);
	tf->old_locks = (NULL != mlk_pvt_root);
	tf->orig_gv_target = gv_target;
	DBG_CHECK_GVTARGET_CSADDRS_IN_SYNC;
#	ifdef DEBUG
	if (!jgbl.forw_phase_recovery)
	{	/* In case of forward phase of journal recovery, gv_currkey is set by caller (mur_forward) only
		 * after the call to op_tstart so avoid doing gv_currkey check.
		 */
		DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
	}
#	endif
	/* If the TP structures have not yet been initialized, do that now. */
	if (NULL == TREF(gv_tporigkey_ptr))
	{	/* This need only be set once */
		TREF(gv_tporigkey_ptr) = (gv_orig_key_array *)malloc(SIZEOF(gv_orig_key_array));
		memset(TREF(gv_tporigkey_ptr), 0, SIZEOF(gv_orig_key_array));
	}
	tf->orig_key = (gv_key *)&((TREF(gv_tporigkey_ptr))->gv_orig_key[dollar_tlevel][0]);
	assert(NULL != gv_currkey);
	MEMCPY_KEY(tf->orig_key, gv_currkey);
	tf->gd_header = gd_header;
	tf->gd_reg = gv_cur_region;
	tf->zgbldir = dollar_zgbldir;
	tf->mvc = mv_st_ent;
	tf->sym = curr_symval;
	tf->tp_save_all_flg = FALSE;
	if (NULL == tp_pointer)
	{
		tf->implicit_tstart = implicit_tstart;
		GTMTRIG_ONLY(tf->implicit_trigger = implicit_trigger);
	} else
	{
		tf->implicit_tstart = tp_pointer->implicit_tstart;
		GTMTRIG_ONLY(tf->implicit_trigger = tp_pointer->implicit_trigger);
	}
	GTMTRIG_ONLY(tf->cannot_commit = FALSE;)
	tf->vars = (tp_var *)NULL;
	tf->old_tp_frame = tp_pointer;
	tp_pointer = tf;
	if (prescnt > 0)
	{
		VAR_COPY(lvname, varlst);
		for (pres = 0;  pres < prescnt;  ++pres)
		{
			preserve = va_arg(lvname, mval *);
			/* Note: the assumption (according to the comment below) is that this mval points into the literal table
			   and thus could not possibly be undefined. In that case, I do not understand why the earlier loop to
			   do MV_FORCE_STR on these variables. Future todo -- verify if that loop is needed. On the assumption
			   that it is not, the below assert will verify that the mval is defined to catch any NOUNDEF case.
			*/
			assert(MV_DEFINED(preserve));
			/* The incoming 'preserve' is the pointer to a literal mval table entry. For the indirect code
			 * (eg. Direct Mode), since the literal table is no longer on the M stack, we should not shift
			 * the incoming va_arg pointer (C9D01-002205) */
			mvname = preserve;
			if (0 != mvname->str.len)
			{	/* Convert mval to mident and see if it's in the symbol table */
				tpvent.var_name.len = mvname->str.len;
				tpvent.var_name.addr = mvname->str.addr;
				COMPUTE_HASH_MNAME(&tpvent);
				tpvent.marked = FALSE;
				if (add_hashtab_mname_symval(&curr_symval->h_symtab, &tpvent, NULL, &tabent))
					lv_newname(tabent, curr_symval);
				lv = (lv_val *)tabent->value;
				assert(lv);
				assert(LV_IS_BASE_VAR(lv));
				assert(0 < lv->stats.trefcnt);
				assert(lv->stats.crefcnt <= lv->stats.trefcnt);
				/* In order to allow restart of a sub-transaction, this should chain rather than back stop,
				   with appropriate changes to lv_var_clone and tp_unwind */
				if (NULL == lv->tp_var)
				{
					TP_SAVE_RESTART_VAR(lv, tf, &tabent->key);
					if (LV_HAS_CHILD(lv))
						TPSAV_CNTNRS_IN_TREE(lv);
				} else
				{	/* We have saved this var previously. But check if it got saved via a container var
					   and therefore has no name associated with it. If so, update the key in the tp_var
					   structure so it gets its name restored properly if necessary.
					*/
					if (0 == lv->tp_var->key.var_name.len)
						lv->tp_var->key = tabent->key;
				}
			}
		}
		va_end(lvname);
	} else if (ALLLOCAL == prescnt)
	{	/* Preserve all variables */
		tf->tp_save_all_flg = TRUE;
		++curr_symval->tp_save_all;
		for (curent = curr_symval->h_symtab.base, topent = curr_symval->h_symtab.top;  curent < topent;  curent++)
		{
			if (HTENT_VALID_MNAME(curent, lv_val, lv) && ('$' != *curent->key.var_name.addr))
			{
				assert(lv);
				assert(LV_IS_BASE_VAR(lv));
				assert(0 < lv->stats.trefcnt);
				assert(lv->stats.crefcnt <= lv->stats.trefcnt);
				if (NULL == lv->tp_var)
				{
					TP_SAVE_RESTART_VAR(lv, tf, &curent->key);
					if (LV_HAS_CHILD(lv))
						TPSAV_CNTNRS_IN_TREE(lv);
				} else
				{	/* We have saved this var previously. But check if it got saved via a container var
					   and therefore has no name associated with it. If so, update the key in the tp_var
					   structure so it gets its name restored properly if necessary.
					*/
					if (0 == lv->tp_var->key.var_name.len)
						lv->tp_var->key = curent->key;
				}
			}
		}
	}
	va_end(varlst);
	/* Store existing state of locks */
	for (pre_lock = mlk_pvt_root;  NULL != pre_lock;  pre_lock = pre_lock->next)
	{
		if ((NULL == pre_lock->tp) || (pre_lock->tp->level != pre_lock->level)
			|| (pre_lock->tp->zalloc != pre_lock->zalloc))
		{	/* Only stack locks that have changed since last TSTART */
			lck_tp = (mlk_tp *)malloc(SIZEOF(mlk_tp));
			lck_tp->tplevel = dollar_tlevel;
			lck_tp->level = pre_lock->level;
			lck_tp->zalloc = pre_lock->zalloc;
			lck_tp->next = pre_lock->tp;
			pre_lock->tp = lck_tp;
		}
	}
	++dollar_tlevel;
	/* Store the global (across all segments) dollar_tlevel specific information. */
	if (NULL != first_sgm_info) /* database activity existed in prior levels */
	{
		for (prev_gtli = NULL, gtli = global_tlvl_info_head; gtli; gtli = gtli->next_global_tlvl_info)
			prev_gtli = gtli;
		new_gtli = (global_tlvl_info *)get_new_element(global_tlvl_info_list, 1);
		new_gtli->global_tlvl_fence_info = jnl_fence_ctl.fence_list;
		new_gtli->t_level = dollar_tlevel;
		GTMTRIG_ONLY(new_gtli->tlvl_prev_ztworm_ptr = jgbl.prev_ztworm_ptr;)
		new_gtli->tlvl_cumul_jrec_len = jgbl.cumul_jnl_rec_len;
		DEBUG_ONLY(new_gtli->tlvl_cumul_index = jgbl.cumul_index;)
		new_gtli->tlvl_tp_ztp_jnl_upd_num = jgbl.tp_ztp_jnl_upd_num;
		/* Store current state of update_array global variables */
		assert((NULL != first_ua) && (NULL != curr_ua)); /* Since first_sgm_info is NOT NULL, database activity existed */
		new_gtli->curr_ua = (struct ua_list *)(curr_ua);
		new_gtli->upd_array_ptr = update_array_ptr;
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
		for (prev_tli = NULL, tli = si->tlvl_info_head; (NULL != tli); tli = tli->next_tlevel_info)
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
		/* Prepare for journaling logical records if journaling is enabled on this region OR if replication was
		 * allowed on this region (this is a case where replication was ON originally but later transitioned
		 * into WAS_ON state and journaling got turned OFF.
		 */
		csa = si->tp_csa;
		if (JNL_WRITE_LOGICAL_RECS(csa))
		{
			assert((NULL != si->jnl_head) || (NULL == csa->next_fenced));
			assert((NULL == si->jnl_head) || (NULL != csa->next_fenced));
			assert((NULL == csa->next_fenced) || (JNL_FENCE_LIST_END == csa->next_fenced)
								|| (NULL != csa->next_fenced->sgm_info_ptr->jnl_head));
			assert(NULL == *si->jnl_tail);
			SET_PREV_JFB(si, prev_jfb);
			new_tli->tlvl_jfb_info = prev_jfb;
			assert(NULL != si->jnl_list);
			assert(NULL != si->format_buff_list);
			new_tli->jnl_list_elems = si->jnl_list->nElems;
			new_tli->jfb_list_elems = si->format_buff_list->nElems;
		}
		new_tli->next_tlevel_info = NULL;
		new_tli->update_trans = si->update_trans;
		if (prev_tli)
			prev_tli->next_tlevel_info = new_tli;
		else
			si->tlvl_info_head = new_tli;
	}
	/* If starting first TP level, also start TP timer if set to non-default value */
	assert(0 <= TREF(dollar_zmaxtptime));
	if ((0 < TREF(dollar_zmaxtptime)) && (1 == dollar_tlevel))
		(*tp_timeout_start_timer_ptr)(TREF(dollar_zmaxtptime));
	DBGRFCT((stderr, "\nop_tstart: complete\n"));
}
