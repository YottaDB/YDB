/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __DSE_H__
#define __DSE_H__

error_def(ERR_DSEWCREINIT);

#define BADDSEBLK		(block_id)-1
#define DSE_DMP_TIME_FMT	"DD-MON-YEAR 24:60:SS"
#define DSEBLKCUR		TRUE
#define DSEBLKNOCUR		FALSE
#define DSEBMLOK		FALSE
#define DSENOBML		TRUE
#define PATCH_SAVE_SIZE		128
#define SPAN_START_BYTE 	0x02
#define SPAN_BYTE_MAX  		255
#define SPAN_BYTE_MIN		1

#define	GET_CURR_TIME_IN_DOLLARH_AND_ZDATE(dollarh_mval, dollarh_buffer, zdate_mval, zdate_buffer)				\
{	/* gets current time in the mval "dollarh_mval" in dollarh format and in the mval "zdate_mval" in ZDATE format		\
	 * the ZDATE format string used is DSE_DMP_TIME_FMT									\
	 * the dollarh_buffer and zdate_buffer are buffers which the corresponding mvals use to store the actual time string	\
	 */															\
	GBLREF mval		dse_dmp_time_fmt;										\
	GBLREF spdesc		stringpool;											\
	LITREF mval		literal_null;											\
																\
	op_horolog(&dollarh_mval); /* returns $H value in stringpool */								\
	assert(SIZEOF(dollarh_buffer) >= dollarh_mval.str.len);									\
	/* if op_fnzdate (called below) calls stp_gcol, dollarh_mval might get corrupt because it is not known to stp_gcol.	\
	 * To prevent problems, copy from stringpool to local buffer */								\
	memcpy(dollarh_buffer, dollarh_mval.str.addr, dollarh_mval.str.len);							\
	dollarh_mval.str.addr = (char *)dollarh_buffer;										\
	stringpool.free -= dollarh_mval.str.len; /* now that we've made a copy, we don't need dollarh_mval in stringpool */	\
	op_fnzdate(&dollarh_mval, &dse_dmp_time_fmt, (mval *)&literal_null, (mval *)&literal_null, &zdate_mval);		\
		/* op_fnzdate() returns zdate formatted string in stringpool */							\
	assert(SIZEOF(zdate_buffer) >= zdate_mval.str.len);									\
	/* copy over stringpool string into local buffer to ensure zdate_mval will not get corrupt */				\
	memcpy(zdate_buffer, zdate_mval.str.addr, zdate_mval.str.len);								\
	zdate_mval.str.addr = (char *)zdate_buffer;										\
	stringpool.free -= zdate_mval.str.len; /* now that we've made a copy, we don't need zdate_mval in stringpool anymore */	\
}

typedef struct
{
	block_id	blk;
	char		*bp;
	gd_region	*region;
	char		*comment;
	short int	ver;
} save_strct;

enum dse_fmt
{
	CLOSED_FMT = 0,
	GLO_FMT,
	ZWR_FMT,
	OPEN_FMT
};

/* Grab crit for dse* functions taking into account -nocrit if specified */
#define	DSE_GRAB_CRIT_AS_APPROPRIATE(WAS_CRIT, WAS_HOLD_ONTO_CRIT, NOCRIT_PRESENT, CS_ADDRS, GV_CUR_REGION)	\
{														\
	if (!WAS_CRIT)												\
	{													\
		if (NOCRIT_PRESENT)										\
			CS_ADDRS->now_crit = TRUE;								\
		else												\
			grab_crit_encr_cycle_sync(GV_CUR_REGION);						\
		WAS_HOLD_ONTO_CRIT = CS_ADDRS->hold_onto_crit;							\
		CS_ADDRS->hold_onto_crit = TRUE;								\
	}													\
}

/* Rel crit for dse* functions taking into account -nocrit if specified */
#define	DSE_REL_CRIT_AS_APPROPRIATE(WAS_CRIT, WAS_HOLD_ONTO_CRIT, NOCRIT_PRESENT, CS_ADDRS, GV_CUR_REGION)	\
{														\
	if (!WAS_CRIT)												\
	{													\
		assert(CS_ADDRS->hold_onto_crit);								\
		assert((TRUE == WAS_HOLD_ONTO_CRIT) || (FALSE == WAS_HOLD_ONTO_CRIT));				\
		CS_ADDRS->hold_onto_crit = WAS_HOLD_ONTO_CRIT;							\
		if (NOCRIT_PRESENT)										\
			CS_ADDRS->now_crit = FALSE;								\
		else												\
			rel_crit(GV_CUR_REGION);								\
	}													\
}

#ifdef UNIX
# define GET_CONFIRM(X, Y)								\
{											\
	PRINTF("CONFIRMATION: ");							\
	FGETS((X), (Y), stdin, fgets_res);						\
	Y = strlen(X);									\
}
#else
# define GET_CONFIRM(X, Y)								\
{											\
	if (!cli_get_str("CONFIRMATION", (X), &(Y)))					\
	{										\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_DSEWCINITCON);		\
		return;									\
	}										\
}
#endif

