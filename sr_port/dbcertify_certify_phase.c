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

/****************************************************************
        dbcertify_certify_phase2.c - Database certification phase 2

        - Verify phase 1 input file.
        - Locate and open database after getting standalong access.
        - Read the identified blocks in and if they are still too
          large, split them.
        - Certify the database as "clean" if no errors encountered.

        Note: Most routines in this utility are self-contained
              meaning they do not reference GT.M library routines
              (with some notable exceptions). This is because
              phase-2 is going to run against V4 format databases
              but any linked routines would be compiled for V5
              databases.
****************************************************************/

#include "mdef.h"

#ifdef VMS
#include <descrip.h>
#include <rms.h>
#include <ssdef.h>
#endif

#include <errno.h>
#include "gtm_stat.h"
#include "gtm_ctype.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_unistd.h"
#include "gtm_stdlib.h"
#include "gtm_fcntl.h"

#ifdef __MVS__
#include "gtm_zos_io.h"
#endif
#include "gtmio.h"
#include "cli.h"
#include "copy.h"
#include "iosp.h"
#include "gdsroot.h"
#include "v15_gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "v15_gdsbt.h"
#include "gdsfhead.h"
#include "v15_gdsfhead.h"
#include "filestruct.h"
#include "v15_filestruct.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "gdscc.h"
#include "bmm_find_free.h"
#include "gdsblkops.h"
#include "bit_set.h"
#include "bit_clear.h"
#include "min_max.h"
#include "gtmmsg.h"
#ifdef VMS
#  include "is_file_identical.h"
#endif
#include "error.h"
#include "mupip_exit.h"
#include "util.h"
#include "dbcertify.h"

GBLREF	char_ptr_t		update_array, update_array_ptr;
GBLREF	uint4			update_array_size;
GBLREF	VSIG_ATOMIC_T		forced_exit;			/* Signal came in while we were in critical section */
GBLREF	int4			exi_condition;
GBLREF	phase_static_area	*psa_gbl;

boolean_t dbc_split_blk(phase_static_area *psa, block_id blk_num, enum gdsblk_type blk_type, v15_trans_num tn, int blk_levl);
void dbc_flush_fhead(phase_static_area *psa);
void dbc_read_p1out(phase_static_area *psa, void *obuf, int olen);

error_def(ERR_DEVOPENFAIL);
error_def(ERR_FILENOTFND);
error_def(ERR_DBCSCNNOTCMPLT);
error_def(ERR_DBCBADFILE);
error_def(ERR_DBCMODBLK2BIG);
error_def(ERR_DBCINTEGERR);
error_def(ERR_DBCNOEXTND);
error_def(ERR_DBCDBCERTIFIED);
error_def(ERR_DBCNOTSAMEDB);
error_def(ERR_DBCDBNOCERTIFY);
error_def(ERR_DBCREC2BIGINBLK);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);
error_def(ERR_BITMAPSBAD);
error_def(ERR_MUPCLIERR);
ZOS_ONLY(error_def(ERR_BADTAG);)

/* The final certify phase of certification process */
void dbcertify_certify_phase(void)
{
	int		save_errno, len, rc, restart_cnt, maxkeystructsize;
	uint4		rec_num;
	char_ptr_t	errmsg;
	boolean_t	restart_transaction, p1rec_read;
	unsigned short	buff_len;
	int		tmp_cmpc;
	char		ans[2];
	unsigned char	dbfn[MAX_FN_LEN + 1];
	file_control	*fc;
	phase_static_area *psa;
	ZOS_ONLY(int	realfiletag;)

	psa = psa_gbl;
	DBC_DEBUG(("DBC_DEBUG: Beginning certification phase\n"));
	psa->phase_one = FALSE;
	UNIX_ONLY(atexit(dbc_certify_phase_cleanup));
	psa->block_depth = psa->block_depth_hwm = -1;		/* Initialize no cache */

	/* Check parsing results */
	if (CLI_PRESENT == cli_present("BLOCKS"))
	{
		if (!cli_get_hex("BLOCKS", &psa->blocks_to_process))
			exit(EXIT_FAILURE);		/* Error message already raised */
	} else
		psa->blocks_to_process = MAXTOTALBLKS_V4;
	if (CLI_PRESENT == cli_present("TEMPFILE_DIR"))
	{	/* Want to put temp files in this directory */
		buff_len = SIZEOF(psa->tmpfiledir) - 1;
		if (0 == cli_get_str("TEMPFILE_DIR", (char_ptr_t)psa->tmpfiledir, &buff_len))
			mupip_exit(ERR_MUPCLIERR);
	}
	psa->keep_temp_files = (CLI_PRESENT == cli_present("KEEP_TEMPS"));
	buff_len = SIZEOF(psa->outfn) - 1;
	if (0 == cli_get_str("P1OUTFILE", (char_ptr_t)psa->outfn, &buff_len))
		mupip_exit(ERR_MUPCLIERR);

	/* Open phase-1 output file (our input file) */
	psa->outfd = OPEN((char_ptr_t)psa->outfn, O_RDONLY RMS_OPEN_BIN);
	if (FD_INVALID == psa->outfd)
	{
		save_errno = errno;
		if (save_errno == ENOENT)
			rts_error(VARLSTCNT(4) ERR_FILENOTFND, 2, RTS_ERROR_STRING((char_ptr_t)psa->outfn));
		else
		{
			errmsg = STRERROR(save_errno);
			rts_error(VARLSTCNT(8) ERR_DEVOPENFAIL, 2, RTS_ERROR_STRING((char_ptr_t)psa->outfn),
				  ERR_TEXT, 2, RTS_ERROR_STRING(errmsg));
		}
	}
#ifdef __MVS__
	if (-1 == gtm_zos_tag_to_policy(psa->outfd, TAG_BINARY, &realfiletag))
		TAG_POLICY_GTM_PUTMSG((char_ptr_t)psa->outfn, errno, realfiletag, TAG_BINARY);
#endif
	dbc_read_p1out(psa, &psa->ofhdr, SIZEOF(p1hdr));		/* Read phase 1 output file header */
	if (0 != memcmp(psa->ofhdr.p1hdr_tag, P1HDR_TAG, SIZEOF(psa->ofhdr.p1hdr_tag)))
		rts_error(VARLSTCNT(4) ERR_DBCBADFILE, 2, RTS_ERROR_STRING((char_ptr_t)psa->outfn));
	if (0 == psa->ofhdr.tot_blocks)
		/* Sanity check that the output file was finished and completed */
		rts_error(VARLSTCNT(4) ERR_DBCSCNNOTCMPLT, 2, RTS_ERROR_STRING((char_ptr_t)psa->outfn));
	assert(0 != psa->ofhdr.tn);

	/* Check if region name still associates to the same file */
	dbc_find_database_filename(psa, psa->ofhdr.regname, dbfn);

	/* Notify user this is a critical change and give them the opportunity to abort */
	util_out_print("--------------------------------------------------------------------------------", FLUSH);
	util_out_print("You must have a backup of database !AD before you proceed!!", FLUSH,
		       RTS_ERROR_STRING((char_ptr_t)psa->ofhdr.dbfn));
	util_out_print("An abnormal termination can damage the database while doing the certification !!", FLUSH);
	util_out_print("Proceeding will also turn off replication and/or journaling if enabled", FLUSH);
	util_out_print("--------------------------------------------------------------------------------", FLUSH);
	util_out_print("Proceed? [y/n]:", FLUSH);
	SCANF("%1s", ans);	/* We only need one char, any more would overflow our buffer */
	if ('y' != ans[0] && 'Y' != ans[0])
	{
		util_out_print("Certification phase aborted\n", FLUSH);
		return;
	}
	util_out_print("Certification phase for database !AD beginning", FLUSH, RTS_ERROR_STRING((char_ptr_t)psa->ofhdr.dbfn));

	/* Build database structures */
	MALLOC_INIT(psa->dbc_gv_cur_region, SIZEOF(gd_region));
	MALLOC_INIT(psa->dbc_gv_cur_region->dyn.addr, SIZEOF(gd_segment));
	psa->dbc_gv_cur_region->dyn.addr->acc_meth = dba_bg;
	len = STRLEN((char_ptr_t)psa->ofhdr.dbfn);
	strcpy((char_ptr_t)psa->dbc_gv_cur_region->dyn.addr->fname, (char_ptr_t)dbfn);
	psa->dbc_gv_cur_region->dyn.addr->fname_len = len;

	FILE_CNTL_INIT(psa->dbc_gv_cur_region->dyn.addr);
	psa->dbc_gv_cur_region->dyn.addr->file_cntl->file_type = dba_bg;

	psa->dbc_cs_data = malloc(SIZEOF(*psa->dbc_cs_data));
	fc = psa->fc = psa->dbc_gv_cur_region->dyn.addr->file_cntl;
	fc->file_type = psa->dbc_gv_cur_region->dyn.addr->acc_meth = dba_bg;	/* Always treat as BG mode */

	/* Initialize for db processing - open and read in file-header, get "real" filename for comparison */
	dbc_init_db(psa);
	if (0 != strcmp((char_ptr_t)psa->dbc_gv_cur_region->dyn.addr->fname, (char_ptr_t)psa->ofhdr.dbfn))
		/* File name change means db was moved or at least is not as it was when it was scanned */
		rts_error(VARLSTCNT(1) ERR_DBCNOTSAMEDB);
	if (psa->ofhdr.tn > psa->dbc_cs_data->trans_hist.curr_tn)
		rts_error(VARLSTCNT(1) ERR_DBCNOTSAMEDB);
	psa->max_blk_len = psa->dbc_cs_data->blk_size - psa->dbc_cs_data->reserved_bytes;

	/* Initialize maximum key we may need later if we encounter gvtroot blocks */
	maxkeystructsize = SIZEOF(dbc_gv_key) + MAX_DBC_KEY_SZ - 1;
	MALLOC_INIT(psa->max_key, maxkeystructsize);
	psa->max_key->top = maxkeystructsize;
	psa->max_key->gvn_len = 1;
	*psa->max_key->base = (unsigned char)0xFF;
	/* Key format: 0xFF, 0x00, 0x00 : This is higher than any valid key would be */
	psa->max_key->end = MAX_DBC_KEY_SZ - 1;

	/* Allocate update array based on fileheader values */
	psa->dbc_cs_data->max_update_array_size = psa->dbc_cs_data->max_non_bm_update_array_size =
		(uint4)ROUND_UP2(MAX_NON_BITMAP_UPDATE_ARRAY_SIZE(psa->dbc_cs_data), UPDATE_ARRAY_ALIGN_SIZE);
	psa->dbc_cs_data->max_update_array_size += (int4)(ROUND_UP2(MAX_BITMAP_UPDATE_ARRAY_SIZE, UPDATE_ARRAY_ALIGN_SIZE));
	update_array = malloc(psa->dbc_cs_data->max_update_array_size);
	update_array_size = psa->dbc_cs_data->max_update_array_size;

	/* Now to the real work -- Read and split each block the phase 1 file recorded that still needs
	   to be split (concurrent updates may have "fixed" some blocks).
	*/
	psa->hint_blk = psa->hint_lcl = 1;
	restart_transaction = p1rec_read = FALSE;
	restart_cnt = 0;
	for (rec_num = 0; rec_num < psa->ofhdr.blk_count || 0 < psa->gvtroot_rchildren_cnt;)
	{	/* There is the possibility that we are restarting the processing of a given record. In
		   that case we will not read the next record in but process what is already in the buffer.
		   This can occur if we have extended the database. */
		if (!restart_transaction)
		{	/* First to check is if we have any queued gvtroot_rchildren to process (described elsewhere). If we have
			   these, we process them now without bumping the record count.
			*/
			p1rec_read = FALSE;	/* Assume we did NOT read from the file */
			if (0 < psa->gvtroot_rchildren_cnt)
			{
				psa->gvtroot_rchildren_cnt--;
				memcpy((char *)&psa->rhdr, (char *)&psa->gvtroot_rchildren[psa->gvtroot_rchildren_cnt],
				       SIZEOF(p1rec));
				psa->gvtrchildren++;	/* counter */
				DBC_DEBUG(("DBC_DEBUG: Pulling p1rec from queued gvtroot_rchildren array (%d)\n",
					   psa->gvtroot_rchildren_cnt));
			} else
			{	/* Normal processing - read record from phase one file */
				if (rec_num == psa->blocks_to_process)
				{	/* Maximum records processed */
					DBC_DEBUG(("DBC_DEBUG: Maximum records to process limit reached "
						   "- premature exit to main loop\n"));
					break;
				}
				DBC_DEBUG(("DBC_DEBUG: ****************** Reading new p1out record (%d) *****************\n",
					   (rec_num + 1)));
				dbc_read_p1out(psa, &psa->rhdr, SIZEOF(p1rec));
				if (0 != psa->rhdr.akey_len)
				{	/* This module does not need the ascii key so just bypass it if it exists */
					if (0 != psa->rhdr.blk_levl || SIZEOF(psa->rslt_buff) < psa->rhdr.akey_len )
						GTMASSERT;		/* Must be corrupted file? */
					dbc_read_p1out(psa, (char_ptr_t)psa->rslt_buff, psa->rhdr.akey_len);
				}
				p1rec_read = TRUE;	/* Note, not reset by restarted transaction */
			}
			/* Don't want to reset the high water mark on a restarted transaction */
			if (psa->block_depth > psa->block_depth_hwm)
				psa->block_depth_hwm = psa->block_depth;	/* Keep track of maximum indexes we have used */
			restart_cnt = 0;
		} else
		{
			++restart_cnt;
			if (MAX_RESTART_CNT < restart_cnt)
				GTMASSERT;			/* No idea what could cause this.. */
			DBC_DEBUG(("DBC_DEBUG: ****************** Restarted transaction (%d) *****************\n",
				   (rec_num + 1)));
			/* "restart_transaction" is either set or cleared by dbc_split_blk() below */
		}
		assert((int)psa->rhdr.blk_type);
		/* Note assignment in "if" below */
		if (restart_transaction = dbc_split_blk(psa, psa->rhdr.blk_num, psa->rhdr.blk_type,
							psa->rhdr.tn, psa->rhdr.blk_levl))
			psa->block_depth_hwm = -1;	/* Zaps cache so all blocks are re-read */
		else if (p1rec_read)			/* If rec processed was from scan phase, bump record counter */
			rec_num++;
	} /* for each record in phase-1 output file or each restart or each queued rh child */

	/* Reaching this point, the database has been updated, with no errors. We can now certify
	   this database as ready for the current version of GT.M
	*/
	util_out_print("", FLUSH);	/* New line for below message in case MUPIP extension leaves prompt */
	if (0 == psa->blk_process_errors)
	{
		if (psa->blocks_to_process != rec_num)
		{
			((sgmnt_data_ptr_t)psa->dbc_cs_data)->certified_for_upgrade_to = GDSV6;
			psa->dbc_fhdr_dirty = TRUE;
			gtm_putmsg(VARLSTCNT(6) ERR_DBCDBCERTIFIED, 4, RTS_ERROR_STRING((char_ptr_t)psa->ofhdr.dbfn),
				   RTS_ERROR_LITERAL("GT.M V5"));
		} else
		{
			DBC_DEBUG(("DBC_DEBUG: Database certification bypassed due to records to process limit being reached\n"));
		}
	} else
		gtm_putmsg(VARLSTCNT(4) ERR_DBCDBNOCERTIFY, 2, RTS_ERROR_STRING((char_ptr_t)psa->ofhdr.dbfn));

	dbc_flush_fhead(psa);
	dbc_close_db(psa);
	CLOSEFILE_RESET(psa->outfd, rc);	/* resets "psa->outfd" to FD_INVALID */

	PRINTF("\n");
	PRINTF("Total blocks in scan phase file --   %12d [0x%08x]\n", psa->ofhdr.blk_count, psa->ofhdr.blk_count);
	PRINTF("Blocks bypassed ------------------   %12d [0x%08x]\n", psa->blks_bypassed, psa->blks_bypassed);
	PRINTF("Blocks processed -----------------   %12d [0x%08x]\n", psa->blks_processed, psa->blks_processed);
	PRINTF("Blocks read ----------------------   %12d [0x%08x]\n", psa->blks_read, psa->blks_read);
	PRINTF("Blocks read from cache -----------   %12d [0x%08x]\n", psa->blks_cached, psa->blks_cached);
	PRINTF("Blocks updated -------------------   %12d [0x%08x]\n", psa->blks_updated, psa->blks_updated);
	PRINTF("Blocks created -------------------   %12d [0x%08x]\n", psa->blks_created, psa->blks_created);
	PRINTF("GVTROOT right children processed -   %12d [0x%08x]\n", psa->gvtrchildren, psa->gvtrchildren);

	/* Release resources */
	free(update_array);
	free(psa->dbc_cs_data);
#ifdef VMS
	/* Some extra freeing of control blocks on VMS */
	if (NULL != FILE_INFO(psa->dbc_gv_cur_region)->fab)
		free(FILE_INFO(psa->dbc_gv_cur_region)->fab);
	if (NULL != FILE_INFO(psa->dbc_gv_cur_region)->nam)
	{
		if (NULL != FILE_INFO(psa->dbc_gv_cur_region)->nam->nam$l_esa)
			free(FILE_INFO(psa->dbc_gv_cur_region)->nam->nam$l_esa);
		free(FILE_INFO(psa->dbc_gv_cur_region)->nam);
	}
	if (NULL != FILE_INFO(psa->dbc_gv_cur_region)->xabfhc)
		free(FILE_INFO(psa->dbc_gv_cur_region)->xabfhc);
	if (NULL != FILE_INFO(psa->dbc_gv_cur_region)->xabpro)
		free(FILE_INFO(psa->dbc_gv_cur_region)->xabpro);
#endif
	free(psa->dbc_gv_cur_region->dyn.addr->file_cntl->file_info);
	free(psa->dbc_gv_cur_region->dyn.addr->file_cntl);
	free(psa->dbc_gv_cur_region->dyn.addr);
	free(psa->dbc_gv_cur_region);
	psa->dbc_gv_cur_region = NULL;
	if (psa->first_rec_key)
		free(psa->first_rec_key);
	free(psa);
}

