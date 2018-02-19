/****************************************************************
 *								*
 * Copyright (c) 2016-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_ipc.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "dpgbldir.h"
#include "filestruct.h"
#include "gvcst_protos.h"
#include "mvalconv.h"
#include "op.h"
#include "gdsblk.h"
#include "gtm_reservedDB.h"
#include "gvn2gds.h"
#include "mu_cre_file.h"
#include "gds_rundown.h"
#include "min_max.h"
#include "gtm_caseconv.h"
#include "hashtab_mname.h"
#include "send_msg.h"
#include "error.h"
#include "gtm_logicals.h"
#include "trans_log_name.h"
#include "iosp.h"
#include "parse_file.h"
#include "getzposition.h"
#include "util.h"
#include "repl_msg.h"			/* for gtmsource.h */
#include "gtmsource.h"			/* for jnlpool_addrs_ptr_t */


LITREF mval		literal_statsDB_gblname;

GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data	*cs_data;
GBLREF jnlpool_addrs_ptr_t	jnlpool;
GBLREF uint4		process_id;
GBLREF uint4		dollar_tlevel;
GBLREF gd_region	*gv_cur_region;
GBLREF gv_key		*gv_altkey, *gv_currkey;
GBLREF gv_namehead	*reset_gv_target;
GBLREF boolean_t	need_core;
GBLREF boolean_t	created_core;
GBLREF boolean_t	dont_want_core;
GBLREF gd_addr		*gd_header;
GBLREF mstr		extnam_str;
DEBUG_ONLY(GBLREF boolean_t    ok_to_UNWIND_in_exit_handling;)

STATICDEF intrpt_state_t	gvcst_statsDB_open_ch_intrpt_ok_state;
STATICDEF gd_region		*save_statsDBreg;	/* For use in condition handler */

/* Macro to restore the values that were saved at the start of the routine */
#define RESTORE_SAVED_VALUES					\
MBSTART {							\
	TP_CHANGE_REG(save_cur_region);				\
	jnlpool = save_jnlpool;					\
	gv_target = save_gv_target;				\
	reset_gv_target = save_reset_gv_target;			\
	RESTORE_GV_ALTKEY(save_altkey);				\
	RESTORE_GV_CURRKEY(save_currkey);			\
	TREF(gd_targ_gvnh_reg) = save_gd_targ_gvnh_reg;		\
	TREF(gd_targ_map) = save_gd_targ_map;			\
	TREF(gd_targ_addr) = save_gd_targ_addr;			\
	TREF(gv_last_subsc_null) = save_gv_last_subsc_null;	\
	TREF(gv_some_subsc_null) = save_gv_some_subsc_null;	\
	gd_header = save_gd_header;				\
} MBEND

error_def(ERR_DBPRIVERR);
error_def(ERR_DRVLONGJMP);	/* Generic internal only error used to drive longjump() in a queued condition handler */
error_def(ERR_RNDWNSTATSDBFAIL);
error_def(ERR_STATSDBERR);

/* This routine is a wrapper for mu_cre_file() when called from GTM. The issue is mu_cre_file() was written to run
 * largely stand-alone and then quit. So it uses stack vars for all the needed database structures and initializes only
 * what it needs leaving those structures largely unusable for functions that come later. That's fine when it is called
 * from mupip_create() but not good when called to create an auto-created DB from within mumps or other executables.
 * What we do here is buffer the current region (pointed to by the parameter region) and the dyn.addr segment it points
 * with stack variables, then call mu_cre_file() before throwing it all away when we return.
 *
 * Parameter(s):
 *
 *   reg - gd_region * of region to be created
 *
 * Return value:
 *   File creation return code
 */
unsigned char gvcst_cre_autoDB(gd_region *reg)
{
	gd_region		*save_cur_region;
	gd_region		cur_region;
	gd_segment		cur_segment;
	jnlpool_addrs_ptr_t	save_jnlpool;
	unsigned char		cstatus;

	assert(RDBF_AUTODB & reg->reservedDBFlags);
	save_cur_region = gv_cur_region;
	save_jnlpool = jnlpool;
	memcpy((char *)&cur_region, reg, SIZEOF(gd_region));
	memcpy((char *)&cur_segment, reg->dyn.addr, SIZEOF(gd_segment));
	gv_cur_region = &cur_region;
	gv_cur_region->dyn.addr = &cur_segment;
	cstatus = mu_cre_file();
	TP_CHANGE_REG(save_cur_region);
	jnlpool = save_jnlpool;
	return cstatus;
}

/* Initialize a statsDB database. This includes the following steps:
 *   1. Locate and open the statsDB related to the region parameter passed in.
 *   2. Write the initial gvstats_rec_t record for the input region and the current process. The writing of
 *      the stats record involves a bit of work:
 *        a. All statsDB records are written with two things in mind:
 *             i.  Records in memory must be written such that the gvstats_rec_t part of the record is 8 byte aligned.
 *             ii. Records must be of sufficient size in the block to prevent another record from being added to the
 *	           block that might cause the earlier record to move. Records must NOT move because they are being
 *	           updated directly (outside of database APIs).
 *        b. So need to compute two types of padding that is written before the gvstats_rec_t structure. The M user
 *	     program accesses the gvstats_rec_t as the last (SIZEOF(gvstats_rec_t)) bytes in the record.
 *        c. Need to copy the existing stats from cs_addrs into the shared record.
 *	  d. Write the gvstats_rec_t record.
 *   3. Once the stats record is written, mark the file R/O to prevent further updates (from this process).
 *   4. Need to point the cs_addrs pointer to the stats in the newly written record (after locating it via fields
 *      in gv_target.
 *
 * The call to op_gvname() sets up gv_currkey for our op_gvput() call also giving us the key length we need for
 * the alignment pad calculation.
 *
 * Parameter(s):
 *   baseDBreg  - gd_region* address of base (not hidden) database whose statsDB database we are to initialize.
 */