#define GET_CONFIRM_AND_HANDLE_NEG_RESPONSE						\
{											\
	int		len;								\
	char		confirm[256];							\
											\
	len = SIZEOF(confirm);								\
	GET_CONFIRM(confirm, len);							\
	if (confirm[0] != 'Y' && confirm[0] != 'y')					\
	{										\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_DSEWCINITCON);		\
		return;									\
	}										\
}

#define DSE_WCREINIT(CS_ADDRS)										\
{													\
	assert(CS_ADDRS->now_crit);									\
	if (dba_bg == CS_ADDRS->hdr->acc_meth)								\
		bt_refresh(CS_ADDRS, TRUE);								\
	db_csh_ref(CS_ADDRS, TRUE);									\
	send_msg_csa(CSA_ARG(CS_ADDRS) VARLSTCNT(4) ERR_DSEWCREINIT, 2, DB_LEN_STR(gv_cur_region));	\
}

/* This macro is currently used only inside the BUILD_AIMG_IF_JNL_ENABLED_AND_T_END_WITH_EFFECTIVE_TN macro */
#define	BUILD_ENCRYPT_TWINBUFF_IF_NEEDED(CSA, CSD, TN)										\
{																\
	blk_hdr_ptr_t		bp, save_bp;											\
	gd_segment		*seg;												\
	int			gtmcrypt_errno, req_enc_blk_size;								\
	boolean_t		use_new_key;											\
																\
	GBLREF	gd_region	*gv_cur_region;											\
																\
	if (USES_ENCRYPTION(CSD->is_encrypted) && (TN < CSA->ti->curr_tn))							\
	{	/* BG and db encryption are enabled and the DSE update caused the block-header to potentially have a tn LESS	\
		 * than before. At this point, the global buffer (corresponding to blkhist.blk_num) reflects the contents of	\
		 * the block AFTER the dse update (bg_update would have touched this), whereas the corresponding encryption	\
		 * global buffer reflects the contents of the block BEFORE the update.						\
		 *														\
		 * Normally, wcs_wtstart would take care of propagating the tn update from the regular global buffer to the	\
		 * corresponding encryption buffer. But if before it gets a chance, a process goes to t_end as a part of a	\
		 * subsequent transaction and updates this same block, then, since the blk-hdr-tn potentially decreased, it is	\
		 * possible that the PBLK writing check (that compares blk-hdr-tn with the epoch_tn) will decide to write a	\
		 * PBLK for this block, even though a PBLK was already written for this block as part of a previous		\
		 * DSE CHANGE -BL -TN in the same epoch. In this case, since the db is encrypted, the logic will assume	that	\
		 * there were no updates to this block because the last time wcs_wtstart updated the encryption buffer and,	\
		 * therefore, use that to write the PBLK, which is incorrect, for it does not yet contain the tn update.	\
		 * 														\
		 * The consequence of this is that we would be writing an older before-image PBLK record to the journal file.	\
		 * To prevent this situation, we update the encryption buffer here (before releasing crit) using logic similar	\
		 * to that in wcs_wtstart, to ensure it is in sync with the regular global buffer. To prevent t_end from	\
		 * releasing crit, we set CSA->hold_onto_crit to TRUE.								\
		 * 														\
		 * Note that although we use cw_set[0] to access the global buffer corresponding to the block number being	\
		 * updated, cw_set_depth at this point is 0 because t_end resets it. This is considered safe since cw_set is a	\
		 * static array (as opposed to malloced memory) and hence is always available and valid until it gets		\
		 * overwritten by subsequent updates.										\
		 */														\
		bp = (blk_hdr_ptr_t)GDS_ANY_REL2ABS(CSA, cw_set[0].cr->buffaddr);						\
		DBG_ENSURE_PTR_IS_VALID_GLOBUFF(CSA, CSD, (sm_uc_ptr_t)bp);							\
		save_bp = (blk_hdr_ptr_t)GDS_ANY_ENCRYPTGLOBUF(bp, CSA);							\
		DBG_ENSURE_PTR_IS_VALID_ENCTWINGLOBUFF(CSA, CSD, (sm_uc_ptr_t)save_bp);						\
		assert((bp->bsiz <= CSD->blk_size) && (bp->bsiz >= SIZEOF(*bp)));						\
		req_enc_blk_size = MIN(CSD->blk_size, bp->bsiz) - SIZEOF(*bp);							\
		if (BLK_NEEDS_ENCRYPTION(bp->levl, req_enc_blk_size))								\
		{														\
			ASSERT_ENCRYPTION_INITIALIZED;										\
			memcpy(save_bp, bp, SIZEOF(blk_hdr));									\
			use_new_key = USES_NEW_KEY(CSD);									\
			GTMCRYPT_ENCRYPT(CSA, (use_new_key ? TRUE : CSD->non_null_iv),						\
					(use_new_key ? CSA->encr_key_handle2 : CSA->encr_key_handle),				\
					(char *)(bp + 1), req_enc_blk_size, (char *)(save_bp + 1),				\
					bp, SIZEOF(blk_hdr), gtmcrypt_errno);							\
			if (0 != gtmcrypt_errno)										\
			{													\
				seg = gv_cur_region->dyn.addr;									\
				GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, gtm_putmsg, seg->fname_len, seg->fname);			\
			}													\
		} else														\
			memcpy(save_bp, bp, bp->bsiz);										\
	}															\
}

