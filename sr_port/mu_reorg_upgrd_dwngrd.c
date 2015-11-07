/****************************************************************
 *								*
 *	Copyright 2005, 2013 Fidelity Information Services, Inc	*
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
#include "gdsbml.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblkops.h"
#include "filestruct.h"
#include "error.h"
#include "gdscc.h"
#include "gdskill.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "cli.h"
#include "iosp.h"		/* for SS_NORMAL */
#include "min_max.h"		/* for MAX macro */

/* Prototypes */
#include "cert_blk.h"
#include "desired_db_format_set.h"
#include "gtmmsg.h"		/* for gtm_putmsg prototype */
#include "mu_getlst.h"
#include "mu_reorg_upgrd_dwngrd.h"
#include "mupip_exit.h"
#include "send_msg.h"		/* for send_msg */
#include "t_begin.h"
#include "t_end.h"
#include "t_qread.h"
#include "t_retry.h"
#include "t_write.h"
#include "t_write_map.h"
#include "targ_alloc.h"
#include "util.h"		/* for util_out_print prototype */
#include "wcs_flu.h"

#define	REORG_CONTINUE	1
#define	REORG_BREAK	2

LITREF	char			*gtm_dbversion_table[];

GBLREF	char			*update_array, *update_array_ptr;	/* for the BLK_INIT/BLK_SEG/BLK_ADDR macros */
GBLREF	uint4			update_array_size;			/* for the BLK_INIT/BLK_SEG/BLK_ADDR macros */
GBLREF	bool			error_mupip;
GBLREF	int4			gv_keysize;
GBLREF	bool			mu_ctrlc_occurred;
GBLREF	bool			mu_ctrly_occurred;
GBLREF	uint4			process_id;
GBLREF	tp_region		*grlist;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	gv_namehead		*gv_target;
GBLREF	gv_namehead		*gv_target_list;
GBLREF	inctn_opcode_t		inctn_opcode;
GBLREF	inctn_detail_t		inctn_detail;			/* holds detail to fill in to inctn jnl record */
GBLREF	boolean_t		mu_reorg_nosafejnl;		/* TRUE if NOSAFEJNL explicitly specified */
GBLREF	trans_num		mu_reorg_upgrd_dwngrd_blktn;	/* tn in blkhdr of current block processed by REORG UP/DOWNGRADE */
GBLREF	boolean_t		mu_reorg_upgrd_dwngrd_in_prog;	/* TRUE if MUPIP REORG UPGRADE/DOWNGRADE is in progress */
GBLREF	unsigned char		rdfail_detail;
GBLREF	unsigned char		t_fail_hist[CDB_MAX_TRIES];
GBLREF 	unsigned int		t_tries;
GBLREF	cw_set_element		cw_set[];		/* create write set. */
GBLREF	unsigned char		cw_set_depth;
GBLREF	unsigned char    	cw_map_depth;
GBLREF	uint4			update_trans;

error_def(ERR_BUFFLUFAILED);
error_def(ERR_DBBTUWRNG);
error_def(ERR_DBFILERR);
error_def(ERR_DBRDONLY);
error_def(ERR_DSEBLKRDFAIL);
error_def(ERR_DYNUPGRDFAIL);
error_def(ERR_MUNOACTION);
error_def(ERR_MUNOFINISH);
error_def(ERR_MUREORGFAIL);
error_def(ERR_MUREUPDWNGRDEND);
error_def(ERR_REORGCTRLY);

/* actually want the following to be a static variable in this module, but getting the address of a
 * static variable through the debugger might be tricky on some platforms. hence use a global variable instead.
 */
GBLDEF	trans_num		mu_reorg_upgrd_dwngrd_start_tn;

typedef struct
{
	int4	blks_read_from_disk_bmp;
	int4	blks_skipped_free;
	int4	blks_read_from_disk_nonbmp;
	int4	blks_skipped_newfmtindisk;
	int4	blks_skipped_newfmtincache;
	int4	blks_converted_bmp;
	int4	blks_converted_nonbmp;
} reorg_stats_t;