void gvcst_init_statsDB(gd_region *baseDBreg, boolean_t do_statsdb_init)
{
	mval				pid_mval, baseDBreg_nam_mval;
	mval				statsDBrec_mval, statsDBget_mval;
	gd_region			*statsDBreg, *statsDBreg_located, *save_cur_region;
	gv_namehead			*save_gv_target, *save_reset_gv_target;
	jnlpool_addrs_ptr_t		save_jnlpool;
	srch_blk_status 		*bh;
	char				statsDBinitrec[SIZEOF(gvstats_rec_t) * 2];	/* Gives chunk large enuf to hold pad */
	int				datasize, extlen, freespace, padsize, sizewkey, sizewkeyrnd;
	gv_key				save_altkey[DBKEYALLOC(MAX_KEY_SZ)], save_currkey[DBKEYALLOC(MAX_KEY_SZ)];
	sgmnt_addrs			*baseDBcsa, *statsDBcsa;
	statsDB_deferred_init_que_elem	*sdiqeptr;
	gvnh_reg_t			*save_gd_targ_gvnh_reg;
	gd_binding			*save_gd_targ_map;
	gd_addr				*save_gd_targ_addr, *save_gd_header;
	boolean_t			save_gv_last_subsc_null, save_gv_some_subsc_null, longjmp_done1, longjmp_done2;
	boolean_t			save_gvcst_statsDB_open_ch_active;
	gd_binding			*ygs_map;
	mname_entry			gvname;
	ht_ent_mname			*tabent;
	gvnh_reg_t			*gvnh_reg;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	BASEDBREG_TO_STATSDBREG(baseDBreg, statsDBreg_located);
	assert(!statsDBreg_located->statsDB_setup_completed);
	assert(baseDBreg->open);
	assert(dba_cm != baseDBreg->dyn.addr->acc_meth);
	baseDBcsa = &FILE_INFO(baseDBreg)->s_addrs;
	save_gvcst_statsDB_open_ch_active = TREF(gvcst_statsDB_open_ch_active);	/* note down in case this function recurses */
	TREF(gvcst_statsDB_open_ch_active) = TRUE;
	longjmp_done2 = FALSE;	/* initialize this variable BEFORE "setjmp" done in first ESTABLISH_NORET below so it is
				 * initialized even if we break out of the for loop due to an error even before
				 * this variable got initialized as part of the second ESTABLISH_NORET below.
				 */
	gvcst_statsDB_open_ch_intrpt_ok_state = intrpt_ok_state;	/* needed by "gvcst_statsDB_open_ch" */
	ESTABLISH_NORET(gvcst_statsDB_open_ch, longjmp_done1);

	for ( ; !longjmp_done1; )	/* have a dummy for loop to be able to use "break" for various codepaths below */
	{
		if (!do_statsdb_init)
		{	/* Want to open statsDB region but do not want statsdb init (^%YGS addition etc.). This is a call from
			 * OPEN_BASEREG_IF_STATSREG. All the macro cares about is the statsDB be open. Not that a statsDB init
			 * happen. So do just that. The reason we need to do it here is because errors are better handled here
			 * (user-invisible) and the caller will modify gld map entries and/or switch baseDB to NOSTATS if any
			 * errors occur thereby preventing future access to this statsDB database file.
			 */
			gvcst_init(statsDBreg_located, NULL);
			if (statsDBreg_located->open)		/* do the check just in case */
			{
				statsDBcsa = &FILE_INFO(statsDBreg_located)->s_addrs;
				/* Database was opened read/only as this process has no privs to write to it.
				 * Even though we do not need to do the statsdb init now, we know we cannot do the init
				 * successfully and since the caller in this case is in a better position to turn off
				 * stats in the gld (OPEN_BASEREG_IF_STATSREG) use this opportunity to raise an error.
				 */
				if (!statsDBcsa->orig_read_write)
					rts_error_csa(CSA_ARG(statsDBcsa) VARLSTCNT(4) ERR_DBPRIVERR, 2,
											DB_LEN_STR(statsDBreg_located));
			}
			break;
		}
		if (0 < dollar_tlevel)
		{	/* We are inside a transaction. We cannot do this inside a transaction for two reasons:
			 *
			 *   1. Were we to write the initialization record, there's a chance the record could be unwound as
			 *      part of a TP rollback and no mechanism exists to rewrite it.
			 *   2. We actually don't write the gvstats record to the database until the commit so there's no
			 *      mechanism to find where the record was written, locate it and set up non-DB-API access to
			 *      it as we do with non-TP initializations.
			 *
			 * Solution is to defer the initialization until the transaction is exited - either by commiting
			 * or by aborting the transaction.
			 */
			sdiqeptr = malloc(SIZEOF(statsDB_deferred_init_que_elem));
			sdiqeptr->baseDBreg = baseDBreg;
			sdiqeptr->statsDBreg = statsDBreg_located;
			sdiqeptr->next = TREF(statsDB_init_defer_anchor);
			TREF(statsDB_init_defer_anchor) = sdiqeptr;
			/* Although we did not open the statsDB as a statsDB, we still need to open the region and see if
			 * there are any errors before returning to the caller. This is because callers like "gvcst_init"
			 * and "OPEN_BASEREG_IF_STATSREG" rely on this open to happen here (and catch errors) so they
			 * can take appropriate action (see comment in OPEN_BASEREG_IF_STATSREG for example reason).
			 */
			gvcst_init(statsDBreg_located, NULL);
			break;
		}
		save_cur_region = gv_cur_region;
		save_jnlpool = jnlpool;
		save_gv_target = gv_target;
		save_reset_gv_target = reset_gv_target;
		SAVE_GV_CURRKEY(save_currkey);
		/* It is possible we end up here with the following function call trace starting from name-level $order.
		 *    op_gvorder -> op_gvdata -> gvcst_spr_data -> op_tcommit -> gvcst_deferred_init_statsDB -> gvcst_init_statsDB
		 * In that case, the op_gvorder would be relying on "gv_altkey" which we could modify as part of the following call
		 * to set up the ^%YGS global name.
		 *    op_gvname -> op_gvname_common -> gv_bind_subsname -> gvcst_root_search
		 * 								-> SET_GV_ALTKEY_TO_GBLNAME_FROM_GV_CURRKEY
		 * Therefore, we need to save/restore "gv_altkey" too.
		 */
		SAVE_GV_ALTKEY(save_altkey);
		/* Below save is similar to that done in op_gvsavtarg/op_gvrectarg */
		save_gd_targ_gvnh_reg = TREF(gd_targ_gvnh_reg);
		save_gd_targ_map = TREF(gd_targ_map);
		save_gd_targ_addr = TREF(gd_targ_addr);
		save_gv_last_subsc_null = TREF(gv_last_subsc_null);
		save_gv_some_subsc_null = TREF(gv_some_subsc_null);
		save_gd_header = gd_header;	/* save "gd_header" before tampering with global variable */
		gd_header = baseDBreg->owning_gd; /* direct "op_gvname" to search for maps in this gld */
		/* Must have baseDB open and be opted in to be here */
		assert(TREF(statshare_opted_in));
		assert(!IS_DSE_IMAGE);	/* DSE opens a statsdb only directly (never through a base DB) */
		/* Create a condition handler so the above saved items can be undone on an error to restore the environment */
		ESTABLISH_NORET(gvcst_statsDB_init_ch, longjmp_done2);
		if (longjmp_done2)
		{	/* We returned here due to an error encountered somewhere below.
			 * Restore the things that were saved, then REVERT our handler.
			 */
			REVERT;
			RESTORE_SAVED_VALUES;
			break;
		}
		/* If this statsDB was previously opened by direct references to ^%YGS, gvcst_init() will have set the DB
		 * to R/O mode so if it was previously opened, we need to set it back to write-mode to add the needed
		 * gvstats record.
		 */
		if (statsDBreg_located->open)
		{	/* This statsDB was already open so we have to reset the R/O flags so we can add the process record */
			assert(!statsDBreg_located->statsDB_setup_completed);
			statsDBcsa = &FILE_INFO(statsDBreg_located)->s_addrs;
			assert(statsDBcsa->orig_read_write);
			statsDBreg_located->read_only = FALSE;			/* Maintain read_only/read_write in parallel */
			statsDBcsa->read_write = TRUE;				/* Maintain reg->read_only simultaneously */
		}
		/* Create mvals with region name and processid in them to feed to op_gvname */
		memset((char *)&baseDBreg_nam_mval, 0, SIZEOF(mval));
		baseDBreg_nam_mval.mvtype = MV_STR;
		baseDBreg_nam_mval.str.len = baseDBreg->rname_len;
		baseDBreg_nam_mval.str.addr = (char *)&baseDBreg->rname;
		/* Init parts of the gvstats record mval not dependent on further calculations */
		statsDBrec_mval.mvtype = MV_STR;
		statsDBrec_mval.str.addr = statsDBinitrec;
		/* Step 1: The very first thing that needs to happen when a statsDB is freshly opened is that we need to write
		 *         a special record ^%YGS(region,INT_MAX) that must be there to prevent left hand block splits that
		 *         can cause a process gvstats record to be moved to a new block. We can't check it until we do the
		 *         first op_gvname that opens the proper DB and makes sure cs_addrs is populated, etc. Since most of
		 *         the time, processes won't need to do this, we go ahead and do the op_gvname that most processes
		 *         need which is for the ^%YGS(region,pid) record. If we then check and find the max record not written,
		 *         we'll redo the op_gvname to setup for the max record instead.
		 */
		i2mval(&pid_mval, process_id);
		/* We have already saved gv_currkey at function entry and are tampering with it in this function.
		 * The "op_gvname" call does a DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC check which would cross-check gv_currkey
		 * & gv_target. That check could fail if this function was called when the two were not in sync (possible for
		 * example if the call stack is "op_gvorder -> gv_init_reg -> gvcst_init -> gvcst_init_statsDB". So bypass that
		 * check by clearing the key.
		 */
		DEBUG_ONLY(gv_currkey->base[0] = KEY_DELIMITER;) /* to bypass DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC in op_gvname */
		/* Note that "op_gvname" of ^%YGS is going to first open the region corresponding to the unsubscripted name %YGS.
		 * That is guaranteed (by GDE) to be a statsDB region. If it happens to be different from "statsDBreg_located",
		 * then we need to open that region first and that in turn means we first need to open its associated baseDB region
		 * (Design assumption : statsDB region is never opened until its corresponding baseDB region is opened). To avoid
		 * these unnecessary database opens, we modify the gld map so the unsubscripted %YGS name maps to
		 * "statsDBreg_located". This way we don't need any opens of baseDB as we know "baseDBreg" (baseDB region of
		 * "statsDBreg_located") is already open at this point. And we avoid doing this map modification more than once
		 * per gld using the "ygs_map_entry_changed" flag in the gld.
		 */
		if (!gd_header->ygs_map_entry_changed)
		{
			ygs_map = gv_srch_map(gd_header, STATSDB_GBLNAME, STATSDB_GBLNAME_LEN, SKIP_BASEDB_OPEN_TRUE);
				/* SKIP_BASEDB_OPEN_TRUE is to signal "gv_srch_map" to not invoke OPEN_BASEREG_IF_STATSREG.
				 * See comment about "skip_basedb_open" in function prototype of "gv_srch_map" for details.
				 */
			assert(IS_STATSDB_REG(statsDBreg_located));
			ygs_map->reg.addr = statsDBreg_located;
			gvname.var_name.addr = STATSDB_GBLNAME;
			gvname.var_name.len = STATSDB_GBLNAME_LEN;
			COMPUTE_HASH_MSTR(gvname.var_name, gvname.hash_code);
			tabent = lookup_hashtab_mname((hash_table_mname *)gd_header->tab_ptr, &gvname);
			if (NULL != tabent)
			{	/* Repoint ^%YGS hashtable entry to point unsubscripted global name to new statsdb region */
				gvnh_reg = (gvnh_reg_t *)tabent->value;
				assert(NULL != gvnh_reg);
				gvnh_reg->gd_reg = statsDBreg_located;
			}
			gd_header->ygs_map_entry_changed = TRUE;
		}
		extlen = extnam_str.len;
		op_gvname(3, (mval *)&literal_statsDB_gblname, &baseDBreg_nam_mval, &pid_mval);
		extnam_str.len = extlen;
		assert(NULL != gv_currkey);
		assert(0 != gv_currkey->end);
		statsDBreg = gv_cur_region;
		if (statsDBreg != statsDBreg_located)
		{	/* For some reason the statsDB was not opened or is not available - mark it NOSTATS and we are done */
			baseDBreg->reservedDBFlags |= RDBF_NOSTATS;
			baseDBcsa->reservedDBFlags |= RDBF_NOSTATS;
			REVERT;
			RESTORE_SAVED_VALUES;
			break;
		}
		statsDBcsa = &FILE_INFO(statsDBreg)->s_addrs;
		if (!statsDBcsa->orig_read_write)
			/* Database was opened read/only as this process has no privs to write to it - raise error */
			rts_error_csa(CSA_ARG(statsDBcsa) VARLSTCNT(4) ERR_DBPRIVERR, 2, DB_LEN_STR(statsDBreg));
		assert(!statsDBreg->statsDB_setup_completed);
		if (!statsDBcsa->statsDB_setup_completed)
		{	/* If initialization was never completed, do it now */
			assert(IS_STATSDB_REG(statsDBreg));
			/* Step 2: Now figure out the alignment pad size needed to make record fields align in memory */
			sizewkey = SIZEOF(blk_hdr) + SIZEOF(rec_hdr) + gv_currkey->end + 1;
			sizewkeyrnd = ROUND_UP2(sizewkey, SIZEOF(gtm_uint64_t));
			padsize = sizewkeyrnd - sizewkey;
			/* Now to figure out the padding to prevent additional records in this block. Find how much space in this
			 * block remains unused after this record. Use this space to increase the pad size by the needed amount.
			 */
			freespace = cs_data->blk_size - sizewkeyrnd - SIZEOF(gvstats_rec_t);
			padsize += (freespace >= MIN_STATSDB_REC) ? (((freespace - MIN_STATSDB_REC) / 8) + 1) * 8 : 0;
			if (0 < padsize)
				memset(statsDBinitrec, 0, padsize);		/* Initialize prefix pad area with NULLs */
			/* Move the current stats in cs_addrs to the shared rec we are about to write */
			memcpy(statsDBinitrec + padsize, cs_addrs->gvstats_rec_p, SIZEOF(gvstats_rec_t));
			memset((char *)&statsDBrec_mval, 0, SIZEOF(mval));
			statsDBrec_mval.mvtype = MV_STR;
			statsDBrec_mval.str.addr = statsDBinitrec;
			statsDBrec_mval.str.len = padsize + SIZEOF(gvstats_rec_t);
			op_gvput(&statsDBrec_mval);
			/* Step 3: Now we have written a record - set the DB to R/O */
			assert(statsDBcsa == &FILE_INFO(statsDBreg)->s_addrs);
			assert(statsDBcsa->orig_read_write);
			/* Step 4: Locate the newly written record and update the csa->gvstats_rec_p so new stats updates occur
			 *         in shared memory instead of process-private.
			 */
			bh = gv_target->hist.h;
			/* "op_gvput" could have returned a 0 value in case it was the one creating the GVT. To make sure we have
			 * a clean history - fetch back the record we just wrote.
			 */
			gvcst_get(&statsDBget_mval);    /* This should call "gvcst_search" which should fill in "bh->curr_rec" */
			assert(0 < bh->curr_rec.offset);
			assert((gv_currkey->end + 1) == bh->curr_rec.match);
			assert(SIZEOF(blk_hdr) == bh->curr_rec.offset); 			/* Should find 1st record in blk */
			baseDBcsa->gvstats_rec_p = (gvstats_rec_t *)(bh->buffaddr + sizewkey + padsize);
												/* ==> Start of gvstats_rec_t */
			assert(0 == (((UINTPTR_T)baseDBcsa->gvstats_rec_p) & 0x7));		/* Verify 8 byte alignment */
			statsDBcsa->statsDB_setup_completed = TRUE;
		}
		statsDBreg->read_only = TRUE;
		statsDBcsa->read_write = FALSE;				/* Maintain read_only/read_write in parallel */
		statsDBreg->statsDB_setup_completed = TRUE;
		REVERT;
		/* Restore previous region's setup */
		RESTORE_SAVED_VALUES;
		break;
	}
	REVERT;
	TREF(gvcst_statsDB_open_ch_active) = save_gvcst_statsDB_open_ch_active;
	/* Now that we have restored the environment as it was at function entry, check if there were any errors
	 * inside any of the two ESTABLISH_NORETs done above. If so, handle them appropriately.
	 */
	if (longjmp_done2 || longjmp_done1)
	{
		assert(0 != error_condition);
		/* Check if we got a TPRETRY (an internal error) in "gvcst_init" above. If so drive parent condition handler to
		 * trigger restart. For any other error conditions, do not do anything more as we want statsdb open to silently
		 * switch to nostats in that case.
		 */
		assert(0 != error_condition);
		if (ERR_TPRETRY == error_condition)
			DRIVECH(error_condition);	/* Drive lower level handlers with same error we had */
		/* Since we got an error while trying to open the statsDB, silently set NOSTATS on baseDB */
		baseDBreg->reservedDBFlags |= RDBF_NOSTATS;
		baseDBcsa->reservedDBFlags |= RDBF_NOSTATS;
	}
	return;
}