/* This macro is used whenever t_end needs to be invoked with a 3rd parameter != TN_NOT_SPECIFIED.
 * Currently the only two usages of this are from DSE and hence this macro is placed in dse.h.
 */
#define	BUILD_AIMG_IF_JNL_ENABLED_AND_T_END_WITH_EFFECTIVE_TN(CSA, CSD, CTN, HIST)					\
{															\
	trans_num		cTn;											\
	boolean_t		was_hold_onto_crit;									\
															\
	GBLREF	gd_region	*gv_cur_region;										\
	GBLREF	boolean_t	unhandled_stale_timer_pop;								\
	GBLREF	sgmnt_addrs	*cs_addrs;										\
															\
	cTn = CTN;													\
	assert(CSA == cs_addrs);											\
	BUILD_AIMG_IF_JNL_ENABLED(CSD, cTn);										\
	was_hold_onto_crit = CSA->hold_onto_crit;									\
	CSA->hold_onto_crit = TRUE; /* need this so t_end doesn't release crit (see below comment for why) */		\
	t_end(HIST, NULL, cTn);												\
	BUILD_ENCRYPT_TWINBUFF_IF_NEEDED(CSA, CSD, cTn);								\
	CSA->hold_onto_crit = was_hold_onto_crit;									\
	if (!was_hold_onto_crit)											\
		rel_crit(gv_cur_region);										\
	if (unhandled_stale_timer_pop)											\
		process_deferred_stale();										\
}

#define	CLEAR_DSE_COMPRESS_KEY						\
MBSTART {								\
	GBLREF char	patch_comp_key[MAX_KEY_SZ + 1];			\
	GBLREF unsigned short   patch_comp_count;			\
									\
	patch_comp_count = patch_comp_key[0] = patch_comp_key[1] = 0;	\
} MBEND

void		dse_adrec(void);
void		dse_adstar(void);
void		dse_all(void);
boolean_t	dse_b_dmp(void);
void		dse_cache(void);
void		dse_chng_bhead(void);
void		dse_chng_fhead(void);
void		dse_chng_rhead(void);
void		dse_close(void);
void		dse_crit(void);
void		dse_ctrlc_handler(int sig);
void		dse_ctrlc_setup(void);
int		dse_data(char *dst, int *len);
void		dse_dmp(void);
void		dse_dmp_fhead (void);
void		dse_eval(void);
void		dse_exhaus(int4 pp, int4 op);
void		dse_f_blk(void);
void		dse_f_free(void);
void		dse_f_key(void);
void		dse_f_reg(void);
boolean_t	dse_fdmp(sm_uc_ptr_t data, int len);
boolean_t	dse_fdmp_output(void *addr, int4 len);
gv_namehead	*dse_find_gvt(gd_region *reg, char *name, int name_len);
void		dse_find_roots(block_id index);
void		dse_flush(void);
block_id	dse_getblk(char *element, boolean_t nobml, boolean_t carry_curr);
int		dse_getki(char *dst, int *len, char *qual, int qual_len);
void		dse_help(void);
void		dse_integ(void);
boolean_t	dse_is_blk_free(block_id blk, sm_int_ptr_t cycle, cache_rec_ptr_ptr_t cr);
int		dse_is_blk_in(sm_uc_ptr_t rp, sm_uc_ptr_t r_top, short size);
int		dse_ksrch(block_id srch, block_id_ptr_t pp, int4 *off, char *targ_key, int targ_len);
int4		dse_lm_blk_free(int4 blk, sm_uc_ptr_t base_addr);
void		dse_m_rest(block_id blk, unsigned char *bml_list, int4 bml_size, sm_vuint_ptr_t blks_ptr, bool in_dir_tree);
void		dse_maps(void);
void		dse_open (void);
int		dse_order(block_id srch, block_id_ptr_t pp, int4 *op, char *targ_key, short int targ_len, bool dir_data_blk);
void		dse_over(void);
void		dse_page(void);
boolean_t	dse_r_dmp(void);
void		dse_range(void);
void		dse_remove(void);
void		dse_rest(void);
void		dse_rmrec(void);
void		dse_rmsb(void);
void		dse_save(void);
void		dse_shift(void);
void		dse_version(void);
void		dse_wcreinit (void);
sm_uc_ptr_t	dump_record(sm_uc_ptr_t rp, block_id blk, sm_uc_ptr_t bp, sm_uc_ptr_t b_top);
int		parse_dlr_char(char *src, char *top, char *dlr_subsc);

#endif
