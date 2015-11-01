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

#ifndef __DSE_H__
#define __DSE_H__

#define PATCH_SAVE_SIZE 128

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

void dse_ctrlc_setup(void);
int dse_data(char *dst, int *len);
int dse_getki(char *dst, int *len, char *qual, int qual_len);
int dse_is_blk_in(sm_uc_ptr_t rp, sm_uc_ptr_t r_top, short size);
int dse_ksrch(block_id srch, block_id_ptr_t pp, int4 *off, char *targ_key, short targ_len);
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
void dse_chng_bhead(void);
void dse_chng_fhead(void);
void dse_chng_rhead(void);
void dse_crit(void);
int dse_dmp(void);
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