/* Simplistic handler for when errors occur during open of statsDB. The ESTABLISH_NORET is a place-holder for the
 * setjmp/longjmp sequence used to recover quietly from errors opening* the statsDB. So if the error is the magic
 * ERR_DRVLONGJMP, do just that (longjmp() via the UNWIND() macro). If the error is otherwise, capture the error
 * and where it was raised and send the STATSDBERR to the syslog before we unwind back to the ESTABLISH_NORET.
 */
CONDITION_HANDLER(gvcst_statsDB_open_ch)
{
	char	buffer[OUT_BUFF_SIZE];
	int	msglen;
	mval	zpos;

	START_CH(TRUE);
	assert(ERR_DBROLLEDBACK != arg);	/* A statsDB region should never participate in rollback */
	if (DUMPABLE)
		NEXTCH;				/* Bubble down till handled properly in mdb_condition_handler() */
	if ((SUCCESS == SEVERITY) || (INFO == SEVERITY))
		CONTINUE;			/* Keep going for non-error issues */
	if (ERR_DRVLONGJMP != arg)
	{	/* Need to reflect the current error to the syslog - First save message that got us here */
		msglen = TREF(util_outptr) - TREF(util_outbuff_ptr);
		assert(OUT_BUFF_SIZE > msglen);
		memcpy(buffer, TREF(util_outbuff_ptr), msglen);
		getzposition(&zpos);
		/* Send whole thing to syslog */
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_STATSDBERR, 4, zpos.str.len, zpos.str.addr, msglen, buffer);
	}
	intrpt_ok_state = gvcst_statsDB_open_ch_intrpt_ok_state;
	UNWIND(NULL, NULL);			/* Return back to where ESTABLISH_NORET was done */
}

