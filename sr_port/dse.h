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

#ifndef __DSE_H__
#define __DSE_H__

error_def(ERR_DSEWCREINIT);

#define PATCH_SAVE_SIZE		128
#define DSE_DMP_TIME_FMT	"DD-MON-YEAR 24:60:SS"
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
			grab_crit(GV_CUR_REGION);								\
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
	if(!cli_get_str("CONFIRMATION",(X),&(Y)))					\
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
	if (CS_ADDRS->hdr->acc_meth == dba_bg)								\
		bt_refresh(CS_ADDRS, TRUE);								\
	db_csh_ref(CS_ADDRS, TRUE);									\
	send_msg_csa(CSA_ARG(CS_ADDRS) VARLSTCNT(4) ERR_DSEWCREINIT, 2, DB_LEN_STR(gv_cur_region));	\
}

void dse_ctrlc_setup(void);
int dse_data(char *dst, int *len);
int dse_getki(char *dst, int *len, char *qual, int qual_len);
int dse_is_blk_in(sm_uc_ptr_t rp, sm_uc_ptr_t r_top, short size);
int dse_ksrch(block_id srch, block_id_ptr_t pp, int4 *off, char *targ_key, int targ_len);
int dse_key_srch(block_id srch, block_id_ptr_t pp, int4 *off, char *targ_key, int targ_len);
int dse_order(block_id srch, block_id_ptr_t pp, int4 *op, char *targ_key, short int targ_len,
	bool dir_data_blk);
void  dse_rmsb(void);
void dse_ctrlc_handler(int sig);
void dse_exhaus(int4 pp, int4 op);
void dse_m_rest(block_id blk, unsigned char *bml_list, int4 bml_size, sm_vuint_ptr_t blks_ptr,
	bool in_dir_tree);
void dse_rmrec(void);
void dse_find_roots(block_id index);
boolean_t dse_fdmp(sm_uc_ptr_t data, int len);
boolean_t dse_fdmp_output(void *addr, int4 len);
void dse_adrec(void);
void dse_adstar(void);
void dse_all(void);
boolean_t dse_b_dmp(void);
void dse_cache(void);
void dse_chng_bhead(void);
void dse_chng_fhead(void);
void dse_chng_rhead(void);
void dse_crit(void);
void dse_dmp(void);
void dse_eval(void);
void dse_f_blk(void);
void dse_f_free(void);
void dse_f_key(void);
void dse_f_reg(void);
void dse_flush(void);
int parse_dlr_char(char *src, char *top, char *dlr_subsc);
void dse_help(void);
void dse_version(void);
void dse_integ(void);
bool dse_is_blk_free (block_id blk, sm_int_ptr_t cycle, cache_rec_ptr_ptr_t cr);
int4 dse_lm_blk_free(int4 blk, sm_uc_ptr_t base_addr);
void dse_maps(void);
void dse_open (void);
void dse_close(void);
void dse_over(void);
void dse_page(void);
boolean_t dse_r_dmp(void);
void dse_range(void);
void dse_rest(void);
void dse_save(void);
void dse_shift(void);
void dse_wcreinit (void);
sm_uc_ptr_t dump_record(sm_uc_ptr_t rp, block_id blk, sm_uc_ptr_t bp, sm_uc_ptr_t b_top);
void dse_dmp_fhead (void);
void dse_ctrlc_handler(int sig);
void dse_remove(void);

#endif