/* Routine to handle the processing (splitting) of a given database block. If the current block process needs
   to be restarted, this function returns TRUE. else if processing completed normally, returns FALSE.

   Routine notes:

   This routine implements a "simplistic" mini database engine. It is "simplistic" in the regards to fact that it
   doesn't need to worry about concurrency issues. It also has one design assumption that we will NEVER add a
   record to a level 0 block (either DT or GVT). Because of this assumption, many complications from gvcst_put(), on
   which it is largely based, were non-issues and were removed (e.g. no TP). This routine has its own concepts of
   "cache", cw_set elements, update arrays, etc. Following is a brief description of how these things are implemented
   in this routine:

   The primary control block in this scheme is the block_info block which serves as a cache record, change array anchor,
   gv_target, and so on. In short, everything that is known about a given database block is contained in this one
   structure. There is an array of these structures with the name "blk_set" which is a global variable array dimensioned
   at a thoroughly outrageous amount for the worst case scenario.

   There are areas within the blk_set array that are worth describing:

   - The block_depth global variable always holds the top in use index into blk_set.
   - blk_set[0] describes the block that was fed to us from the phase 1 scan. It is the primary block that needs to
     be split. If nothing needs to happen to it, we go to the next record and blk_set[0] get a new block in it.
   - Starting with blk_set[1] through blk_set[bottom_tree_index] are first the directory tree (DT) blocks and then
     (if primary was a GVT block) the global variable tree (GVT) blocks.
   - Starting with blk_set[bottom_tree_index + 1] through blk_set[bottom_created_index] are newly created blocks during
     split processing.
   - Starting with blk_set[bottom_created_index + 1] through blk_set[block_depth] are local bit map blocks that are being
     modified for the "transaction".

   This engine has a very simple cache mechanism. If a block we need is somewhere in the blk_set array (a global variable
   block_depth_hwm maintains a high water mark), the cache version is used rather than forcing a re-read from disk. It is
   fairly simple but seems to save a lot of reads, especially of the directory tree and the local bit_maps.

   Like gvcst_put(), once we have the blocks from the tree loaded, they are processed in reverse order as a split in one
   block requires a record to be inserted into the parent block. We start with the primary block (blk_set[0]) and then
   move to blk_set[bottom_tree_index] and work backwards from there until either we get to a block for which there are
   no updates or we hit a root block (GVT or DT depending) at which time we are done with the primary update loop.

   After performing the block splits and creating new blocks, we double check that we have room to hold them all. If not,
   we make a call to MUPIP EXTEND to extend the database for us. Since this means we have to close the file and give up
   our locks on it, we also restart the transaction and force all blocks to be re-read from disk.

   Once assured we have sufficient free blocks, we start at blk_set[bottom_created_index] and work down to
   blk_set[bottom_tree_index + 1] allocating and assigning block numbers to the created blocks. Part of this process also
   puts the block numbers into places where the update arrays will pick them up when the referencing blocks are built.

   Once all the new blocks have been assigned, we loop through blk_set[bottom_tree_index] to blk_set[0] and create the
   new versions of the blocks (for those blocks marked as being updated). A note here is that this engine does not build
   update array entries for bitmap blocks, preferring instead to just update the local bitmap block buffers directly.

   The last major loop is to write to disk all the new and changed blocks to disk. There is no processing but simple IO
   in this loop to minimize the potential of something going wrong. There is no recovery at this point. If this loop fails
   in mid-stream, the database is toast.

*/
boolean_t dbc_split_blk(phase_static_area *psa, block_id blk_num, enum gdsblk_type blk_type, v15_trans_num tn, int blk_levl)
{
	int		blk_len, blk_size, restart_cnt, save_block_depth;
	int		gvtblk_index, dtblk_index, blk_index, bottom_tree_index, bottom_created_index;
	int		curr_blk_len, curr_blk_levl, curr_rec_len, ins_key_len, ins_rec_len;
	int		curr_rec_shrink, curr_rec_offset, blks_this_lmap;
	int		prev_rec_offset, new_blk_len, new_rec_len, remain_offset, remain_len, blk_seg_cnt;
	int		new_lh_blk_len, new_rh_blk_len, created_blocks, extent_size;
	int		local_map_max, lbm_blk_index, lcl_blk, curr_rec_cmpc, cmpc;
	int		tmp_cmpc;
	int4		lclmap_not_full;
	uint4		total_blks;
	boolean_t	dummy_bool;
	boolean_t	got_root, level_0, completed, insert_point, restart_transaction;
	blk_segment	*bs_ptr, *bs1, *blk_sega_p, *blk_array_top;
	rec_hdr_ptr_t	ins_rec_hdr, next_rec_hdr, new_star_hdr;
	dbc_gv_key	*last_rec_key;
	uchar_ptr_t	rec_p, next_rec_p, mid_point, cp1, lcl_map_p, new_blk_p, blk_p, blk_endp, chr_p;
	unsigned short	us_rec_len;
	v15_trans_num	curr_tn;
	block_id	blk_ptr;
	block_id	bitmap_blk_num, *lhs_block_id_p, *rhs_block_id_p, allocated_blk_num;
	block_info	*blk_set_p, *blk_set_new_p, *blk_set_prnt_p, *blk_set_bm_p, *blk_set_rhs_p;
	block_info	restart_blk_set;

	DEBUG_ONLY(
		boolean_t	first_time = FALSE;
	)

	/* First order of business is to read the required block in */
	psa->block_depth = -1;
	blk_size = psa->dbc_cs_data->blk_size;	/* BLK_FINI macro needs a local copy */
	dbc_read_dbblk(psa, blk_num, blk_type);

	/* Now that we have read the block in, let us see if it is still a "problem" block. If its
	   TN has changed, that is an indicator that is should NOT be a problem block any longer
	   with the sole exception of a TN RESET having been done on the DB since phase 1. In that
	   case, we will still insist on a phase 1 rerun as some of our sanity checks have disappeared.
	*/
	assert(0 == psa->block_depth);
	blk_p = psa->blk_set[0].old_buff;
	assert(blk_p);
	blk_len = psa->blk_set[0].blk_len;

	/* If the block is still too large, sanity check on TN at phase 1 and now. Note that it is
	   possible in an index block for the TN to have changed yet the block is otherwise unmodified
	   if (1) this is an index block and (2) a record is being inserted before the first record in
	   the block. In this case, the new record is put into the new (LH) sibling and the entire existing
	   block is put unmodified into the RH side in the existing block. The net result is that only
	   the TN changes in this block and if the block is too full it is not split. This will never
	   happen for a created block though. It can only hapen for existing index blocks. Note if the
	   block is not (still) too full that we cannot yet say this block has nothing to happen to it
	   because if it is a gvtroot block, we need to record its right side children further down.
	*/
	GET_ULONG(curr_tn, &((v15_blk_hdr_ptr_t)blk_p)->tn);
	if ((UNIX_ONLY(8) VMS_ONLY(9) > blk_size - blk_len) && (curr_tn != tn) && (gdsblk_gvtleaf == blk_type))
	{
		/* Block has been modified: Three possible reasons it is not fixed:
		   1) The user was playing with reserved bytes and set it too low allowing some
		   large blocks to be created we did not know about (but thankfully just caught).
		   2) User ran a recover after running phase 1 that re-introduced some too-large
		   blocks. This is a documented no-no but we have no way to enforce it on V4.
		   3) There was a TN reset done.
		   All three of these causes require a rerun of the scan phase.
		*/
		rts_error(VARLSTCNT(3) ERR_DBCMODBLK2BIG, 1, blk_num);
	}
	/* Isolate the full key in the first record of the block */
	dbc_init_key(psa, &psa->first_rec_key);
	dbc_find_key(psa, psa->first_rec_key, blk_p + SIZEOF(v15_blk_hdr), psa->blk_set[0].blk_levl);
	if ((0 < psa->blk_set[0].blk_levl) && (0 == psa->first_rec_key->end))
	{	/* dbc_find_key found just a star-key in this index block. dbc_find_record/dbc_match_key (invoked later)
		 * does not know to handle this scenario so we finish this case off right away. No need to do any splits
		 * anyways since the block is obviously not too full.
		 */
		DBC_DEBUG(("DBC_DEBUG: Block not processed as it now has sufficient room (index block with only *-key)\n"));
		psa->blks_bypassed++;
		psa->blks_read++;
		if (psa->blk_set[0].found_in_cache)
			psa->blks_cached++;
		return FALSE;	/* No restart needed */
	}
	psa->first_rec_key->gvn_len = USTRLEN((char_ptr_t)psa->first_rec_key->base);	/* The GVN we need to lookup in the DT */
	if (UNIX_ONLY(8) VMS_ONLY(9) <= blk_size - blk_len)
	{	/* This block has room now - no longer need to split it */
		DBC_DEBUG(("DBC_DEBUG: Block not processed as it now has sufficient room\n"));
		psa->blks_bypassed++;
		psa->blks_read++;
		if (psa->blk_set[0].found_in_cache)
			psa->blks_cached++;
		return FALSE;	/* No restart needed */
	}
	/* Possibilities at this point:
	   1) We are looking for a DT (directory tree) block.
	   2) We are looking for a GVT (global variable tree) block.

	   We lookup first_rec_key in the directory tree. If (1) we pass the block level we are searching for
	   as a parameter. If (2), we pass -1 as the block level we are searching for as we need a complete
	   search of the leaf level DT in order to find the GVN.

	   If (1) then the lookup is complete and verification and (later) block splitting can begin. If (2), we need to
	   take the pointer from the found DT record which points to the GVT root block and start our search again
	   from there using the level from the original block as a stopping point. One special case here is if our
	   target block was a gvtroot block, we don't need to traverse the GVT tree to find it. We get it from the
	   directory tree and stop our search there.
	 */
	switch(blk_type)
	{
		case gdsblk_dtindex:
		case gdsblk_dtleaf:
		case gdsblk_dtroot:
			/* Since our search is to end in the dt tree, stop when we get to the requisite level */
			blk_index = dbc_find_dtblk(psa, psa->first_rec_key, blk_levl);
			if (0 > blk_index)
			{	/* Integrity error encountered or record not found. We cannot proceed */
				assert(FALSE);
				rts_error(VARLSTCNT(8) ERR_DBCINTEGERR, 2, RTS_ERROR_STRING((char_ptr_t)psa->ofhdr.dbfn),
					  ERR_TEXT, 2,
					  RTS_ERROR_LITERAL("Unable to find index (DT) record for an existing global"));
			}
			break;
		case gdsblk_gvtindex:
		case gdsblk_gvtleaf:
			/* Search all the way down to lvl 0 to get a dtleaf block */
			dtblk_index = dbc_find_dtblk(psa, psa->first_rec_key, 0);
			if (0 > dtblk_index)
			{	/* Integrity error encountered or record not found. We cannot proceed */
				assert(FALSE);
				rts_error(VARLSTCNT(8) ERR_DBCINTEGERR, 2, RTS_ERROR_STRING((char_ptr_t)psa->ofhdr.dbfn),
					  ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to locate DT leaf (root) block"));
			}
			assert(0 == ((v15_blk_hdr_ptr_t)psa->blk_set[dtblk_index].old_buff)->levl);
			/* Note level 0 directory blocks can have collation data in them but it would be AFTER
			   the block pointer which is the first thing in the record after the key.
			*/
			GET_ULONG(blk_ptr, (psa->blk_set[dtblk_index].curr_rec + SIZEOF(rec_hdr)
					    + psa->blk_set[dtblk_index].curr_blk_key->end + 1
					    - EVAL_CMPC((rec_hdr *)psa->blk_set[dtblk_index].curr_rec)));
			gvtblk_index = dbc_read_dbblk(psa, blk_ptr, gdsblk_gvtroot);
			assert(-1 != gvtblk_index);
			/* If our target block was not the gvtroot block we just read in then we keep scanning for our
			   target record. Otherwise, the scan stops here.
			*/
			if (0 != gvtblk_index)
			{
				blk_index = dbc_find_record(psa, psa->first_rec_key, gvtblk_index, blk_levl, gdsblk_gvtroot, FALSE);
				if (0 > blk_index)
				{
					if (-1 == blk_index)
					{	/* Integrity error encountered. We cannot proceed */
						assert(FALSE);
						rts_error(VARLSTCNT(8) ERR_DBCINTEGERR, 2,
							  RTS_ERROR_STRING((char_ptr_t)psa->ofhdr.dbfn),
							  ERR_TEXT, 2,
							  RTS_ERROR_LITERAL("Unable to find index record for an existing global"));
					} else if (-2 == blk_index)
					{	/* Record was not found. Record has been deleted since we last
						   found it. Elicits a warning message in DEBUG mode but is otherwise ignored.
						*/
						assert(FALSE);
						DBC_DEBUG(("DBC_DEBUG: Block split of blk 0x%x bypassed because its "
							   "key could not be located in the GVT\n", blk_num));
						psa->blks_bypassed++;
						psa->blks_read += psa->block_depth;
						/* Only way to properly update the count of cached records is to run the list
						   and check them.
						*/
						for (blk_index = psa->block_depth, blk_set_p = &psa->blk_set[blk_index];
						     0 <= blk_index;
						     --blk_index, --blk_set_p)
						{	/* Check each block we read */
							if (gdsblk_create != blk_set_p->usage && blk_set_p->found_in_cache)
								psa->blks_cached++;
						}
						return FALSE;	/* No restart necessary */
					} else
						GTMASSERT;
				}
			} else
			{	/* This is a gvtroot block and is the subject of our search */
				blk_index = gvtblk_index;
				assert(gdsblk_gvtroot == psa->blk_set[0].blk_type);
			}
			break;
		default:
			GTMASSERT;
	}
	/* The most recently accessed block (that terminated the search) should be the block
	   we are looking for (which should have been found in the cache as block 0. If not,
	   there is an integrity error and we should not continue.
	*/
	if (0 != blk_index)
	{	/* Integrity error encountered. We cannot proceed */
		assert(FALSE);
		rts_error(VARLSTCNT(8) ERR_DBCINTEGERR, 2, RTS_ERROR_STRING((char_ptr_t)psa->ofhdr.dbfn),
			  ERR_TEXT, 2,
			  RTS_ERROR_LITERAL("Did not locate record in same block as we started searching for"));
	}

	/* If this is a gvtroot type block, we have some extra processing to do. Following is a description of
	   the issue we are addressing here. If a gvtroot block is "too large" and was too large at the time
	   the scan was run, it will of course be identified by the scan as too large. Prior to running the scan,
	   the reserved bytes field was set so no more too-full blocks can be created. But if a gvtroot block is
	   identified by the scan and subsequently has to be split by normal GTM processing before the certify
	   can be done, the too-full part of the block can (in totality) end up in the right hand child of the
	   gvtroot block (not obeying the reserved bytes rule). But the gvtroot block is the only one that was
	   identified by the scan and certify may now miss the too-full block in the right child. Theoretically,
	   the entire right child chain of the gvtroot block can be too full. Our purpose here is that when we
	   have identified a gvtblock as being too full, we pause here to read the right child chain coming off
	   of that block all the way down to (but not including) block level 0. Each of these blocks will be
	   processed to check for being too full. The way we do this is to run the chain and build p1rec entries
	   in the gvtroot_rchildren[] array. When we are at the top of the processing loop, we will take these
	   array entries over records from the phase one input file. We only load up the array if it is empty.
	   Otherwise, the assumption is that we are re-processing and the issue has already been handled.
	*/
	blk_set_p = &psa->blk_set[0];
	if (gdsblk_gvtroot == blk_set_p->blk_type && 0 == psa->gvtroot_rchildren_cnt)
	{
		DBC_DEBUG(("DBC_DEBUG: Encountered gvtroot block (block %d [0x%08x]), finding/queueing children\n",
			   blk_set_p->blk_num, blk_set_p->blk_num));
		save_block_depth = psa->block_depth;	/* These reads are temporary and should not remain in cache so
							   we will restore block_depth after we are done.
							*/
		/* Attempting to locate the maximum possible key for this database should read the list of right
		   children into the cache. Pretty much any returncode from dbc_find_record is possible. We usually
		   aren't going to find the global which may come up as not found or an integrity error or it could
		   possibly even be found. Just go with what it gives us. Not much verification we can do on it.
		*/
		blk_index = dbc_find_record(psa, psa->max_key, 0, 0, gdsblk_gvtroot, TRUE);
		/* Pull children (if any) out of cache and put into queue for later processing */
		for (blk_index = save_block_depth + 1;
		     blk_index <= psa->block_depth && gdsblk_gvtleaf != psa->blk_set[blk_index].blk_type;
		     ++blk_index, ++psa->gvtroot_rchildren_cnt)
		{	/* Fill in p1rec type entry in gvtroot_rchildren[] for later */
			DBC_DEBUG(("DBC_DEBUG: Right child block: blk_index: %d  blk_num: %d [0x%08x]  blk_levl: %d\n",
				   blk_index, psa->blk_set[blk_index].blk_num, psa->blk_set[blk_index].blk_num,
				   psa->blk_set[blk_index].blk_levl));
			psa->gvtroot_rchildren[psa->gvtroot_rchildren_cnt].tn = psa->blk_set[blk_index].tn;
			psa->gvtroot_rchildren[psa->gvtroot_rchildren_cnt].blk_num = psa->blk_set[blk_index].blk_num;
			psa->gvtroot_rchildren[psa->gvtroot_rchildren_cnt].blk_type = psa->blk_set[blk_index].blk_type;
			psa->gvtroot_rchildren[psa->gvtroot_rchildren_cnt].blk_levl = psa->blk_set[blk_index].blk_levl;
			psa->gvtroot_rchildren[psa->gvtroot_rchildren_cnt].akey_len = 0;
		}
		psa->block_depth = save_block_depth;
		blk_index = 0;	/* reset to start *our* work in the very first block */
	}
	/* Now we have done the gvtroot check if we were going to. If this particular block has sufficient room in it
	 * we don't need to split it of course.
	 */
	if (UNIX_ONLY(8) VMS_ONLY(9) <= blk_size - blk_len)
	{	/* This block has room now - no longer need to split it */
		DBC_DEBUG(("DBC_DEBUG: Block not processed as it now has sufficient room\n"));
		psa->blks_bypassed++;
		psa->blks_read++;
		if (psa->blk_set[0].found_in_cache)
			psa->blks_cached++;
		return FALSE;	/* No restart needed */
	}

	/* Beginning of block update/split logic. We need to process the blocks in the reverse order from the
	   tree path. This means blk_set[0] which is actually the block we want to split must be the first
	   in our path. We then need to process the block array backwards in case the changes made to those
	   records cause subsequent splits.

	   First order of business is to find a suitable place to split this block .. Run through
	   the records in the block until we are "halfway" through the block. Split so that the first record
	   (after the first) whose end point is in the "second half" of the block will be the first record of
	   the second half or right hand side block after the split. This makes sure that the left side has at
	   least one record in it. We already know that this block has at least 2 records in it or it would not
	   need splitting.
	*/
	rec_p = blk_p + SIZEOF(v15_blk_hdr);
	blk_set_p->curr_rec = rec_p;
	dbc_find_key(psa, blk_set_p->curr_blk_key, rec_p, blk_set_p->blk_levl);
	GET_USHORT(us_rec_len, &((rec_hdr *)rec_p)->rsiz);
	curr_rec_len = us_rec_len;
	next_rec_p = rec_p + curr_rec_len;
	blk_set_p->curr_match = 0;		/* First record of block always cmpc 0 */
	blk_len = ((v15_blk_hdr_ptr_t)blk_p)->bsiz;
	blk_endp = blk_p + blk_len;
	mid_point = blk_p + blk_size / 2;
	do
	{	/* Keep scanning the next record until you find the split point which is the first record that straddles the
		 * mid-point of the block. This loop makes sure the prev_key and curr_key fields are correctly set when we
		 * enter the processing loop below.
		 */
		blk_set_p->prev_match = blk_set_p->curr_match;
		memcpy(blk_set_p->prev_blk_key, blk_set_p->curr_blk_key, (SIZEOF(dbc_gv_key) + blk_set_p->curr_blk_key->end));
		rec_p = next_rec_p;	/* Must be at least one record in LHS and one in RHS */
		blk_set_p->prev_rec = blk_set_p->curr_rec;
		blk_set_p->curr_rec = rec_p;
		GET_USHORT(us_rec_len, &((rec_hdr *)rec_p)->rsiz);
		curr_rec_len = us_rec_len;
		dbc_find_key(psa, blk_set_p->curr_blk_key, rec_p, blk_set_p->blk_levl);
		blk_set_p->curr_match = EVAL_CMPC((rec_hdr *)rec_p);
		next_rec_p = rec_p + curr_rec_len;
		if (next_rec_p >= blk_endp)	/* We have reached the last record in the block. Cannot skip anymore. */
			break;
		if (next_rec_p >= mid_point)
		{	/* The current record straddles the mid-point of the almost-full block. This is most likely going
			 * to be the split point. If splitting at the current record causes the RHS block to continue to
			 * be too-full and there is still room in the LHS block we will scan one more record in this loop.
			 * Scanning this one more record should make the RHS block no longer too-full. This is asserted below.
			 */
			/* Compute the sizes of the LHS and RHS blocks assuming the current record moves into each of them */
			if (blk_set_p->blk_levl)
			{	/* Index block. The current record is changed into a *-key (a simple star key rec) */
				new_lh_blk_len = (int)((rec_p - blk_p) + BSTAR_REC_SIZE);
			} else
			{	/* Data block. Always simple split (no inserted record) */
				new_lh_blk_len = (int)(next_rec_p - blk_p);
				assert(gdsblk_gvtleaf == blk_set_p->blk_type || gdsblk_dtleaf == blk_set_p->blk_type);
			}
			assert(0 < new_lh_blk_len);
			/* assert that the LHS block without the current record is guaranteed not to be too-full */
			assert((new_lh_blk_len - (next_rec_p - rec_p)) <= psa->max_blk_len);
			/* Right hand side has key of curr_rec expanded since is first key of blcok */
			new_rh_blk_len = (int)(SIZEOF(v15_blk_hdr) + blk_set_p->curr_match + blk_len - (rec_p - blk_p) );
			assert(0 < new_rh_blk_len);
			if ((new_rh_blk_len <= psa->max_blk_len) || (new_lh_blk_len > psa->max_blk_len))
				break;
			assert(FALSE == first_time);	/* assert we never scan more than one record past mid-point of the block */
			DEBUG_ONLY(first_time = TRUE;)
		}
	} while (TRUE);
	assert((rec_p - blk_p) < ((v15_blk_hdr_ptr_t)blk_p)->bsiz);

	/* Block processing loop */
	bottom_tree_index = psa->block_depth;	/* Record end of the tree in case need bit map blocks later */
	update_array_ptr = update_array;		/* Reset udpate array */
	DBC_DEBUG(("DBC_DEBUG: Beginning split processing loop\n"));
	for (completed = FALSE; !completed;)
	{	/* Backwards process until we hit a block with no changes to it */
		DBC_DEBUG(("DBC_DEBUG: ******** Top of blk process loop for block index %d\n", blk_index));
		assert(0 <= blk_index);
		blk_set_p = &psa->blk_set[blk_index];
		assert(blk_set_p->blk_len == ((v15_blk_hdr_ptr_t)blk_set_p->old_buff)->bsiz);
		assert(blk_set_p->blk_levl == ((v15_blk_hdr_ptr_t)blk_set_p->old_buff)->levl);
		curr_blk_len = blk_set_p->blk_len;
		curr_blk_levl = blk_set_p->blk_levl;
		if (0 != blk_set_p->ins_rec.ins_key->end)
		{
			ins_key_len = blk_set_p->ins_rec.ins_key->end + 1;
			ins_rec_len = ins_key_len + SIZEOF(block_id);	/* We only ever insert index records */
		} else
			ins_key_len = ins_rec_len = 0;
		blk_p = blk_set_p->old_buff;
		/* If ins_rec_len has a non-zero value, then we need to reset the values for prev_match and
		   key_match. These values were computed using the original scan key as their basis. Now we
		   are using these fields to insert a new key. The positioning is still correct but the
		   number of matching characters is potentially different.
		*/
		if (ins_rec_len)
		{
			if (0 != blk_set_p->prev_blk_key->end)
			{	/* There is a "previous record" */
				insert_point = dbc_match_key(blk_set_p->prev_blk_key, blk_set_p->blk_levl,
							     blk_set_p->ins_rec.ins_key, &blk_set_p->prev_match);
				assert(!insert_point);	/* This is prior to insert point (sanity check) */
			}
			insert_point = dbc_match_key(blk_set_p->curr_blk_key, blk_set_p->blk_levl,
						     blk_set_p->ins_rec.ins_key, &blk_set_p->curr_match);
			assert(insert_point);	/* This is supposed to *be* the insert point */
		}
		/* Make convenient copies of some commonly used record fields */
		curr_rec_cmpc = EVAL_CMPC((rec_hdr *)blk_set_p->curr_rec);
		curr_rec_shrink = blk_set_p->curr_match - curr_rec_cmpc;
		curr_rec_offset = (int)(blk_set_p->curr_rec - blk_set_p->old_buff);
		GET_USHORT(us_rec_len, &((rec_hdr *)blk_set_p->curr_rec)->rsiz);
		curr_rec_len = us_rec_len;
		prev_rec_offset = (int)(blk_set_p->prev_rec - blk_set_p->old_buff);
		got_root = (gdsblk_dtroot == blk_set_p->blk_type) || (gdsblk_gvtroot == blk_set_p->blk_type);
		/* Decide if this record insert (if an insert exists) will cause a block split or not. If this
		   is the first block in the tree (the one we got from the phase 1 file), there will be no insert.
		   If we find a block that does not need to change, we are done and can exit the loop.
		   This differs from the regular GT.M runtime which must keep checking even the split blocks
		   but since we never add data to a level 0 block being split, we will never create split-off
		   blocks that themselves are (still) too full.
		*/
		assert(gdsblk_read == blk_set_p->usage);
		new_blk_len = (int)(ins_rec_len ? (curr_blk_len + curr_rec_cmpc + SIZEOF(rec_hdr) + ins_rec_len
					      - blk_set_p->prev_match - blk_set_p->curr_match)
			       : curr_blk_len);		/* No inserted rec, size does not change */
		if (new_blk_len <= psa->max_blk_len)
		{	/* "Simple" case .. we do not need a block split - only (possibly) a record added. Note
			   that this is the only path where there may not be a "previous" record so we
			   have to watch for that possibility.
			*/
			assert(0 != blk_index);		/* Never insert a record into target blk so should never be here */
			/* In this path we should always have an inserted record length. We should have detected we
			   were done in an earlier loop iteration.
			*/
			assert(ins_rec_len);
			DBC_DEBUG(("DBC_DEBUG: Block index %d is a simple update\n", blk_index));
			/* We must have an insert at this point and since we only ever insert records into
			   index blocks, we must be in that situation */
			assert(0 != curr_blk_levl);
			blk_set_p->usage = gdsblk_update;	/* It's official .. blk is being modified */
			/* We have a record to insert into this block but no split is needed */
			BLK_INIT(bs_ptr, bs1);
			blk_set_p->upd_addr = bs1;		/* Save address of our update array */
			if (0 != blk_set_p->prev_blk_key->end)
			{	/* First piece is block prior to the record + key + value */
				BLK_SEG(bs_ptr,
					blk_set_p->old_buff + SIZEOF(v15_blk_hdr),
					(curr_rec_offset - SIZEOF(v15_blk_hdr)));
			}
			BLK_ADDR(ins_rec_hdr, SIZEOF(rec_hdr), rec_hdr);
			/* Setup new record header */
			new_rec_len = (int)(SIZEOF(rec_hdr) + ins_rec_len - blk_set_p->prev_match);
			ins_rec_hdr->rsiz = new_rec_len;
			SET_CMPC(ins_rec_hdr, blk_set_p->prev_match);
			BLK_SEG(bs_ptr, (sm_uc_ptr_t)ins_rec_hdr, SIZEOF(rec_hdr));
			/* Setup key */
			BLK_ADDR(cp1,
				 blk_set_p->ins_rec.ins_key->end + 1 - blk_set_p->prev_match,
				 unsigned char);
			memcpy(cp1, blk_set_p->ins_rec.ins_key->base + blk_set_p->prev_match,
			       blk_set_p->ins_rec.ins_key->end + 1 - blk_set_p->prev_match);
			BLK_SEG(bs_ptr, cp1, blk_set_p->ins_rec.ins_key->end + 1 - blk_set_p->prev_match);
			/* Setup value (all index records have value of size "block_id". The proper value is
			   either there already or will be when we go to commit these changes. */
			BLK_SEG(bs_ptr, (sm_uc_ptr_t)&blk_set_p->ins_rec.blk_id, SIZEOF(block_id));
			/* For index blocks, we know that since a star key is the last record in the block
			   (which is the last record that can be curr_rec) that there is a trailing portion
			   of the block we need to output.
			*/
			BLK_ADDR(next_rec_hdr, SIZEOF(rec_hdr), rec_hdr);	/* Replacement rec header */
			next_rec_hdr->rsiz = curr_rec_len - curr_rec_shrink;
			SET_CMPC(next_rec_hdr, blk_set_p->curr_match);
			BLK_SEG(bs_ptr, (sm_uc_ptr_t)next_rec_hdr, SIZEOF(rec_hdr));
			remain_offset = curr_rec_shrink + SIZEOF(rec_hdr);	/* Where rest of record plus any
										   further records begin */
			remain_len = curr_blk_len - curr_rec_offset;
			BLK_SEG(bs_ptr,
				blk_set_p->curr_rec + remain_offset,
				remain_len - remain_offset);
			if (0 == BLK_FINI(bs_ptr, bs1))
				GTMASSERT;
			assert(blk_seg_cnt == new_blk_len);
			DBC_DEBUG(("DBC_DEBUG: Stopping block scan after simple update (no further inserts to previous lvls)\n"));
			completed = TRUE;
			break;
		} else
		{	/* The block is either already too large or would be too large when the record is inserted
			   and so it must be split.

			   There are two different ways a block can be split. It can either be split so that:

			   (1) the inserted record is at the end of the left block or,

			   (2) the record is the first record in the right half.

			   Compute the left/right block sizes for these two options and see which one does not
			   force a secondary block split (one of them must be true here unlike in GT.M code because
			   here we are NEVER adding a record to a level 0 block, we only split lvl 0 blocks as
			   needed). Note that the case where we are splitting a level 0 block with no record insert
			   is treated as an unremarkable variant of option (1) as described above.

			   Follow the conventions of gvcst_put (LHS to new block, RHS to old block):

			   (1) If we are inserting the record into the lefthand side then a new split-off block will
			   receive the first part of the block including the record. The remainder of the block is
			   placed into the current (existing) block.

			   (2) If we are putting the record into the righthand side, then a new split-off block will
			   receive the first part of the block. The new record plus the remainder of the block is
			   placed into the current block.

			   The sole exception to the above is if a root block (either DT or GVT) is being split. In
			   that case, BOTH the LHS and RHS become NEW blocks and the root block is (a) increased in
			   level and (b) contains only entries for the two created blocks.

			   Note that gvcst_put has several additional checks and balances here that we are forgoing
			   such as making sure the blocks are as balanced as possible, concurrency concerns, etc. They
			   add un-needed complications to this one-time code. Any inefficiencies here can be undone
			   with a pass of MUPIP REORG.
			*/
			DBC_DEBUG(("DBC_DEBUG: Block index %d needs to be split\n", blk_index));
			/*  First up is split so that the inserted record (if any) is the last record in the left
			    hand block. Note if this is an index block, the last record must be a star key rec as per
			    option (1) above.
			*/
			if (curr_blk_levl)
				/* Index block. Two cases: (a) We are adding a key to the end in which case it is just
				   a simple star key rec or (b) No record is being added so the previous record is
				   changed into a star key rec.
				*/
				new_lh_blk_len = (int)(curr_rec_offset + BSTAR_REC_SIZE
					- (ins_rec_len ? 0 : (blk_set_p->curr_rec - blk_set_p->prev_rec)));
			else
			{
				/* Data block. Always simple split (no inserted record) */
				new_lh_blk_len = curr_rec_offset;
				assert(gdsblk_gvtleaf == blk_set_p->blk_type || gdsblk_dtleaf == blk_set_p->blk_type);
			}
			assert(0 < new_lh_blk_len);
			/* Right hand side has key of curr_rec expanded since is first key of blcok */
			new_rh_blk_len = (int)(SIZEOF(v15_blk_hdr) + EVAL_CMPC((rec_hdr *)blk_set_p->curr_rec) +
					       (curr_blk_len - curr_rec_offset));
			assert(0 < new_rh_blk_len);
			/* Common initialization */
			++psa->block_depth;			/* Need a new block to split into */
			if (MAX_BLOCK_INFO_DEPTH <= psa->block_depth)
				GTMASSERT;
			DBC_DEBUG(("DBC_DEBUG: Block index %d used for newly created split (lhs) block\n", psa->block_depth));
			blk_set_new_p = &psa->blk_set[psa->block_depth];
			dbc_init_blk(psa, blk_set_new_p, -1, gdsblk_create, new_lh_blk_len, curr_blk_levl);
			if (got_root)
				/* If root, the LHS sub-block is a different type */
				blk_set_new_p->blk_type = (gdsblk_gvtroot == blk_set_p->blk_type)
					? gdsblk_gvtindex : gdsblk_dtindex;
			else
				blk_set_new_p->blk_type = blk_set_p->blk_type;
			/* Complete our LHS block */
			BLK_INIT(bs_ptr, bs1);		/* Our new block to create */
			blk_set_new_p->upd_addr = bs1;
			level_0 = (0 == curr_blk_levl);
			/* See if they fit in their respective blocks */
			if (level_0 || (new_lh_blk_len <= psa->max_blk_len) && (new_rh_blk_len <= psa->max_blk_len))
			{	/* Method 1 - record goes to left-hand side */
				DBC_DEBUG(("DBC_DEBUG: Method 1 block lengths: lh: %d  rh: %d  max_blk_len: %d\n",
					   new_lh_blk_len, new_rh_blk_len, psa->max_blk_len));
				/* New update array for new block */
				if (level_0)
				{	/* Level 0 block, we are only splitting it -- never adding a record */
					assert(curr_rec_offset <= psa->max_blk_len);
					BLK_SEG(bs_ptr, blk_set_p->old_buff + SIZEOF(v15_blk_hdr),
						curr_rec_offset - SIZEOF(v15_blk_hdr));
					assert(0 == ins_rec_len);	/* Never insert records to lvl0 */
					if (new_rh_blk_len > psa->max_blk_len)
					{	/* Case of a data block that has a DBCREC2BIG error unnoticed by DBCERTIFY SCAN.
						 * Should not happen normally. But in case it does in production, we will handle
						 * it by NOT certifying the database and requiring a rerun of the SCAN
						 */
						assert(FALSE);
						gtm_putmsg(VARLSTCNT(6) ERR_DBCREC2BIGINBLK, 4,
							   blk_num, psa->dbc_cs_data->max_rec_size,
							   psa->dbc_gv_cur_region->dyn.addr->fname_len,
							   psa->dbc_gv_cur_region->dyn.addr->fname);
						psa->blk_process_errors++;	/* must be zero to certify db at end */
					}
				} else
				{	/* Index block -- may or may not be adding a record.
					   If adding a record, the inserted record becomes a star key record.
					   If not adding a record the last record is morphed into a star key record.
					*/
					BLK_SEG(bs_ptr, blk_set_p->old_buff + SIZEOF(v15_blk_hdr),
						(ins_rec_len ? curr_rec_offset : prev_rec_offset)
						- SIZEOF(v15_blk_hdr));
					BLK_ADDR(new_star_hdr, SIZEOF(rec_hdr), rec_hdr);
					new_star_hdr->rsiz = BSTAR_REC_SIZE;
					SET_CMPC(new_star_hdr, 0);
					BLK_SEG(bs_ptr, (uchar_ptr_t)new_star_hdr, SIZEOF(rec_hdr));
					BLK_SEG(bs_ptr, (ins_rec_len ? (uchar_ptr_t)&blk_set_p->ins_rec.blk_id
							 : (blk_set_p->prev_rec + SIZEOF(rec_hdr)
							    + blk_set_p->prev_blk_key->end + 1
							    - EVAL_CMPC((rec_hdr *)blk_set_p->prev_rec))),
						SIZEOF(block_id));
				}
				/* Complete our LHS block */
				if (0 == BLK_FINI(bs_ptr, bs1))
					GTMASSERT;
				assert(blk_seg_cnt == new_lh_blk_len);
				/* Remember key of last record in this block */
				if (0 == ins_rec_len)
					last_rec_key = blk_set_p->prev_blk_key;
				else
					last_rec_key = blk_set_p->ins_rec.ins_key;
				if (!got_root)
				{	/* New block created, insert record to it in parent block. To do this we create
					   a record with the last key in this LH block to be inserted between curr_rec
					   and prev_rec of the parent block.
					*/
					if (0 == blk_index)
						blk_set_prnt_p = &psa->blk_set[bottom_tree_index]; /* Cycle back up to parent */
					else
						blk_set_prnt_p = blk_set_p - 1;
					assert(blk_set_prnt_p != &psa->blk_set[0]);
					assert(NULL != last_rec_key);
					/* Note: We do not need the "+ 1" on the key length since SIZEOF(dbc_gv_key) contains
					   the first character of the key so the "+ 1" to get the last byte of the key is
					   already integrated into the length
					*/
					memcpy(blk_set_prnt_p->ins_rec.ins_key, last_rec_key,
					       SIZEOF(dbc_gv_key) + last_rec_key->end);
					/* Setup so that creation of the blk_set_new_p block can then set its block id into
					   our parent block's insert rec buffer which will be made part of the inserted
					   record at block build time
					*/
					blk_set_new_p->ins_blk_id_p = &blk_set_prnt_p->ins_rec.blk_id;
					blk_set_rhs_p = blk_set_p;	/* Use original block for rhs */
					blk_set_rhs_p->usage = gdsblk_update;
				} else
				{	/* Have root block: need to put the RHS into a new block too */
					DBC_DEBUG(("DBC_DEBUG: Splitting root block, extra block to be created\n"));
					++psa->block_depth;			/* Need a new block to split into */
					if (MAX_BLOCK_INFO_DEPTH <= psa->block_depth)
						GTMASSERT;
					blk_set_rhs_p = &psa->blk_set[psa->block_depth];
					dbc_init_blk(psa, blk_set_rhs_p, -1, gdsblk_create, new_rh_blk_len, curr_blk_levl);
					/* We will put the pointers to both this block and the RHS we build next
					   into the original root block -- done later when RHS is complete */
					/* If root, the RHS sub-block is a different type */
					blk_set_rhs_p->blk_type = (gdsblk_gvtroot == blk_set_p->blk_type)
						? gdsblk_gvtindex : gdsblk_dtindex;
				}

				/**** Now build RHS into either current or new block ****/
				BLK_INIT(bs_ptr, bs1);
				blk_set_rhs_p->upd_addr = bs1;		/* Block building roadmap.. */
				BLK_ADDR(next_rec_hdr, SIZEOF(rec_hdr), rec_hdr);
				next_rec_hdr->rsiz = curr_rec_len + curr_rec_cmpc;
				SET_CMPC(next_rec_hdr, 0);
				BLK_SEG(bs_ptr, (uchar_ptr_t)next_rec_hdr, SIZEOF(rec_hdr));
				/* Copy the previously compressed part of the key out of curr_rec. Note, if this
				   key is a star rec key, nothing is written because cmpc is zero */
				if (curr_rec_cmpc)
				{
					BLK_ADDR(cp1, curr_rec_cmpc, unsigned char);
					memcpy(cp1, blk_set_p->curr_blk_key->base, curr_rec_cmpc);
					BLK_SEG(bs_ptr, cp1, curr_rec_cmpc);
				}
				/* Remainder of existing block */
				BLK_SEG(bs_ptr,
					blk_set_p->curr_rec + SIZEOF(rec_hdr),
					curr_blk_len - curr_rec_offset - SIZEOF(rec_hdr));
				/* Complete update array */
				if (0 == BLK_FINI(bs_ptr, bs1))
					GTMASSERT;
				assert(blk_seg_cnt == new_rh_blk_len);
			} else
			{	/* Recompute sizes for inserted record being in righthand block as per
				   method (2) */
				DBC_DEBUG(("DBC_DEBUG: Method 1 created invalid blocks: lh: %d  rh: %d  "
					   "max_blk_len: %d -- trying method 2\n", new_lh_blk_len, new_rh_blk_len,
					   psa->max_blk_len));
				/* By definition we *must* have an inserted record in this path */
				assert(0 != ins_rec_len);
				/* New block sizes - note because we *must* be inserting a record in this method,
				   the only case considered here is when we are operating on an index block.
				*/
				assert(!level_0);
				/* Last record turns into star key record */
				new_lh_blk_len = (int)(curr_rec_offset + BSTAR_REC_SIZE -
						       (blk_set_p->curr_rec - blk_set_p->prev_rec) );
				assert(0 < new_lh_blk_len);
				new_rh_blk_len = (int)(SIZEOF(v15_blk_hdr) + SIZEOF(rec_hdr) +
						       ins_rec_len  + curr_blk_len - (curr_rec_offset) - curr_rec_shrink);
				assert(0 < new_rh_blk_len);
				if (new_lh_blk_len > psa->max_blk_len || new_rh_blk_len > psa->max_blk_len)
				{	/* This is possible if we are inserting a record into a block (and thus we are
					   not picking the insertion point) and the insertion point is either the first or
					   next-to-last record in the block such that neither method 1 nor 2 can create blocks
					   of acceptable size. In this case, although this problem block is likely on the
					   list of blocks to process, we cannot wait and thus must perform the split now.
					   To do that, we call this same routine recursively with the necessary parms to
					   process *THIS* block. Since this will destroy all the structures we had built
					   up, signal a transaction restart which will re-read everything and should allow
					   the transaction we were processing to proceed.
					*/
					if (curr_blk_len <= psa->max_blk_len)
						/* Well, that wasn't the problem, something else is wrong */
						rts_error(VARLSTCNT(8) ERR_DBCINTEGERR, 2,
							  RTS_ERROR_STRING((char_ptr_t)psa->ofhdr.dbfn),
							  ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to split block appropriately"));
					/* If we do have to restart, we won't be able to reinvoke dbc_split_blk() with the
					   parms taken from the current blk_set_p as that array will be overwritten by the
					   recursion. Save the current blk_set_p so we can use it in a restartable context.
					*/
					restart_blk_set = *blk_set_p;
					for (restart_cnt = 0, restart_transaction = TRUE;
					     restart_transaction;
					     ++restart_cnt)
					{
						if (MAX_RESTART_CNT < restart_cnt)
							GTMASSERT;		/* No idea what could cause this */
						DBC_DEBUG(("DBC_DEBUG: *** *** Recursive call to handle too large block 0x%x\n",
							   restart_blk_set.blk_num));
						psa->block_depth_hwm = -1;	/* Zaps cache so all blocks are re-read */
						restart_transaction = dbc_split_blk(psa, restart_blk_set.blk_num,
										    restart_blk_set.blk_type, restart_blk_set.tn,
										    restart_blk_set.blk_levl);
					}
					return TRUE;	/* This transaction must restart */
				}
				DBC_DEBUG(("DBC_DEBUG: Method 2 block lengths: lh: %d  rh: %d  max_blk_len: %d\n",
					   new_lh_blk_len, new_rh_blk_len, psa->max_blk_len));

				/* Start building (new) LHS block - for this index record, the record before the split
				   becomes a new *-key.

				   Note:  If the block split was caused by our appending the new record
				   to the end of the block, this code causes the record PRIOR to the
				   current *-key to become the new *-key.
				*/
				BLK_SEG(bs_ptr,
					blk_set_p->old_buff + SIZEOF(v15_blk_hdr),
					prev_rec_offset - SIZEOF(v15_blk_hdr));
				/* Replace last record with star key rec */
				BLK_ADDR(new_star_hdr, SIZEOF(rec_hdr), rec_hdr);
				new_star_hdr->rsiz = BSTAR_REC_SIZE;
				SET_CMPC(new_star_hdr, 0);
				BLK_SEG(bs_ptr, (uchar_ptr_t)new_star_hdr, SIZEOF(rec_hdr));
				/* Output pointer from prev_rec as star key record's value */
				BLK_SEG(bs_ptr,	blk_set_p->curr_rec - SIZEOF(block_id), SIZEOF(block_id));
				/* Complete our LHS block */
				if (0 == BLK_FINI(bs_ptr, bs1))
					GTMASSERT;
				assert(blk_seg_cnt == new_lh_blk_len);
				if (!got_root)
				{
					/* New block created, insert record to it in parent block. To do this we create
					   a record with the last key in this LH block to be inserted between curr_rec
					   and prev_rec of the parent block.
					*/
					if (0 == blk_index)
						blk_set_prnt_p = &psa->blk_set[bottom_tree_index]; /* Cycle back up to parent */
					else
						blk_set_prnt_p = blk_set_p - 1;
					assert(blk_set_prnt_p != &psa->blk_set[0]);
					assert(NULL != blk_set_p->prev_blk_key);
					/* Note: We do not need the "+ 1" on the key length since SIZEOF(dbc_gv_key) contains
					   the first character of the key so the "+ 1" to get the last byte of the key is
					   already integrated into the length
					*/
					memcpy(blk_set_prnt_p->ins_rec.ins_key, blk_set_p->prev_blk_key,
					       SIZEOF(dbc_gv_key) + blk_set_p->prev_blk_key->end);
					/* Setup so that creation of the blk_set_new_p block can then set its block id into
					   our parent block's insert rec buffer which will be made part of the inserted
					   record at block build time
					*/
					blk_set_new_p->ins_blk_id_p = &blk_set_prnt_p->ins_rec.blk_id;
					blk_set_rhs_p = blk_set_p;	/* Use original block for rhs */
					blk_set_rhs_p->usage = gdsblk_update;
				} else
				{	/* Have root block: need to put the RHS into a new block too */
					DBC_DEBUG(("DBC_DEBUG: Splitting root block, extra block to be created\n"));
					++psa->block_depth;			/* Need a new block to split into */
					if (MAX_BLOCK_INFO_DEPTH <= psa->block_depth)
						GTMASSERT;
					blk_set_rhs_p = &psa->blk_set[psa->block_depth];
					/* Key for last record in the LHS block used to (re)construct root block */
					last_rec_key = blk_set_p->curr_blk_key;
					dbc_init_blk(psa, blk_set_rhs_p, -1, gdsblk_create, new_rh_blk_len, curr_blk_levl);
					/* We will put the pointers to both this block and the RHS we build next
					   into the original root block -- done later when RHS is complete */
					/* If root, the RHS sub-block is a different type */
					blk_set_rhs_p->blk_type = (gdsblk_gvtroot == blk_set_p->blk_type)
						? gdsblk_gvtindex : gdsblk_dtindex;
				}

				/**** Now build RHS into current block ****/
				BLK_INIT(bs_ptr, bs1);
				blk_set_rhs_p->upd_addr = bs1;		/* Block building roadmap.. */
				/* Build record header for inserted record. Inserted record is always for index
				   type blocks
				*/
				BLK_ADDR(ins_rec_hdr, SIZEOF(rec_hdr), rec_hdr);
				ins_rec_hdr->rsiz = SIZEOF(rec_hdr) + blk_set_p->ins_rec.ins_key->end + 1
					+ SIZEOF(block_id);
				SET_CMPC(ins_rec_hdr, 0);
				BLK_SEG(bs_ptr, (uchar_ptr_t)ins_rec_hdr, SIZEOF(rec_hdr));
				/* Now for the inserted record key */
				BLK_SEG(bs_ptr,
					blk_set_p->ins_rec.ins_key->base,
					blk_set_p->ins_rec.ins_key->end + 1);
				/* Finally the inserted record value always comes from the block_id field. It is
				   not filled in now but will be when the block it refers to is created. */
				BLK_SEG(bs_ptr,	(uchar_ptr_t)&blk_set_p->ins_rec.blk_id, SIZEOF(block_id));
				/* Record that was first in RH side now needs its cmpc (and length) reset since
				   it is now the second record in the new block. */
				BLK_ADDR(next_rec_hdr, SIZEOF(rec_hdr), rec_hdr);
				next_rec_hdr->rsiz = curr_rec_len - curr_rec_shrink;
				SET_CMPC(next_rec_hdr, blk_set_p->curr_match);
				BLK_SEG(bs_ptr, (uchar_ptr_t)next_rec_hdr, SIZEOF(rec_hdr));
				remain_offset = curr_rec_shrink + SIZEOF(rec_hdr);	/* Where rest of record plus any
											   further records begin */
				remain_len = curr_blk_len - curr_rec_offset;
				BLK_SEG(bs_ptr,
					blk_set_p->curr_rec + remain_offset,
					remain_len - remain_offset);
				if (0 == BLK_FINI(bs_ptr, bs1))
					GTMASSERT;
				assert(blk_seg_cnt == new_rh_blk_len);
			} /* else method (2) */
			if (got_root)
			{	/* If we have split a root block, we need to now set the pointers to the new LHS
				   and RHS blocks into the root block as the only records. Note this requires a
				   level increase of the tree. Hopefully we will not come across a database that is
				   already at maximum level. If so, the only way to reduce the level is to run
				   MUPIP REORG with a fairly recent vintage of GT.M
				*/
				BLK_INIT(bs_ptr, bs1);
				blk_set_p->usage = gdsblk_update;	/* It's official .. blk is being modified */
				blk_set_p->upd_addr = bs1;		/* Block building roadmap.. */
				blk_set_p->blk_levl++;			/* Needs to be at a new level */
				if (MAX_BT_DEPTH <= blk_set_p->blk_levl)
					/* Tree is too high */
					GTMASSERT;
				/* First record will have last key in LHS block */
				BLK_ADDR(next_rec_hdr, SIZEOF(rec_hdr), rec_hdr);
				next_rec_hdr->rsiz = SIZEOF(rec_hdr) + last_rec_key->end + 1 + SIZEOF(block_id);
				SET_CMPC(next_rec_hdr, 0);
				BLK_SEG(bs_ptr, (uchar_ptr_t)next_rec_hdr, SIZEOF(rec_hdr));
				BLK_SEG(bs_ptr, last_rec_key->base, (last_rec_key->end + 1));
				BLK_ADDR(lhs_block_id_p, SIZEOF(block_id), block_id);	/* First record's value */
				BLK_SEG(bs_ptr, (uchar_ptr_t)lhs_block_id_p, SIZEOF(block_id));
				blk_set_new_p->ins_blk_id_p = lhs_block_id_p;	/* Receives block id when created */
				/* Second record is a star key record pointing to the RHS block */
				BLK_ADDR(new_star_hdr, SIZEOF(rec_hdr), rec_hdr);
				new_star_hdr->rsiz = BSTAR_REC_SIZE;
				SET_CMPC(new_star_hdr, 0);
				BLK_SEG(bs_ptr, (uchar_ptr_t)new_star_hdr, SIZEOF(rec_hdr));
				BLK_ADDR(rhs_block_id_p, SIZEOF(block_id), block_id);	/* First record's value */
				BLK_SEG(bs_ptr, (uchar_ptr_t)rhs_block_id_p, SIZEOF(block_id));
				blk_set_rhs_p->ins_blk_id_p = rhs_block_id_p;	/* Receives block id when created */
				/* Complete update array */
				if (0 == BLK_FINI(bs_ptr, bs1))
					GTMASSERT;
				/* The root block is the last one we need to change */
				DBC_DEBUG(("DBC_DEBUG: Stopping block scan as blk_index %d is a root block\n", blk_index));
				completed = TRUE;
				break;
			}
		} /* else need block split */
		if (0 != blk_index)
			blk_index--;	/* working backwards.. */
		else
			blk_index = bottom_tree_index;
	} /* while !completed */
	assert(completed);

	/* Check that we have sufficient free blocks to create the blccks we need (if any) */
	created_blocks = psa->block_depth - bottom_tree_index;
	if (created_blocks > psa->dbc_cs_data->trans_hist.free_blocks)
	{	/* We have a slight problem in that this transaction requires more free blocks than are
		   available. Our recourse is to flush the current file-header preserving any changes we
		   have already made, close the file and execute a mupip command to perform an extension before
		   re-opening the db for further processing.
		*/
		DBC_DEBUG(("DBC_DEBUG: Insufficient free blocks for this transaction - calling MUPIP EXTEND\n"));
		dbc_flush_fhead(psa);
		dbc_close_db(psa);
		extent_size = MAX(psa->dbc_cs_data->extension_size, created_blocks);
		/* Now build command file to perform the MUPIP EXTEND for the region */
		dbc_open_command_file(psa);
		dbc_write_command_file(psa, MUPIP_START);
		strcpy((char_ptr_t)psa->util_cmd_buff, MUPIP_EXTEND);
		strcat((char_ptr_t)psa->util_cmd_buff, (char_ptr_t)psa->ofhdr.regname);
		strcat((char_ptr_t)psa->util_cmd_buff, " "OPTDELIM"B=");
		chr_p = psa->util_cmd_buff + strlen((char_ptr_t)psa->util_cmd_buff);
		chr_p = i2asc(chr_p, extent_size);
		*chr_p = 0;
		dbc_write_command_file(psa, (char_ptr_t)psa->util_cmd_buff);
		UNIX_ONLY(dbc_write_command_file(psa, "EOF"));
		dbc_close_command_file(psa);
		dbc_run_command_file(psa, "MUPIP", (char_ptr_t)psa->util_cmd_buff, FALSE);
		/* Seeing as how it is very difficult to (in portable code) get a coherent error code back from
		   an executed command, we will just assume it worked, open the database back in and see if in
		   fact it did actually extend sufficiently. If not, this is a perm error and we stop here.
		*/
		dbc_init_db(psa);
		if (created_blocks > psa->dbc_cs_data->trans_hist.free_blocks)
			rts_error(VARLSTCNT(4) ERR_DBCNOEXTND, 2, RTS_ERROR_STRING((char_ptr_t)psa->ofhdr.dbfn));
		/* Database is now extended -- safest bet is to restart this particular update so that it is certain
		   nothing else got in besides the extention.
		*/
		DBC_DEBUG(("DBC_DEBUG: Restarting processing of this p1rec due to DB extension\n"));
		return TRUE;

	}
	/* The update arrarys are complete, we know there are sufficient free blocks in the database to accomodate
	   the splitting we have to do.
	*/
	bottom_created_index = psa->block_depth;	/* From here on out are bit map blocks */
	if (0 != created_blocks)
	{	/* Run through the created blocks assigning block numbers and filling the numbers into the buffers
		   that need them. If we didn't create any blocks, we know we didn't split any and there is nothing
		   to do for this p1 record.
		*/
		total_blks = psa->dbc_cs_data->trans_hist.total_blks;
		local_map_max = DIVIDE_ROUND_UP(total_blks, psa->dbc_cs_data->bplmap);
		DBC_DEBUG(("DBC_DEBUG: Assigning block numbers to created DB blocks\n"));
		for (blk_index = psa->block_depth, blk_set_p = &psa->blk_set[blk_index];
		     bottom_tree_index < blk_index; --blk_index, --blk_set_p)
		{
			assert(gdsblk_create == blk_set_p->usage);
			assert(NULL != blk_set_p->upd_addr);
			/* Find and allocate a database block for this created block */
			assert(NULL != blk_set_p->ins_blk_id_p);	/* Must be a place to put the block id */
			/* First find local bit map with some room in it */
			lclmap_not_full = bmm_find_free(psa->hint_blk / psa->dbc_cs_data->bplmap,
							(sm_uc_ptr_t)psa->dbc_cs_data->master_map,
							local_map_max);
			if (NO_FREE_SPACE == lclmap_not_full)
			{
				assert(FALSE);
				rts_error(VARLSTCNT(5) ERR_DBCINTEGERR, 2, RTS_ERROR_STRING((char_ptr_t)psa->ofhdr.dbfn),
					  ERR_BITMAPSBAD);
			}
			if (ROUND_DOWN2(psa->hint_blk, psa->dbc_cs_data->bplmap) != lclmap_not_full)
				psa->hint_lcl = 1;
			bitmap_blk_num = lclmap_not_full * psa->dbc_cs_data->bplmap;
			/* Read this bitmap in. Note it may already exist in the cache (likely for multiple creates) */
			lbm_blk_index = dbc_read_dbblk(psa, bitmap_blk_num, gdsblk_bitmap);
			blk_set_bm_p = &psa->blk_set[lbm_blk_index];
			assert(IS_BML(blk_set_bm_p->old_buff));	/* Verify we have a bit map block */
			assert(ROUND_DOWN2(blk_set_bm_p->blk_num, psa->dbc_cs_data->bplmap) == blk_set_bm_p->blk_num);
			if (ROUND_DOWN2(psa->dbc_cs_data->trans_hist.total_blks, psa->dbc_cs_data->bplmap) == bitmap_blk_num)
				/* This bitmap is the last one .. compute total blks in partial this bitmap */
				blks_this_lmap = (psa->dbc_cs_data->trans_hist.total_blks - bitmap_blk_num);
			else
				/* Regular bitmap (not last one) */
				blks_this_lmap = psa->dbc_cs_data->bplmap;
			lcl_map_p = blk_set_bm_p->old_buff + SIZEOF(v15_blk_hdr);
			lcl_blk = psa->hint_lcl = bm_find_blk(psa->hint_lcl, lcl_map_p, blks_this_lmap, &dummy_bool);
			if (NO_FREE_SPACE == lcl_blk)
			{
				assert(FALSE);
				rts_error(VARLSTCNT(5) ERR_DBCINTEGERR, 2, RTS_ERROR_STRING((char_ptr_t)psa->ofhdr.dbfn),
					  ERR_BITMAPSBAD);
			}
			/* Found a free block, mark it busy. Note that bitmap blocks are treated somewhat differently
			   than other blocks. We do not create an update array for them but just change the copy in
			   old_buff as appropriate.
			*/
			bml_busy(lcl_blk, lcl_map_p);
			blk_set_bm_p->usage = gdsblk_update;
			/* See if entire block is full - if yes, we need to mark master map too */
			psa->hint_lcl = bml_find_free(psa->hint_lcl, lcl_map_p, blks_this_lmap);
			if (NO_FREE_SPACE == psa->hint_lcl)
			{	/* Local map was filled .. clear appropriate master map bit */
				DBC_DEBUG(("DBC_DEBUG: -- Local map now full - marking master map\n"));
				bit_clear(bitmap_blk_num / psa->dbc_cs_data->bplmap, psa->dbc_cs_data->master_map);
			}
			assert(lcl_blk);	/* Shouldn't be zero as that is for the lcl bitmap itself */
			allocated_blk_num = psa->hint_blk = bitmap_blk_num + lcl_blk;
			DBC_DEBUG(("DBC_DEBUG: -- The newly allocated block for block index %d is 0x%x\n",
				   blk_index, allocated_blk_num));
			/* Fill this block number in the places it needs to be filled */
			assert(-1 == blk_set_p->blk_num);
			blk_set_p->blk_num = allocated_blk_num;
			*blk_set_p->ins_blk_id_p = allocated_blk_num;
			psa->dbc_cs_data->trans_hist.free_blocks--;	/* There is one fewer free blocks tonite */
			assert(0 <= (int)psa->dbc_cs_data->trans_hist.free_blocks);
			psa->dbc_fhdr_dirty = TRUE;
		}
		/* Now that all the block insertions have been filled in, run the entire chain looking for
		   both created and updated blocks. Build the new versions of their blocks in new_buff.
		*/
		DBC_DEBUG(("DBC_DEBUG: Create new and changed blocks via their update arrays\n"));
		for (blk_index = psa->block_depth, blk_set_p = &psa->blk_set[blk_index]; 0 <= blk_index; --blk_index, --blk_set_p)
		{	/* Run through the update array for this block */
			blk_sega_p = (blk_segment *)blk_set_p->upd_addr;
			if (gdsblk_bitmap == blk_set_p->blk_type || gdsblk_read == blk_set_p->usage)
			{	/* Bitmap blocks are updated in place and of course read blocks have no updates */
				DBC_DEBUG(("DBC_DEBUG: -- Block index %d bypassed for type (%d) or usage (%d)\n",
					   blk_index,  blk_set_p->blk_type, blk_set_p->usage));
				assert(NULL == blk_sega_p);
				continue;
			}
			DBC_DEBUG(("DBC_DEBUG: -- Block index %d being (re)built\n", blk_index));
			assert(NULL != blk_sega_p);
			new_blk_len = INTCAST(blk_sega_p->len);
			new_blk_p = blk_set_p->new_buff;
			((v15_blk_hdr_ptr_t)new_blk_p)->bsiz = blk_set_p->blk_len = new_blk_len;
			((v15_blk_hdr_ptr_t)new_blk_p)->levl = blk_set_p->blk_levl;
			/* VMS has an unalighed tn. All UNIX variants have an aligned TN */
			VMS_ONLY(PUT_ULONG(&((v15_blk_hdr_ptr_t)new_blk_p)->tn, psa->dbc_cs_data->trans_hist.curr_tn));
			UNIX_ONLY(((v15_blk_hdr_ptr_t)new_blk_p)->tn = psa->dbc_cs_data->trans_hist.curr_tn);
			new_blk_p += SIZEOF(v15_blk_hdr);
			for (blk_array_top = (blk_segment *)blk_sega_p->addr, blk_sega_p++;
			     blk_sega_p <= blk_array_top; blk_sega_p++)
			{	/* Start with first subtantive array entry ([1]) and do the segment move thing */
				memcpy(new_blk_p, blk_sega_p->addr, blk_sega_p->len);
				new_blk_p += blk_sega_p->len;
			}
			assert((new_blk_p - blk_set_p->new_buff) == new_blk_len);
		}
		/* One last pass through the block list to do the physical IO on the database */
		psa->fc->op = FC_WRITE;
		psa->fc->op_len = blk_size;		/* Just write the full block out regardless */
		DBC_DEBUG(("DBC_DEBUG: Flush all modified blocks out to disk for this transaction\n"));
		psa->dbc_critical = TRUE;
		for (blk_index = psa->block_depth, blk_set_p = &psa->blk_set[blk_index]; 0 <= blk_index; --blk_index, --blk_set_p)
		{	/* Output all modified/created blocks */
			if (gdsblk_create != blk_set_p->usage)
			{	/* We read everything but created blocks and some of them were found in cache */
				psa->blks_read++;
				if (blk_set_p->found_in_cache)
					psa->blks_cached++;
			}
			if (gdsblk_read == blk_set_p->usage)
			{
				DBC_DEBUG(("DBC_DEBUG: -- Block index %d bypassed for usage (read)\n", blk_index));
				continue;	/* Nothing to do for read-only block */
			}
			if (gdsblk_bitmap == blk_set_p->blk_type)
			{	/* Bitmap blocks are built in old_buff, swap with new_buff. This also lets the
				   buffer be reused correctly (by dbc_read_dbblk) if we read this block into
				   the same place later.
				*/
				blk_p = blk_set_p->new_buff;
				blk_set_p->new_buff = blk_set_p->old_buff;
				blk_set_p->old_buff = blk_p;
			}
			DBC_DEBUG(("DBC_DEBUG: -- Block index %d being written as block 0x%x\n", blk_index,
				   blk_set_p->blk_num));
			psa->fc->op_buff = blk_set_p->new_buff;
			psa->fc->op_pos = psa->dbc_cs_data->start_vbn
				+ ((gtm_int64_t)(blk_size / DISK_BLOCK_SIZE) * blk_set_p->blk_num);
			dbcertify_dbfilop(psa);
			if (gdsblk_create == blk_set_p->usage)
				psa->blks_created++;
			else
				psa->blks_updated++;
		}
		psa->dbc_critical = FALSE;
		if (forced_exit)
		{	/* Our exit was deferred until we cleared the critical section area */
			UNIX_ONLY(dbcertify_deferred_signal_handler());
			VMS_ONLY(sys$exit(exi_condition));
		}

		/* Update the transaction number in the fileheader for the next transaction */
		psa->dbc_cs_data->trans_hist.curr_tn++;
		psa->dbc_fhdr_dirty = TRUE;
	} else
		GTMASSERT;	/* If we got this far we should have split a block which would create a block */
	DBC_DEBUG(("DBC_DEBUG: Block processing completed\n"));
	psa->blks_processed++;

	return FALSE;	/* No transaction restart necessary */
}

/* Flush the file-header back out to the database */
void dbc_flush_fhead(phase_static_area *psa)
{
	if (!psa->dbc_fhdr_dirty)
		return;		/* Nothing to do if it wasn't dirtied */
	psa->fc->op = FC_WRITE;
	psa->fc->op_buff = (uchar_ptr_t)psa->dbc_cs_data;
	psa->fc->op_pos = 1;
	psa->fc->op_len = SIZEOF(v15_sgmnt_data);
	dbcertify_dbfilop(psa);
	psa->dbc_fhdr_dirty = FALSE;
	return;
}

/* Read the next output record into the buffer provided */
void dbc_read_p1out(phase_static_area *psa, void *obuf, int olen)
{
	int		rc, save_errno;
	char_ptr_t	errmsg;

	DOREADRC(psa->outfd, obuf, olen, rc);
	if (-1 == rc)
	{
		save_errno = errno;
		errmsg = STRERROR(save_errno);
		assert(FALSE);
		rts_error(VARLSTCNT(11) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("read()"), CALLFROM,
			  ERR_TEXT, 2, RTS_ERROR_STRING(errmsg));
	}
}

/* Exit/cleanup routine */
void dbc_certify_phase_cleanup(void)
{
	phase_static_area	*psa;

	psa = psa_gbl;
	if (psa->dbc_gv_cur_region && psa->dbc_gv_cur_region->dyn.addr && psa->dbc_gv_cur_region->dyn.addr->file_cntl)
	{
		dbc_flush_fhead(psa);
		dbc_close_db(psa);
		if (psa->dbc_critical)
			rts_error(VARLSTCNT(4) ERR_TEXT,
				  2, RTS_ERROR_LITERAL("Failure while in critical section -- database damage likely"));
	}
	UNIX_ONLY(dbc_release_standalone_access(psa));
	if (psa_gbl->tmp_file_names_gend)
	{	/* Only close/delete if we know what they are */
		if (psa->tcfp)
			dbc_close_command_file(psa);
		if (!psa->keep_temp_files)
			dbc_remove_command_file(psa);
		if (psa->trfp)
			dbc_close_result_file(psa);
		if (!psa->keep_temp_files)
			dbc_remove_result_file(psa);
	}
}