/* Condition handler for gvcst_statsDB_init() - all we need to do is unwind back to where the ESTABLISH_NORET() is done
 * as the code at that point can do the needful in the frame of reference it needs to be done.
 */
CONDITION_HANDLER(gvcst_statsDB_init_ch)
{
	START_CH(TRUE);
	if (DUMPABLE)
		NEXTCH;				/* Bubble down till handled properly in mdb_condition_handler() */
	if ((SUCCESS == SEVERITY) || (INFO == SEVERITY))
		CONTINUE;			/* Keep going for non-error issues */
	DEBUG_ONLY(ok_to_UNWIND_in_exit_handling = TRUE);
	UNWIND(NULL, NULL);			/* Return back to where ESTABLISH_NORET was done */
}

/* Routine to remove the process's gvstats_rec from the statsDB database associated with a given baseDB. This also
 * entails copying the shared stats record back into cs_addrs of the baseDB and resetting it's stats pointer. This
 * "unlinks" the two databases.
 *
 * Parameter(s):
 *   baseDBreg  - gd_region* of baseDB whose stats we are "unsharing".
 */
void gvcst_remove_statsDB_linkage(gd_region *baseDBreg)
{
	mval			pid_mval, baseDBreg_nam_mval;
	mval			statsDBrec_mval, statsDBget_mval;
	gd_region		*statsDBreg, *save_cur_region;
	gv_namehead		*save_gv_target, *save_reset_gv_target;
	char			statsDBinitrec[SIZEOF(gvstats_rec_t) * 2];	/* Gives us a chunk large enuf to hold padding */
	gv_key			save_currkey[DBKEYALLOC(MAX_KEY_SZ)];
	sgmnt_addrs		*baseDBcsa, *statsDBcsa;
	gvstats_rec_t		*gvstats_rec_p;
	gvnh_reg_t		*save_gd_targ_gvnh_reg;
	gd_binding		*save_gd_targ_map;
	gd_addr			*save_gd_targ_addr, *save_gd_header;
	jnlpool_addrs_ptr_t	save_jnlpool;
	boolean_t		save_gv_last_subsc_null, save_gv_some_subsc_null;
#	ifdef DEBUG
	mval			stats_rec;
	srch_blk_status		*bh;
	uint4			recsize;
	rec_hdr			*recptr;
	gd_region		*statsDBreg_located;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(dba_cm != baseDBreg->dyn.addr->acc_meth);
	save_cur_region = gv_cur_region;
	save_gv_target = gv_target;
	save_reset_gv_target = reset_gv_target;
	SAVE_GV_CURRKEY(save_currkey);
	save_jnlpool = jnlpool;
	/* Below save is similar to that done in op_gvsavtarg/op_gvrectarg */
	save_gd_targ_gvnh_reg = TREF(gd_targ_gvnh_reg);
	save_gd_targ_map = TREF(gd_targ_map);
	save_gd_targ_addr = TREF(gd_targ_addr);
	save_gv_last_subsc_null = TREF(gv_last_subsc_null);
	save_gv_some_subsc_null = TREF(gv_some_subsc_null);
	/* The baseDB does not have to be open but if it is, stats will be unshared */
	assert(TREF(statshare_opted_in));
	/* Create mvals with region name and processid in them to feed to op_gvname */
	memset((char *)&baseDBreg_nam_mval, 0, SIZEOF(mval));
	baseDBreg_nam_mval.mvtype = MV_STR;
	baseDBreg_nam_mval.str.len = baseDBreg->rname_len;
	baseDBreg_nam_mval.str.addr = (char *)&baseDBreg->rname;
	i2mval(&pid_mval, process_id);
	/* Step 1: Locate the existing ^%YGS(region,pid) node. This locates the existing record for us so we can
	 *         copy the gvstats_rec data back to private storage in cs_addrs but also sets up the key for the
	 *	   "op_gvkill" call to kill that record.
	 */
	save_gd_header = gd_header;	/* save "gd_header" before tampering with global variable */
	gd_header = baseDBreg->owning_gd; /* direct "op_gvname" to search for maps in this gld */
	op_gvname(3, (mval *)&literal_statsDB_gblname, &baseDBreg_nam_mval, &pid_mval);
	assert(NULL != gv_currkey);
	assert(0 != gv_currkey->end);
	statsDBreg = gv_cur_region;
	statsDBcsa = &FILE_INFO(statsDBreg)->s_addrs;
	DEBUG_ONLY(BASEDBREG_TO_STATSDBREG(baseDBreg, statsDBreg_located));
	assert(statsDBreg == statsDBreg_located);
	assert(IS_STATSDB_REG(statsDBreg));
	assert(statsDBreg->statsDB_setup_completed);
	/* Step 2: Need to switch the database back to R/W so we can do the KILL */
	assert(statsDBcsa == &FILE_INFO(statsDBreg)->s_addrs);
	/* Note that if multiple statsDB regions map to the same statsDB file, it is possible to have
	 * statsDBreg->statsDB_setup_completed TRUE for more than one such region in which case all of them
	 * would map to the same statsDBcsa and might end up calling this function more than once for the same statsDBcsa.
	 * In that case, do the removal of ^%YGS node only once. The below check accomplishes that.
	 */
	statsDBreg->read_only = FALSE;
	if (statsDBcsa->statsDB_setup_completed)
	{
		statsDBcsa->read_write = TRUE;				/* Maintain read_only/read_write in parallel */
		assert(statsDBcsa->orig_read_write);
		/* Step 3: Copy the shared gvstats_rec_t data back to private and for debug, verify record address in DEBUG but only
		 *         if the baseDB is actually still open.
		 */
		if (baseDBreg->open)
		{
			baseDBcsa = &FILE_INFO(baseDBreg)->s_addrs;
#			ifdef DEBUG
			gvcst_get(&stats_rec);				/* Fetch record to set history for DEBUG mode validation */
			bh = gv_target->hist.h;
			assert(0 != bh->curr_rec.match);		/* Shouldn't be possible to create a GVT with this call */
			assert((gv_currkey->end + 1) == bh->curr_rec.match);
			assert(SIZEOF(blk_hdr) == bh->curr_rec.offset); 	/* We should find 1st record in block */
			recptr = (rec_hdr *)(bh->buffaddr + SIZEOF(blk_hdr));
			recsize = recptr->rsiz;
			/* The gvstats_rec_t part of the record is the last part of the record */
			gvstats_rec_p = (gvstats_rec_t *)((char *)recptr + (recsize - SIZEOF(gvstats_rec_t)));
			assert(gvstats_rec_p == baseDBcsa->gvstats_rec_p);
#			else
			gvstats_rec_p = baseDBcsa->gvstats_rec_p;
#			endif
			memcpy(&baseDBcsa->gvstats_rec, gvstats_rec_p, SIZEOF(gvstats_rec_t));
			baseDBcsa->gvstats_rec_p = &baseDBcsa->gvstats_rec;	/* ==> Reset start of gvstats_rec_t to private */
		}
		/* Step 4: Kill the record */
		op_gvkill();
		statsDBcsa->statsDB_setup_completed = FALSE;
	}
	statsDBreg->statsDB_setup_started = FALSE;
	statsDBreg->statsDB_setup_completed = FALSE;
	/* Restore previous region's setup */
	TP_CHANGE_REG(save_cur_region);
	jnlpool = save_jnlpool;
	gv_target = save_gv_target;
	reset_gv_target = save_reset_gv_target;
	RESTORE_GV_CURRKEY(save_currkey);
	TREF(gd_targ_gvnh_reg) = save_gd_targ_gvnh_reg;
	TREF(gd_targ_map) = save_gd_targ_map;
	TREF(gd_targ_addr) = save_gd_targ_addr;
	TREF(gv_last_subsc_null) = save_gv_last_subsc_null;
	TREF(gv_some_subsc_null) = save_gv_some_subsc_null;
	gd_header = save_gd_header;
}