void	mu_reorg_upgrd_dwngrd(void)
{
	blk_hdr			new_hdr;
	blk_segment		*bs1, *bs_ptr;
	block_id		*blkid_ptr, curblk, curbmp, start_blk, stop_blk, start_bmp, last_bmp;
	block_id		startblk_input, stopblk_input;
	boolean_t		upgrade, downgrade, safejnl, nosafejnl, region, first_reorg_in_this_db_fmt, reorg_entiredb;
	boolean_t		startblk_specified, stopblk_specified, set_fully_upgraded, db_got_to_v5_once, mark_blk_free;
	cache_rec_ptr_t		cr;
	char			*bml_lcl_buff = NULL, *command, *reorg_command;
	sm_uc_ptr_t		bptr = NULL;
	cw_set_element		*cse;
	enum cdb_sc		cdb_status;
	enum db_ver		new_db_format, ondsk_blkver;
	gd_region		*reg;
	int			cycle;
	int4			blk_seg_cnt, blk_size;	/* needed for BLK_INIT,BLK_SEG and BLK_FINI macros */
	int4			blocks_left, expected_blks2upgrd, actual_blks2upgrd, total_blks, free_blks;
	int4			status, status1, mapsize, lcnt, bml_status;
	reorg_stats_t		reorg_stats;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	sm_uc_ptr_t		blkBase, bml_sm_buff;	/* shared memory pointer to the bitmap global buffer */
	srch_hist		alt_hist;
	srch_blk_status		*blkhist, bmlhist;
	tp_region		*rptr;
	trans_num		curr_tn;
	unsigned char    	save_cw_set_depth;
	uint4			lcl_update_trans;

	region    = (CLI_PRESENT == cli_present("REGION"));
	upgrade   = (CLI_PRESENT == cli_present("UPGRADE"));
	downgrade = (CLI_PRESENT == cli_present("DOWNGRADE"));
	assert(upgrade && !downgrade || !upgrade && downgrade);
	command = upgrade ? "UPGRADE" : "DOWNGRADE";
	reorg_command = upgrade ? "MUPIP REORG UPGRADE" : "MUPIP REORG DOWNGRADE";
	reorg_entiredb = TRUE;	/* unless STARTBLK or STOPBLK is specified we are going to {up,down}grade the entire database */
	startblk_specified = FALSE;
	assert(SIZEOF(block_id) == SIZEOF(uint4));
	if ((CLI_PRESENT == cli_present("STARTBLK")) && (cli_get_hex("STARTBLK", (uint4 *)&startblk_input)))
	{
		reorg_entiredb = FALSE;
		startblk_specified = TRUE;
	}
	stopblk_specified = FALSE;
	assert(SIZEOF(block_id) == SIZEOF(uint4));
	if ((CLI_PRESENT == cli_present("STOPBLK")) && (cli_get_hex("STOPBLK", (uint4 *)&stopblk_input)))
	{
		reorg_entiredb = FALSE;
		stopblk_specified = TRUE;
	}
	mu_reorg_upgrd_dwngrd_in_prog = TRUE;
	mu_reorg_nosafejnl = (CLI_NEGATED == cli_present("SAFEJNL")) ? TRUE : FALSE;

	assert(region);
	status = SS_NORMAL;
	error_mupip = FALSE;
	gvinit();	/* initialize gd_header (needed by the later call to mu_getlst) */
	mu_getlst("REG_NAME", SIZEOF(tp_region));	/* get the parameter corresponding to REGION qualifier */
	if (error_mupip)
	{
		util_out_print("!/MUPIP REORG !AD cannot proceed with above errors!/", TRUE, LEN_AND_STR(command));
		mupip_exit(ERR_MUNOACTION);
	}
	GVKEYSIZE_INCREASE_IF_NEEDED(DBKEYSIZE(MAX_KEY_SZ)); /* Keep gv_currkey/gv_altkey in sync with respect to gv_keysize
							      * (now MAX_KEY_SZ) */
	assert(DBKEYSIZE(MAX_KEY_SZ) == gv_keysize);
	gv_target = targ_alloc(gv_keysize, NULL, NULL);	/* t_begin needs this initialized */
	gv_target_list = NULL;
	memset(&alt_hist, 0, SIZEOF(alt_hist));	/* null-initialize history */
	blkhist = &alt_hist.h[0];
	for (rptr = grlist;  NULL != rptr;  rptr = rptr->fPtr)
	{
		if (mu_ctrly_occurred || mu_ctrlc_occurred)
			break;
		reg = rptr->reg;
		util_out_print("!/Region !AD : MUPIP REORG !AD started", TRUE, REG_LEN_STR(reg), LEN_AND_STR(command));
		if (reg_cmcheck(reg))
		{
			util_out_print("Region !AD : MUPIP REORG !AD cannot run across network",
				TRUE, REG_LEN_STR(reg), LEN_AND_STR(command));
			status = ERR_MUNOFINISH;
			continue;
		}
		gvcst_init(reg);
		assert(update_array != NULL);
		/* access method stored in global directory and database file header might be different in which case
		 * the database setting prevails. therefore, the access method check can be done only after opening
		 * the database (i.e. after the gvcst_init)
		 */
		if (dba_bg != reg->dyn.addr->acc_meth)
		{
			util_out_print("Region !AD : MUPIP REORG !AD cannot continue as access method is not BG",
				TRUE, REG_LEN_STR(reg), LEN_AND_STR(command));
			status = ERR_MUNOFINISH;
			continue;
		}
		if (reg->was_open)	/* Already open under another name.  Region will not be marked open */
		{
			reg->open = FALSE;
			continue;
		}
		TP_CHANGE_REG(reg);	/* sets gv_cur_region, cs_addrs, cs_data */
		csa = cs_addrs;
		csd = cs_data;
		blk_size = csd->blk_size;	/* "blk_size" is used by the BLK_FINI macro */
		if (reg->read_only)
		{
			gtm_putmsg(VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(reg));
			status = ERR_MUNOFINISH;
			continue;
		}
		assert(GDSVCURR == GDSV6); /* so we trip this assert in case GDSVCURR changes without a change to this module */
		new_db_format = (upgrade ? GDSV6 : GDSV4);
		grab_crit(reg);
		curr_tn = csd->trans_hist.curr_tn;
		/* set the desired db format in the file header to the appropriate version, increment transaction number */
		status1 = desired_db_format_set(reg, new_db_format, reorg_command);
		assert(csa->now_crit);	/* desired_db_format_set() should not have released crit */
		first_reorg_in_this_db_fmt = TRUE;	/* with the current desired_db_format, this is the first reorg */
		if (SS_NORMAL != status1)
		{	/* "desired_db_format_set" would have printed appropriate error messages */
			if (ERR_MUNOACTION != status1)
			{	/* real error occurred while setting the db format. skip to next region */
				status = ERR_MUNOFINISH;
				rel_crit(reg);
				continue;
			}
			util_out_print("Region !AD : Desired DB Format remains at !AD after !AD", TRUE, REG_LEN_STR(reg),
				LEN_AND_STR(gtm_dbversion_table[new_db_format]), LEN_AND_STR(reorg_command));
			if (csd->reorg_db_fmt_start_tn == csd->desired_db_format_tn)
				first_reorg_in_this_db_fmt = FALSE;
		} else
			util_out_print("Region !AD : Desired DB Format set to !AD by !AD", TRUE, REG_LEN_STR(reg),
				LEN_AND_STR(gtm_dbversion_table[new_db_format]), LEN_AND_STR(reorg_command));
		assert(dba_bg == csd->acc_meth);
		/* Check blks_to_upgrd counter to see if upgrade/downgrade is complete */
		total_blks = csd->trans_hist.total_blks;
		free_blks = csd->trans_hist.free_blocks;
		actual_blks2upgrd = csd->blks_to_upgrd;
		/* If MUPIP REORG UPGRADE and there is no block to upgrade in the database as indicated by BOTH
		 * 	"csd->blks_to_upgrd" and "csd->fully_upgraded", then we can skip processing.
		 * If MUPIP REORG UPGRADE and all non-free blocks need to be upgraded then again we can skip processing.
		 */
		if ((upgrade && (0 == actual_blks2upgrd) && csd->fully_upgraded)
			|| (!upgrade && ((total_blks - free_blks) == actual_blks2upgrd)))
		{
			util_out_print("Region !AD : Blocks to Upgrade counter indicates no action needed for MUPIP REORG !AD",
				       TRUE, REG_LEN_STR(reg), LEN_AND_STR(command));
			util_out_print("Region !AD : Total Blocks = [0x!XL] : Free Blocks = [0x!XL] : "
				       "Blocks to upgrade = [0x!XL]",
				       TRUE, REG_LEN_STR(reg), total_blks, free_blks, actual_blks2upgrd);
			util_out_print("Region !AD : MUPIP REORG !AD finished!/", TRUE, REG_LEN_STR(reg), LEN_AND_STR(command));
			rel_crit(reg);
			continue;
		}
		stop_blk = total_blks;
		if (stopblk_specified && stopblk_input <= stop_blk)
			stop_blk = stopblk_input;
		if (first_reorg_in_this_db_fmt)
		{	/* Note down reorg start tn (in case we are interrupted, future reorg will know to resume) */
			csd->reorg_db_fmt_start_tn = csd->desired_db_format_tn;
			csd->reorg_upgrd_dwngrd_restart_block = 0;
			start_blk = (startblk_specified ? startblk_input : 0);
		} else
		{	/* Either a concurrent MUPIP REORG of the same type ({up,down}grade) is currently running
			 * or a previously running REORG of the same type was interrupted (Ctrl-Ced).
			 * In either case resume processing from whatever restart block number is stored in fileheader
			 * the only exception is if "STARTBLK" was specified in the input in which use that unconditionally.
			 */
			start_blk = (startblk_specified ? startblk_input : csd->reorg_upgrd_dwngrd_restart_block);
		}
		if (start_blk > stop_blk)
			start_blk = stop_blk;
		mu_reorg_upgrd_dwngrd_start_tn = csd->reorg_db_fmt_start_tn;
		/* Before releasing crit, flush the file-header and dirty buffers in cache to disk. This is because we are now
		 * going to read each GDS block directly from disk to determine if it needs to be upgraded/downgraded or not.
		 */
		if (!wcs_flu(WCSFLU_FLUSH_HDR))	/* wcs_flu assumes gv_cur_region is set (which it is in this routine) */
		{
			rel_crit(reg);
			gtm_putmsg(VARLSTCNT(6) ERR_BUFFLUFAILED, 4, LEN_AND_LIT("MUPIP REORG UPGRADE/DOWNGRADE"), DB_LEN_STR(reg));
			status = ERR_MUNOFINISH;
			continue;
		}
		rel_crit(reg);
		/* Loop through entire database one GDS block at a time and upgrade/downgrade each of them */
		status1 = SS_NORMAL;
		start_bmp = ROUND_DOWN2(start_blk, BLKS_PER_LMAP);
		last_bmp  = ROUND_DOWN2(stop_blk - 1, BLKS_PER_LMAP);
		curblk = start_blk;	/* curblk is the block to be upgraded/downgraded */
		util_out_print("Region !AD : Started processing from block number [0x!XL]", TRUE, REG_LEN_STR(reg), curblk);
		if (NULL != bptr)
		{	/* malloc/free "bptr" for each region as GDS block-size can be different */
			free(bptr);
			bptr = NULL;
		}
		memset(&reorg_stats, 0, SIZEOF(reorg_stats));	/* initialize statistics for this region */
		for (curbmp = start_bmp; curbmp <= last_bmp; curbmp += BLKS_PER_LMAP)
		{
			if (mu_ctrly_occurred || mu_ctrlc_occurred)
			{
				status1 = ERR_MUNOFINISH;
				break;
			}
			/* --------------------------------------------------------------
			 *             Read in current bitmap block
			 * --------------------------------------------------------------
			 */
			assert(!csa->now_crit);
			bml_sm_buff = t_qread(curbmp, (sm_int_ptr_t)&cycle, &cr); /* bring block into the cache outside of crit */
			reorg_stats.blks_read_from_disk_bmp++;
			grab_crit(reg);
			if (mu_reorg_upgrd_dwngrd_start_tn != csd->desired_db_format_tn)
			{	/* csd->desired_db_format changed since reorg started. discontinue the reorg */
				/* see later comment on "csd->reorg_upgrd_dwngrd_restart_block" for why the assignment
				 * of this field should be done only if a db format change did not occur.
				 */
				rel_crit(reg);
				status1 = ERR_MUNOFINISH;
				/* This "start_tn" check is redone after the for-loop and an error message is printed there */
				break;
			} else if (reorg_entiredb)
			{	/* Change "csd->reorg_upgrd_dwngrd_restart_block" only if STARTBLK or STOPBLK was NOT specified */
				assert(csd->reorg_upgrd_dwngrd_restart_block <= MAX(start_blk, curbmp));
				csd->reorg_upgrd_dwngrd_restart_block = curbmp;	/* previous blocks have been upgraded/downgraded */
			}
			/* Check blks_to_upgrd counter to see if upgrade/downgrade is complete.
			 * Repeat check done a few steps earlier outside of this for loop.
			 */
			total_blks = csd->trans_hist.total_blks;
			free_blks = csd->trans_hist.free_blocks;
			actual_blks2upgrd = csd->blks_to_upgrd;
			if ((upgrade && (0 == actual_blks2upgrd) && csd->fully_upgraded)
				|| (!upgrade && ((total_blks - free_blks) == actual_blks2upgrd)))
			{
				rel_crit(reg);
				break;
			}
			bml_sm_buff = t_qread(curbmp, (sm_int_ptr_t)&cycle, &cr); /* now that in crit, note down stable buffer */
			if (NULL == bml_sm_buff)
				rts_error(VARLSTCNT(1) ERR_DSEBLKRDFAIL);
			ondsk_blkver = cr->ondsk_blkver;	/* note down db fmt on disk for bitmap block */
			/* Take a copy of the shared memory bitmap buffer into process-private memory before releasing crit.
			 * We are interested in those blocks that are currently marked as USED in the bitmap.
			 * It is possible that once we release crit, concurrent updates change the bitmap state of those blocks.
			 * In that case, those updates will take care of doing the upgrade/downgrade of those blocks in the
			 * format currently set in csd->desired_db_format i.e. accomplishing MUPIP REORG UPGRADE/DOWNGRADE's job.
			 * If the desired_db_format changes concurrently, we will stop doing REORG UPGRADE/DOWNGRADE processing.
			 */
			if (NULL == bml_lcl_buff)
				bml_lcl_buff = malloc(BM_SIZE(BLKS_PER_LMAP));
			memcpy(bml_lcl_buff, (blk_hdr_ptr_t)bml_sm_buff, BM_SIZE(BLKS_PER_LMAP));
			if (FALSE == cert_blk(reg, curbmp, (blk_hdr_ptr_t)bml_lcl_buff, 0, FALSE))
			{	/* certify the block while holding crit as cert_blk uses fields from file-header (shared memory) */
				assert(FALSE);	/* in pro, skip ugprading/downgarding all blks in this unreliable local bitmap */
				rel_crit(reg);
				util_out_print("Region !AD : Bitmap Block [0x!XL] has integrity errors. Skipping this bitmap.",
					TRUE, REG_LEN_STR(reg), curbmp);
				status1 = ERR_MUNOFINISH;
				continue;
			}
			rel_crit(reg);
			/* ------------------------------------------------------------------------
			 *         Upgrade/Downgrade all BUSY blocks in the current bitmap
			 * ------------------------------------------------------------------------
			 */
			curblk = (curbmp == start_bmp) ? start_blk : curbmp;
			mapsize = (curbmp == last_bmp) ? (stop_blk - curbmp) : BLKS_PER_LMAP;
			assert(0 != mapsize);
			assert(mapsize <= BLKS_PER_LMAP);
			db_got_to_v5_once = csd->db_got_to_v5_once;
			for (lcnt = curblk - curbmp; lcnt < mapsize; lcnt++, curblk++)
			{
				if (mu_ctrly_occurred || mu_ctrlc_occurred)
				{
					status1 = ERR_MUNOFINISH;
					goto stop_reorg_on_this_reg;	/* goto needed because of nested FOR Loop */
				}
				GET_BM_STATUS(bml_lcl_buff, lcnt, bml_status);
				assert(BLK_MAPINVALID != bml_status); /* cert_blk ran clean so we dont expect invalid entries */
				if (BLK_FREE == bml_status)
				{
					reorg_stats.blks_skipped_free++;
					continue;
				}
				/* MUPIP REORG UPGRADE/DOWNGRADE will convert USED & RECYCLED blocks */
				if (db_got_to_v5_once || (BLK_RECYCLED != bml_status))
				{	/* Do NOT read recycled V4 block from disk unless it is guaranteed NOT to be too full */
					if (lcnt)
					{	/* non-bitmap block */
						/* read in block from disk into private buffer. dont pollute the cache yet */
						if (NULL == bptr)
							bptr = (sm_uc_ptr_t)malloc(blk_size);
						status1 = dsk_read(curblk, bptr, &ondsk_blkver, FALSE);
						/* dsk_read on curblk could return an error (DYNUPGRDFAIL) if curblk needs to be
						 * upgraded and if its block size was too big to allow the extra block-header space
						 * requirements for a dynamic upgrade. a MUPIP REORG DOWNGRADE should not error out
						 * in that case as the block is already in the downgraded format.
						 */
						if (SS_NORMAL != status1)
						{
							if (!upgrade && (ERR_DYNUPGRDFAIL == status1))
							{
								assert(GDSV4 == new_db_format);
								ondsk_blkver = new_db_format;
							} else
							{
								gtm_putmsg(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), status1);
								util_out_print("Region !AD : Error occurred while reading block "
									"[0x!XL]", TRUE, REG_LEN_STR(reg), curblk);
								status1 = ERR_MUNOFINISH;
								goto stop_reorg_on_this_reg;/* goto needed due to nested FOR Loop */
							}
						}
						reorg_stats.blks_read_from_disk_nonbmp++;
					} /* else bitmap block has been read in crit earlier and ondsk_blkver appropriately set */
					if (new_db_format == ondsk_blkver)
					{
						assert((SS_NORMAL == status1) || (!upgrade && (ERR_DYNUPGRDFAIL == status1)));
						status1 = SS_NORMAL;	/* treat DYNUPGRDFAIL as no error in case of downgrade */
						reorg_stats.blks_skipped_newfmtindisk++;
						continue;	/* current disk version is identical to what is desired */
					}
					assert(SS_NORMAL == status1);
				}
				/* Begin non-TP transaction to upgrade/downgrade the block.
				 * The way we do that is by updating the block using a null update array.
				 * Any update to a block will trigger an automatic upgrade/downgrade of the block based on
				 * 	the current fileheader desired_db_format setting and we use that here.
				 */
				t_begin(ERR_MUREORGFAIL, UPDTRNS_DB_UPDATED_MASK);
				for (; ;)
				{
					CHECK_AND_RESET_UPDATE_ARRAY;	/* reset update_array_ptr to update_array */
					curr_tn = csd->trans_hist.curr_tn;
					db_got_to_v5_once = csd->db_got_to_v5_once;
					if (db_got_to_v5_once || (BLK_RECYCLED != bml_status))
					{
						blkhist->cse = NULL;	/* start afresh (do not use value from previous retry) */
						blkBase = t_qread(curblk, (sm_int_ptr_t)&blkhist->cycle, &blkhist->cr);
						if (NULL == blkBase)
						{
							t_retry((enum cdb_sc)rdfail_detail);
							continue;
						}
						blkhist->blk_num = curblk;
						blkhist->buffaddr = blkBase;
						ondsk_blkver = blkhist->cr->ondsk_blkver;
						new_hdr = *(blk_hdr_ptr_t)blkBase;
						mu_reorg_upgrd_dwngrd_blktn = new_hdr.tn;
						mark_blk_free = FALSE;
						inctn_opcode = upgrade ? inctn_blkupgrd : inctn_blkdwngrd;
					} else
					{
						mark_blk_free = TRUE;
						inctn_opcode = inctn_blkmarkfree;
					}
					inctn_detail.blknum_struct.blknum = curblk;
					/* t_end assumes that the history it is passed does not contain a bitmap block.
					 * for bitmap block, the history validation information is passed through cse instead.
					 * therefore we need to handle bitmap and non-bitmap cases separately.
					 */
					if (!lcnt)
					{	/* Means a bitmap block.
						 * At this point we can do a "new_db_format != ondsk_blkver" check to determine
						 * if the block got converted since we did the dsk_read (see the non-bitmap case
						 * for a similar check done there), but in that case we will have a transaction
						 * which has read 1 bitmap block and is updating no block. "t_end" currently cannot
						 * handle this case as it expects any bitmap block that needs validation to also
						 * have a corresponding cse which will hold its history. Hence we avoid doing the
						 * new_db_format check. The only disadvantage of this is that we will end up
						 * modifying the bitmap block as part of this transaction (in an attempt to convert
						 * its ondsk_blkver) even though it is already in the right format. Since this
						 * overhead is going to be one per bitmap block and since the block is in the cache
						 * at this point, we should not lose much.
						 */
						assert(!mark_blk_free);
						BLK_ADDR(blkid_ptr, SIZEOF(block_id), block_id);
						*blkid_ptr = 0;
						t_write_map(blkhist, (unsigned char *)blkid_ptr, curr_tn, 0);
						assert(&alt_hist.h[0] == blkhist);
						alt_hist.h[0].blk_num = 0; /* create empty history for bitmap block */
						assert(update_trans);
					} else
					{	/* non-bitmap block. fill in history for validation in t_end */
						assert(curblk);	/* we should never come here for block 0 (bitmap) */
						if (!mark_blk_free)
						{
							assert(blkhist->blk_num == curblk);
							assert(blkhist->buffaddr == blkBase);
							blkhist->tn      = curr_tn;
							alt_hist.h[1].blk_num = 0;
						}
						/* Also need to pass the bitmap as history to detect if any concurrent M-kill
						 * is freeing up the same USED block that we are trying to convert OR if any
						 * concurrent M-set is reusing the same RECYCLED block that we are trying to
						 * convert. Because of t_end currently not being able to validate a bitmap
						 * without that simultaneously having a cse, we need to create a cse for the
						 * bitmap that is used only for bitmap history validation, but should not be
						 * used to update the contents of the bitmap block in bg_update.
						 */
						bmlhist.buffaddr = t_qread(curbmp, (sm_int_ptr_t)&bmlhist.cycle, &bmlhist.cr);
						if (NULL == bmlhist.buffaddr)
						{
							t_retry((enum cdb_sc)rdfail_detail);
							continue;
						}
						bmlhist.blk_num = curbmp;
						bmlhist.tn = curr_tn;
						GET_BM_STATUS(bmlhist.buffaddr, lcnt, bml_status);
						if (BLK_MAPINVALID == bml_status)
						{
							t_retry(cdb_sc_lostbmlcr);
							continue;
						}
						if (!mark_blk_free)
						{
							if ((new_db_format != ondsk_blkver) && (BLK_FREE != bml_status))
							{	/* block still needs to be converted. create cse */
								BLK_INIT(bs_ptr, bs1);
								BLK_SEG(bs_ptr, blkBase + SIZEOF(new_hdr),
									new_hdr.bsiz - SIZEOF(new_hdr));
								BLK_FINI(bs_ptr, bs1);
								t_write(blkhist, (unsigned char *)bs1, 0, 0,
									((blk_hdr_ptr_t)blkBase)->levl, FALSE,
									FALSE, GDS_WRITE_PLAIN);
								/* The tree_status for now is only used to determin whether writing
								 * the block to snapshot file.  * (see t_end_ops.c).
 								 * For reorg upgrade/downgrade process, the block is updated in a
								 * sequential way without changing the gv_target. In this case,
								 * we assume the block is in directory tree so as to have
								 * it written to the snapshot file
			 					 */
								BIT_SET_DIR_TREE(cw_set[cw_set_depth-1].blk_prior_state);
								/* reset update_trans in case previous retry had set it to 0 */
								update_trans = UPDTRNS_DB_UPDATED_MASK;
								if (BLK_RECYCLED == bml_status)
								{	/* If block that we are upgarding is RECYCLED, indicate to
									 * bg_update that blks_to_upgrd counter should NOT be
									 * touched in this case by setting "mode" to a special value
									 */
									assert(cw_set[cw_set_depth-1].mode == gds_t_write);
									cw_set[cw_set_depth-1].mode = gds_t_write_recycled;
									/* we SET block as NOT RECYCLED, otherwise, the mm_update()
									 * or bg_update_phase2 may skip writing it to snapshot file
									 * when its level is 0
									 */
									BIT_CLEAR_RECYCLED(cw_set[cw_set_depth-1].blk_prior_state);
								}
							} else
							{	/* Block got converted by another process since we did the dsk_read.
								 * 	or this block became marked free in the bitmap.
								 * No need to update this block. just call t_end for validation of
								 * 	both the non-bitmap block as well as the bitmap block.
								 * Note down that this transaction is no longer updating any blocks.
								 */
								update_trans = 0;
							}
							/* Need to put bit maps on the end of the cw set for concurrency checking.
							 * We want to simulate t_write_map, except we want to update "cw_map_depth"
							 * instead of "cw_set_depth". Hence the save and restore logic below.
							 * This part of the code is similar to the one in mu_swap_blk.c
							 */
							save_cw_set_depth = cw_set_depth;
							assert(!cw_map_depth);
							t_write_map(&bmlhist, NULL, curr_tn, 0); /* will increment cw_set_depth */
							cw_map_depth = cw_set_depth; /* set cw_map_depth to latest cw_set_depth */
							cw_set_depth = save_cw_set_depth;/* restore cw_set_depth */
							/* t_write_map simulation end */
						} else
						{
							if (BLK_RECYCLED != bml_status)
							{	/* Block was RECYCLED at beginning but no longer so. Retry */
								t_retry(cdb_sc_bmlmod);
								continue;
							}
							/* Mark recycled block as FREE in bitmap */
							assert(lcnt == (curblk - curbmp));
							assert(update_array_ptr == update_array);
							*((block_id *)update_array_ptr) = lcnt;
							update_array_ptr += SIZEOF(block_id);
							/* the following assumes SIZEOF(block_id) == SIZEOF(int) */
							assert(SIZEOF(block_id) == SIZEOF(int));
							*(int *)update_array_ptr = 0;
							t_write_map(&bmlhist, (unsigned char *)update_array, curr_tn, 0);
							update_trans = UPDTRNS_DB_UPDATED_MASK;
						}
					}
					assert(SIZEOF(lcl_update_trans) == SIZEOF(update_trans));
					lcl_update_trans = update_trans;	/* take a copy before t_end modifies it */
					if ((trans_num)0 != t_end(&alt_hist, NULL, TN_NOT_SPECIFIED))
					{	/* In case this is MM and t_end() remapped an extended database, reset csd */
						assert(csd == cs_data);
						if (!lcl_update_trans)
						{
							assert(lcnt);
							assert(!mark_blk_free);
							assert((new_db_format == ondsk_blkver) || (BLK_BUSY != bml_status));
							if (BLK_BUSY != bml_status)
								reorg_stats.blks_skipped_free++;
							else
								reorg_stats.blks_skipped_newfmtincache++;
						} else if (!lcnt)
							reorg_stats.blks_converted_bmp++;
						else
							reorg_stats.blks_converted_nonbmp++;
						break;
					}
					assert(csd == cs_data);
				}
			}
		}
	stop_reorg_on_this_reg:
		/* even though ctrl-c occurred, update file-header fields to store reorg's progress before exiting */
		grab_crit(reg);
		blocks_left = 0;
		assert(csd->trans_hist.total_blks >= csd->blks_to_upgrd);
		actual_blks2upgrd = csd->blks_to_upgrd;
		total_blks = csd->trans_hist.total_blks;
		free_blks = csd->trans_hist.free_blocks;
		/* Care should be taken not to set "csd->reorg_upgrd_dwngrd_restart_block" in case of a concurrent db fmt
		 * change. This is because let us say we are doing REORG UPGRADE. A concurrent REORG DOWNGRADE would
		 * have reset "csd->reorg_upgrd_dwngrd_restart_block" field to 0 and if that reorg is interrupted by a
		 * Ctrl-C (before this reorg came here) it would have updated "csd->reorg_upgrd_dwngrd_restart_block" to
		 * a non-zero value indicating how many blocks from 0 have been downgraded. We should not reset this
		 * field to "curblk" as it will be mis-interpreted as the number of blocks that have been DOWNgraded.
		 */
		set_fully_upgraded = FALSE;
		if (mu_reorg_upgrd_dwngrd_start_tn != csd->desired_db_format_tn)
		{	/* csd->desired_db_format changed since reorg started. discontinue the reorg */
			util_out_print("Region !AD : Desired DB Format changed during REORG. Stopping REORG.",
				TRUE, REG_LEN_STR(reg));
			status1 = ERR_MUNOFINISH;
		} else if (reorg_entiredb)
		{	/* Change "csd->reorg_upgrd_dwngrd_restart_block" only if STARTBLK or STOPBLK was NOT specified */
			assert(csd->reorg_upgrd_dwngrd_restart_block <= curblk);
			csd->reorg_upgrd_dwngrd_restart_block = curblk;	/* blocks lesser than this have been upgraded/downgraded */
			expected_blks2upgrd = upgrade ? 0 : (total_blks - free_blks);
			blocks_left = upgrade ? actual_blks2upgrd : (expected_blks2upgrd - actual_blks2upgrd);
			/* If this reorg command went through all blocks in the database, then it should have
			 * 	correctly concluded at this point whether the reorg is complete or not.
			 * If this reorg command started from where a previous incomplete reorg left
			 *	(i.e. first_reorg_in_this_db_fmt is FALSE), it cannot determine if the initial
			 *	GDS blocks that it skipped are completely {up,down}graded or not.
			 */
			assert((0 == blocks_left) || (SS_NORMAL != status1) || !first_reorg_in_this_db_fmt);
			/* If this is a MUPIP REORG UPGRADE that did go through every block in the database (indicated by
			 * "reorg_entiredb" && "first_reorg_in_this_db_fmt") and the current count of "blks_to_upgrd" is
			 * 0 in the file-header and the desired_db_format did not change since the start of the REORG,
			 * we can be sure that the entire database has been upgraded. Set "csd->fully_upgraded" to TRUE.
			 */
			if ((SS_NORMAL == status1) && first_reorg_in_this_db_fmt && upgrade && (0 == actual_blks2upgrd))
			{
				csd->fully_upgraded = TRUE;
				csd->db_got_to_v5_once = TRUE;
				set_fully_upgraded = TRUE;
			}
			/* flush all changes noted down in the file-header */
			if (!wcs_flu(WCSFLU_FLUSH_HDR))	/* wcs_flu assumes gv_cur_region is set (which it is in this routine) */
			{
				gtm_putmsg(VARLSTCNT(6) ERR_BUFFLUFAILED, 4,
					LEN_AND_LIT("MUPIP REORG UPGRADE/DOWNGRADE"), DB_LEN_STR(reg));
				status = ERR_MUNOFINISH;
				rel_crit(reg);
				continue;
			}
		}
		curr_tn = csd->trans_hist.curr_tn;
		rel_crit(reg);
		util_out_print("Region !AD : Stopped processing at block number [0x!XL]", TRUE, REG_LEN_STR(reg), curblk);
		/* Print statistics */
		util_out_print("Region !AD : Statistics : Blocks Read From Disk (Bitmap)     : 0x!XL",
			TRUE, REG_LEN_STR(reg), reorg_stats.blks_read_from_disk_bmp);
		util_out_print("Region !AD : Statistics : Blocks Skipped (Free)              : 0x!XL",
			TRUE, REG_LEN_STR(reg), reorg_stats.blks_skipped_free);
		util_out_print("Region !AD : Statistics : Blocks Read From Disk (Non-Bitmap) : 0x!XL",
			TRUE, REG_LEN_STR(reg), reorg_stats.blks_read_from_disk_nonbmp);
		util_out_print("Region !AD : Statistics : Blocks Skipped (new fmt in disk)   : 0x!XL",
			TRUE, REG_LEN_STR(reg), reorg_stats.blks_skipped_newfmtindisk);
		util_out_print("Region !AD : Statistics : Blocks Skipped (new fmt in cache)  : 0x!XL",
			TRUE, REG_LEN_STR(reg), reorg_stats.blks_skipped_newfmtincache);
		util_out_print("Region !AD : Statistics : Blocks Converted (Bitmap)          : 0x!XL",
			TRUE, REG_LEN_STR(reg), reorg_stats.blks_converted_bmp);
		util_out_print("Region !AD : Statistics : Blocks Converted (Non-Bitmap)      : 0x!XL",
			TRUE, REG_LEN_STR(reg), reorg_stats.blks_converted_nonbmp);
		if (reorg_entiredb && (SS_NORMAL == status1) && (0 != blocks_left))
		{	/* file-header counter does not match what reorg on the entire database expected to see */
			gtm_putmsg(VARLSTCNT(4) ERR_DBBTUWRNG, 2, expected_blks2upgrd, actual_blks2upgrd);
			util_out_print("Region !AD : Run MUPIP INTEG (without FAST qualifier) to fix the counter",
				TRUE, REG_LEN_STR(reg));
			status1 = ERR_MUNOFINISH;
		} else
			util_out_print("Region !AD : Total Blocks = [0x!XL] : Free Blocks = [0x!XL] : "
				       "Blocks to upgrade = [0x!XL]",
				       TRUE, REG_LEN_STR(reg), total_blks, free_blks, actual_blks2upgrd);
		/* Issue success or failure message for this region */
		if (SS_NORMAL == status1)
		{	/* issue success only if REORG did not encounter any error in its processing */
			if (set_fully_upgraded)
				util_out_print("Region !AD : Database is now FULLY UPGRADED", TRUE, REG_LEN_STR(reg));
			util_out_print("Region !AD : MUPIP REORG !AD finished!/", TRUE, REG_LEN_STR(reg), LEN_AND_STR(command));
			send_msg(VARLSTCNT(7) ERR_MUREUPDWNGRDEND, 5, REG_LEN_STR(reg), process_id, process_id, &curr_tn);
		} else
		{
			assert(ERR_MUNOFINISH == status1);
			assert((SS_NORMAL == status) || (ERR_MUNOFINISH == status));
			util_out_print("Region !AD : MUPIP REORG !AD incomplete. See above messages.!/",
					TRUE, REG_LEN_STR(reg), LEN_AND_STR(command));
			status = status1;
		}
	}
	if (NULL != bptr)
		free(bptr);
	if (NULL != bml_lcl_buff)
		free(bml_lcl_buff);
	if (mu_ctrly_occurred || mu_ctrlc_occurred)
	{
		gtm_putmsg(VARLSTCNT(1) ERR_REORGCTRLY);
		status = ERR_MUNOFINISH;
	}
	mupip_exit(status);
}