/* Routine to opt-in opening statsDB for all open databases */
void gvcst_statshare_optin(void)
{
	gd_addr		*gdhdr_addr;
	gd_region	*r_save, *r_top;
	gd_region	*baseDBreg, *statsDBreg;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(0 == dollar_tlevel);
	TREF(statshare_opted_in) = TRUE;
	for (gdhdr_addr = get_next_gdr(NULL); NULL != gdhdr_addr; gdhdr_addr = get_next_gdr(gdhdr_addr))
	{	/* For each global directory */
		for (baseDBreg = gdhdr_addr->regions, r_top = baseDBreg + gdhdr_addr->n_regions; baseDBreg < r_top; baseDBreg++)
		{	/* For each region */
			if (!IS_REG_BG_OR_MM(baseDBreg) || IS_STATSDB_REG(baseDBreg))
				continue;
			if (RDBF_NOSTATS & baseDBreg->reservedDBFlags)
				continue;
			if (baseDBreg->open)
			{	/* Initialize statsDB for the given baseDB region */
				BASEDBREG_TO_STATSDBREG(baseDBreg, statsDBreg);
				statsDBreg->statsDB_setup_started = TRUE;
				gvcst_init_statsDB(baseDBreg, DO_STATSDB_INIT_TRUE);
			}
		}
	}
}

/* Routine to opt-out running down statsDBs for all open databases */
void gvcst_statshare_optout(void)
{
	gd_addr		*gdhdr_addr;
	gd_region	*r_top, *r_save, *baseDBreg, *statsDBreg;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(0 == dollar_tlevel);
	for (gdhdr_addr = get_next_gdr(NULL); gdhdr_addr; gdhdr_addr = get_next_gdr(gdhdr_addr))
	{	/* For each global directory */
		for (baseDBreg = gdhdr_addr->regions, r_top = baseDBreg + gdhdr_addr->n_regions; baseDBreg < r_top; baseDBreg++)
		{	/* For each region */
			if (!IS_REG_BG_OR_MM(baseDBreg) || IS_STATSDB_REG(baseDBreg))
				continue;
			if (RDBF_NOSTATS & baseDBreg->reservedDBFlags)
				continue;
			if (baseDBreg->open)
			{
				BASEDBREG_TO_STATSDBREG(baseDBreg, statsDBreg);
				if (statsDBreg->open && statsDBreg->statsDB_setup_completed)
					gvcst_remove_statsDB_linkage(baseDBreg);
			}
		}
	}
	TREF(statshare_opted_in) = FALSE;
}

/* Routine to remove the statsDB linkages between the statsDB databases and the baseDBs for all open statsDBs.
 * General purpose of this routine is to run the regions, find the statsDBs, locate the associated baseDBs,
 * and call a routine to cleanup one baseDB/statsDB region at a time using a condition handler wrapper so
 * we can report (via operator log messages) but then largely ignore errors so we can process each region
 * appropriately. We are doing this in the exit handler so error handling capability is at a minimum.
 */
void gvcst_remove_statsDB_linkage_all(void)
{
	gd_addr		*gdhdr_addr;
	gd_region	*r_top, *r_save, *statsDBreg, *baseDBreg;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(0 == dollar_tlevel);
	for (gdhdr_addr = get_next_gdr(NULL); gdhdr_addr; gdhdr_addr = get_next_gdr(gdhdr_addr))
	{	/* For each global directory */
		for (statsDBreg = gdhdr_addr->regions, r_top = statsDBreg + gdhdr_addr->n_regions; statsDBreg < r_top; statsDBreg++)
		{
			if (statsDBreg->open && IS_STATSDB_REG(statsDBreg) && statsDBreg->statsDB_setup_completed)
			{	/* We really are an OPEN and initialized statsDB - remove the link */
				STATSDBREG_TO_BASEDBREG(statsDBreg, baseDBreg);
				assert(NULL != baseDBreg);
				gvcst_remove_statsDB_linkage_wrapper(baseDBreg, statsDBreg);
			}
		}
	}
}

/* Routine to provide and condition handler wrapper for gvcst_remove_statsDB_linkage() when called from the exit handler.
 * This is because we can't tolerate much error handling in the exit handler so any errors we do get, we report and
 * unwind (ignore). Since we are taking things apart here, the ramifications of ignoring an error aren't bad. Worst
 * risk is not deleting the process record we created.
 *
 * Parameter(s):
 *   baseDBreg  - gd_region* of baseDB we want to unlink
 *   statsDBreg - gd_region* of statsDB we want to unlink
 */
void gvcst_remove_statsDB_linkage_wrapper(gd_region *baseDBreg, gd_region *statsDBreg)
{
	ESTABLISH(gvcst_remove_statsDB_linkage_ch);
	save_statsDBreg = statsDBreg;						/* Save for use in condition handler */
	gvcst_remove_statsDB_linkage(baseDBreg);				/* If fails, we unwind and go to next one */
	REVERT;
}

/* Routine to handle deferred statsDB initializations. These are initializations that were deferred because they occurred
 * inside a TP transaction. We are now (or should be) free of that transaction so the initializations can be performed. Any
 * regions deferred are in a queue. Process them now and release the blocks that recorded them (this is a one time thing
 * so no reason exists to hold or queue the free blocks).
 */
void gvcst_deferred_init_statsDB(void)
{
	statsDB_deferred_init_que_elem	*sdiqeptr, *sdiqeptr_next;
	gd_region			*baseDBreg, *statsDBreg;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (process_exiting)		/* No sense opening anything if we are exiting. Like driven by exit handler's rollback */
		return;
	assert(0 == dollar_tlevel);
	assert(NULL != TREF(statsDB_init_defer_anchor));
	for (sdiqeptr = TREF(statsDB_init_defer_anchor), sdiqeptr_next = sdiqeptr->next; sdiqeptr; sdiqeptr = sdiqeptr_next)
	{	/* For each statsDB on the queue, perform initialization and free the deferred init block */
		sdiqeptr_next = sdiqeptr->next;					/* Next entry on queue or NULL */
		/* We wanted to initialize it earlier but couldn't because we were in a transaction. Now the baseDB
		 * still needs to be open and not re-opened and we can't have completed initialization elsewhere.
		 */
		baseDBreg = sdiqeptr->baseDBreg;
		statsDBreg = sdiqeptr->statsDBreg;
		assert(baseDBreg->open);
		if (!statsDBreg->statsDB_setup_completed)
			gvcst_init_statsDB(sdiqeptr->baseDBreg, DO_STATSDB_INIT_TRUE);
		TREF(statsDB_init_defer_anchor) = sdiqeptr_next;		/* Remove current entry from queue */
		free(sdiqeptr);
	}
}

/* Condition handler for gvcst_remove_statsDB_linkage_wrapper */
CONDITION_HANDLER(gvcst_remove_statsDB_linkage_ch)
{
	char	buffer[OUT_BUFF_SIZE];
	int	msglen;
	mval	zpos;

	START_CH(TRUE);
	/* Save error that brought us here */
	msglen = TREF(util_outptr) - TREF(util_outbuff_ptr);
	assert(OUT_BUFF_SIZE > msglen);
	memcpy(buffer, TREF(util_outbuff_ptr), msglen);
	getzposition(&zpos);	/* Find out where it occurred */
	/* Send whole thing to syslog */
	send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_RNDWNSTATSDBFAIL, 10, REG_LEN_STR(save_statsDBreg),
		     DB_LEN_STR(save_statsDBreg), zpos.str.len, zpos.str.addr, msglen, buffer);
	if (DUMPABLE && !SUPPRESS_DUMP)
	{
		need_core = TRUE;
		gtm_fork_n_core();
	}
	DEBUG_ONLY(ok_to_UNWIND_in_exit_handling = TRUE);
	UNWIND(NULL, NULL);		/* This returns back to gvcst_remove_statsDB_linkage_all() to do next region */
}

/* For a given input basedb file name "fname", this function determines the corresponding "statsdb" filename and sets it
 * in the 2nd and 3rd parameters (name & byte-length). If no statsdb file name can be determined, *statsdb_fname_len is set to 0.
 * *statsdb_fname_len, at function entry time, is set to the allocated length of the statsdb_fname char array.
 */
void	gvcst_set_statsdb_fname(sgmnt_data_ptr_t csd, gd_region *baseDBreg, char *statsdb_fname, uint4 *statsdb_fname_len)
{
	boolean_t	fname_changed, statsdb_off;
	char		*basedb_fname, *baseBuf, *baseTop, *statsBuf, *statsTop;
	char		tmp_fname[MAX_FN_LEN + 1], save_basedb_fname[MAX_FN_LEN + 1];
	int		baseBufLen, save_basedb_fname_len, statsBufLen;
	int		int_status;
	key_t		hash_ftok;
	gd_segment	*baseDBseg;
	mstr		dbfile, trans, val;
	parse_blk	pblk;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Database file name could be relative path (e.g. mupip rundown -file mumps.dat or other mupip commands
	 * that require standalone access which specify a relative file name). Convert it to absolute path if possible.
	 * The ftok of the path to the basedb is needed by in order to derive the statsdb file name .
	 * Ignore any error exit status. In that case, continue with relative path.
	 */
	baseDBseg = baseDBreg->dyn.addr;
	basedb_fname = (char *)baseDBseg->fname;
	assert('\0' != basedb_fname[0]);
	if ('/' != basedb_fname[0])
	{
		fname_changed = TRUE;
		/* We need to make baseDBreg->dyn.addr->fname an absolute path temporarily. Save a copy and restore later */
		assert(ARRAYSIZE(save_basedb_fname) >= ARRAYSIZE(baseDBseg->fname));
		save_basedb_fname_len = baseDBseg->fname_len;
		assert(ARRAYSIZE(save_basedb_fname) > save_basedb_fname_len);
		memcpy(save_basedb_fname, baseDBseg->fname, save_basedb_fname_len + 1);	/* copy trailing '\0' too */
		mupfndfil(baseDBreg, NULL, LOG_ERROR_FALSE);	/* this will modify baseDBreg->dyn.addr->fname */
	} else
		fname_changed = FALSE;
	assert(ARRAYSIZE(tmp_fname) <= *statsdb_fname_len);	/* caller should have initialized it */
	statsdb_off = FALSE;
	TREF(statsdb_fnerr_reason) = FNERR_NOERR;
	for ( ; ; )	/* have a dummy for loop to be able to use "break" for various codepaths below */
	{
		if (RDBF_NOSTATS & csd->reservedDBFlags)
		{
			TREF(statsdb_fnerr_reason) = FNERR_NOSTATS;
			statsdb_off = TRUE;	/* This db does not have stats turned on (e.g. statsdb) */
			break;
		}
		/* This db has stats turned on. Store the full path name of the stats db when base db shm is created.
		 * The stats db file name is of the form "$gtm_statsdir/<hash>.BASEDB-FILE-NAME.gst" where "$gtm_statsdir"
		 * evaluates to a directory, <hash> is the ftok of the BASEDB-ABSOLUTE-PATH and BASEDB-FILE-NAME is the
		 * file name minus the path. For example, if the basedb is "/a/a.dat", the corresponding statsdb file name
		 * is "$gtm_statsdir/<hash>.a.dat.gst" where <hash> is the ftok of the directory "/a".
		 * Note: The stats db will be created later when it actually needs to be opened by a process that has
		 * opted in (VIEW STATSHARE or $gtm_statshare env var set).
		 */
		val.addr = GTM_STATSDIR;
		val.len = SIZEOF(GTM_STATSDIR) - 1;
		statsBuf = &tmp_fname[0];
		/* Note: "gtm_env_init_sp" already processed GTM_STATSDIR to make it default to GTM_TMP_ENV etc. */
		int_status = TRANS_LOG_NAME(&val, &trans, statsBuf, MAX_STATSDIR_LEN, do_sendmsg_on_log2long);
		if (SS_NORMAL != int_status)
		{
			assert(FALSE);	/* Same TRANS_LOG_NAME in "gtm_env_init_sp" succeeded so this cannot fail */
			TREF(statsdb_fnerr_reason) = FNERR_STATSDIR_TRNFAIL;
			statsdb_off = TRUE;
			break;
		}
		statsTop = statsBuf + MAX_FN_LEN;	/* leave room for '\0' at end (hence not "MAX_FN_LEN + 1") */
		statsBuf += trans.len;
		if ((statsBuf + 1) >= statsTop)
		{	/* Not enough space to store full-path file name of statsdb in basedb shm */
			assert(FALSE);
			TREF(statsdb_fnerr_reason) = FNERR_STATSDIR_TRN2LONG;
			statsdb_off = TRUE;
			break;
		}
		*statsBuf++ = '/';
		/* Now find the base db file name (minus the path) */
		baseBuf = strrchr(basedb_fname, '/');
		if (NULL == baseBuf)
		{
			assert(FALSE);	/* Since db file name is an absolute path, we should see at least one '/' */
			TREF(statsdb_fnerr_reason) = FNERR_INV_BASEDBFN;
			statsdb_off = TRUE;
			break;
		}
		*baseBuf = '\0';	/* temporarily modify basedb name to include just the directory */
		hash_ftok = FTOK(basedb_fname, GTM_ID);
		*baseBuf++ = '/';	/* restore basedb full path name */
		if (-1 == hash_ftok)
		{
			assert(FALSE);	/* possible only if parent dir of base db was deleted midway in db init */
			TREF(statsdb_fnerr_reason) = FNERR_FTOK_FAIL;
			statsdb_off = TRUE;
			break;
		}
		/* Now add the hash to the statsdb file name */
		if ((statsBuf + 9) >= statsTop)	/* 8 bytes for hex display of 4-byte ftok/hash, 1 byte for '.' */
		{	/* Not enough space to store full-path file name of statsdb in basedb shm */
			TREF(statsdb_fnerr_reason) = FNERR_FNAMEBUF_OVERFLOW;
			statsdb_off = TRUE;
			break;
		}
		SPRINTF(statsBuf, "%x", hash_ftok);
		statsBuf += 8;
		*statsBuf++ = '.';
		/* Now add the basedb file name + ".gst" extension to the statsdb file name */
		baseBufLen = STRLEN(baseBuf);
		if ((statsBuf + baseBufLen + STR_LIT_LEN(STATSDB_FNAME_SUFFIX)) > statsTop)
		{	/* Not enough space to store full-path file name of statsdb in basedb shm */
			TREF(statsdb_fnerr_reason) = FNERR_FNAMEBUF_OVERFLOW;
			statsdb_off = TRUE;
			break;
		}
		memcpy(statsBuf, baseBuf, baseBufLen);
		statsBuf += baseBufLen;
		MEMCPY_LIT(statsBuf, STATSDB_FNAME_SUFFIX);
		statsBuf += STR_LIT_LEN(STATSDB_FNAME_SUFFIX);
		*statsBuf = '\0';
		assert(statsBuf <= statsTop);
		statsBufLen = statsBuf - tmp_fname;
		statsBuf = &tmp_fname[0];
		/* Now call "parse_file" to translate any "." or ".." usages in the path */
		memcpy(tmp_fname, statsBuf, statsBufLen);
		dbfile.addr = tmp_fname;
		dbfile.len = statsBufLen;
		memset(&pblk, 0, SIZEOF(pblk));
		pblk.buffer = statsdb_fname;
		pblk.buff_size = (unsigned char)(MAX_FN_LEN);/* Pass buffersize - 1 (standard protocol for parse_file) */
		pblk.def1_buf = STATSDB_FNAME_SUFFIX;
		pblk.def1_size = STR_LIT_LEN(STATSDB_FNAME_SUFFIX);
		pblk.fop = F_SYNTAXO;			/* Syntax check only - bypass directory / file existence check. */
		int_status = parse_file(&dbfile, &pblk);
		if (!(int_status & 1))
		{	/* Some error in "parse_file". Likely not enough space to store full-path file name of statsdb. */
			assert(FALSE);
			TREF(statsdb_fnerr_reason) = FNERR_FNAMEBUF_OVERFLOW;
			statsdb_off = TRUE;
			break;
		}
		assert(pblk.b_esl < *statsdb_fname_len);
		*statsdb_fname_len = pblk.b_esl;
		pblk.buffer[pblk.b_esl] = 0;	/* null terminate "cnl->statsdb_fname" */
		break;
	}
	if (statsdb_off)
	{
		assert(0 < *statsdb_fname_len);
		*statsdb_fname_len = 0;
		statsdb_fname[0] = '\0';	/* turn off stats gathering for this base db */
	}
	if (fname_changed)
	{	/* Restore baseDBseg->fname and baseDBseg->fname_len */
		memcpy(baseDBseg->fname, save_basedb_fname, save_basedb_fname_len + 1);
		baseDBseg->fname_len = save_basedb_fname_len;
	}
	return;
}
