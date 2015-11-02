/****************************************************************
 *								*
 *	Copyright 2005, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/****************************************************************
        DBCERTIFY_SCAN_PHASE - Phase 1 scan utility

        - Flush database buffers (call to DSE BUFFER_FLUSH)
        - Open database
        - Verify either report_only or max_lrecl and reserved_bytes within bounds.
        - If not report_only, open output file.
        - Locate too-full blocks and identify type.
        - Write descriptive records to output file if required
        - rewrite header of output file to include total information.
        - print statistics of run to console.

        Note: Most routines in this utility are self-contained
              meaning they do not reference GT.M library routines
              (with some notable exceptions). This is because
              phase-1 is going to run against live V4 databases
              and the V5 compilation will be using V5 database
              structures.
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

#if defined(__MVS__)
#include "gtm_zos_io.h"
#endif
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
#include "gdsblkops.h"
#include "gtmmsg.h"
#include "gtmio.h"
#include "spec_type.h"
#include "get_spec.h"
#include "collseq.h"
#include "format_targ_key.h"
#include "patcode.h"
#include "error.h"
#include "mupip_exit.h"
#include "gvcst_lbm_check.h"
#include "dbcertify.h"

GBLREF	gv_key			*gv_altkey;
GBLREF	gv_namehead		*gv_target;
GBLREF	pattern			*pattern_list;
GBLREF	pattern			*curr_pattern;
GBLREF	pattern			mumps_pattern;
GBLREF	uint4			*pattern_typemask;
GBLREF	phase_static_area	*psa_gbl;

error_def(ERR_FILENAMETOOLONG);
error_def(ERR_DBNOTGDS);
error_def(ERR_BADDBVER);
error_def(ERR_DBMINRESBYTES);
error_def(ERR_DBMAXREC2BIG);
error_def(ERR_DEVOPENFAIL);
error_def(ERR_DBCREC2BIG);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);
error_def(ERR_DBCINTEGERR);
error_def(ERR_COLLATIONUNDEF);
error_def(ERR_COLLTYPVERSION);
error_def(ERR_GVIS);
error_def(ERR_MUPCLIERR);
ZOS_ONLY(error_def(ERR_BADTAG);)

void dbc_write_p1out(phase_static_area *psa, void *obuf, int olen);
void dbc_requeue_block(phase_static_area *psa, block_id blk_num);
void dbc_integ_error(phase_static_area *psa, block_id blk_num, char_ptr_t emsg);
void dbc_process_block(phase_static_area *psa, int blk_num, gtm_off_t dbptr);
uchar_ptr_t dbc_format_key(phase_static_area *psa, uchar_ptr_t rec_p);

/* Phase 1 certification process scans the dstabase */
void dbcertify_scan_phase(void)
{
	int		max_max_rec_size;	/* Maximum value for max record size for given block size */
	int		lm_offset;		/* Local bit map offset */
	int		mm_offset;		/* Master map offset */
	int		save_errno, blk_index;
	ssize_t		rc;
	size_t		len;
	uchar_ptr_t	badfn;
	char_ptr_t	errmsg;
	unsigned char	dbfn[MAX_FN_LEN + 1];
	unsigned short	buff_len;
	gtm_off_t	dbptr;
	boolean_t	outfile_present;
	enum		gdsblk_type blk_type;
	block_id	bitmap_blk_num, last_bitmap_blk_num, blk_num;
	integ_error_blk_list	*iebl;
	phase_static_area *psa;
	ZOS_ONLY(int	realfiletag;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	psa = psa_gbl;
	DBC_DEBUG(("DBC_DEBUG: Beginning scan phase\n"));
	psa->bsu_keys = TRUE;
	UNIX_ONLY(atexit(dbc_scan_phase_cleanup));
	TREF(transform) = TRUE;
	psa->block_depth = psa->block_depth_hwm = -1;		/* Initialize no cache */
	initialize_pattern_table();
	/* On VMS, file operations are in 512 byte chunks which seems to give VMS some fidgies
	   when we rewrite the header later. Make sure the header is in one single 512 byte
	   block.
	*/
	assert(DISK_BLOCK_SIZE == SIZEOF(p1hdr));
	/* Check results of option parse */
	psa->phase_one = TRUE;
	psa->report_only = (CLI_PRESENT == cli_present("REPORT_ONLY"));
	psa->detail = (CLI_PRESENT == cli_present("DETAIL"));
	psa->bsu_keys = !cli_negated("BSU_KEYS");
	if (outfile_present = (CLI_PRESENT == cli_present("OUTFILE")))
	{
		buff_len = SIZEOF(psa->outfn) - 1;
		if (FALSE == cli_get_str("OUTFILE", (char_ptr_t)psa->outfn, &buff_len))
			mupip_exit(ERR_MUPCLIERR);
		psa->outfn[buff_len] = '\0';	/* Not null terminated if max string on UNIX, not at all on VMS */
	}
	if (CLI_PRESENT == cli_present("TEMPFILE_DIR"))
	{	/* Want to put temp files in this directory */
		buff_len = SIZEOF(psa->tmpfiledir) - 1;
		if (FALSE == cli_get_str("TEMPFILE_DIR", (char_ptr_t)psa->tmpfiledir, &buff_len))
			mupip_exit(ERR_MUPCLIERR);
		psa->tmpfiledir[buff_len] = '\0';
	}
	psa->keep_temp_files = (CLI_PRESENT == cli_present("KEEP_TEMPS"));
	buff_len = SIZEOF(psa->regname) - 1;
	if (FALSE == cli_get_str("REGION", (char_ptr_t)psa->regname, &buff_len))
		mupip_exit(ERR_MUPCLIERR);
	psa->regname[buff_len] = '\0';
	/* First order of business is to flush the database in the cache so we start with
	   as current a version as possible.
	*/
	dbc_open_command_file(psa);
#	ifdef VMS
	strcpy((char_ptr_t)psa->util_cmd_buff, RESULT_ASGN);
	strcat((char_ptr_t)psa->util_cmd_buff, (char_ptr_t)psa->tmprsltfile);
	dbc_write_command_file(psa, (char_ptr_t)psa->util_cmd_buff);
	dbc_write_command_file(psa, DSE_START);
#	else
	strcpy((char_ptr_t)psa->util_cmd_buff, DSE_START_PIPE_RSLT1);
	strcat((char_ptr_t)psa->util_cmd_buff, (char_ptr_t)psa->tmprsltfile);
	strcat((char_ptr_t)psa->util_cmd_buff, DSE_START_PIPE_RSLT2);
	dbc_write_command_file(psa, (char_ptr_t)psa->util_cmd_buff);
#	endif
	strcpy((char_ptr_t)psa->util_cmd_buff, DSE_FIND_REG_ALL);
	strcat((char_ptr_t)psa->util_cmd_buff, "=");
	strcat((char_ptr_t)psa->util_cmd_buff, (char_ptr_t)psa->regname);
	dbc_write_command_file(psa, (char_ptr_t)psa->util_cmd_buff);
	dbc_write_command_file(psa, DSE_BFLUSH);
	dbc_write_command_file(psa, DSE_QUIT);
	UNIX_ONLY(dbc_write_command_file(psa, "EOF"));
	dbc_close_command_file(psa);
	dbc_remove_result_file(psa);
	dbc_run_command_file(psa, "DSE", DSE_BFLUSH, TRUE);
	/* Also need to find out what the database name for this region is */
	dbc_find_database_filename(psa, psa->regname, dbfn);
	/* See if a phase-1 output filename was specified. If not, create a default name */
	if (!outfile_present)
	{	/* No output file name specified -- supply a default */
		len = strlen((char_ptr_t)dbfn);
		if (MAX_FN_LEN < (len + SIZEOF(DEFAULT_OUTFILE_SUFFIX) - 1))
		{
			badfn = malloc(len + SIZEOF(DEFAULT_OUTFILE_SUFFIX));
			memcpy(badfn, dbfn, len);
			badfn[len] = 0;
			strcat((char_ptr_t)badfn, DEFAULT_OUTFILE_SUFFIX);
			rts_error(VARLSTCNT(6) ERR_FILENAMETOOLONG, 0, ERR_TEXT, 2, RTS_ERROR_STRING((char_ptr_t)badfn));
		}
		strcpy((char_ptr_t)psa->outfn, (char_ptr_t)dbfn);
		strcat((char_ptr_t)psa->outfn, DEFAULT_OUTFILE_SUFFIX);
	}
	/* Build data structures and open database */
	MALLOC_INIT(psa->dbc_gv_cur_region, SIZEOF(gd_region));
	MALLOC_INIT(psa->dbc_gv_cur_region->dyn.addr, SIZEOF(gd_segment));
	psa->dbc_gv_cur_region->dyn.addr->acc_meth = dba_bg;
	len = strlen((char_ptr_t)dbfn);
	strcpy((char_ptr_t)psa->dbc_gv_cur_region->dyn.addr->fname, (char_ptr_t)dbfn);
	psa->dbc_gv_cur_region->dyn.addr->fname_len = (unsigned short)len;
	FILE_CNTL_INIT(psa->dbc_gv_cur_region->dyn.addr);
	psa->dbc_gv_cur_region->dyn.addr->file_cntl->file_type = dba_bg;
	psa->dbc_cs_data = malloc(SIZEOF(*psa->dbc_cs_data));
	/* Initialize for db processing - open and read in file-header */
	psa->fc = psa->dbc_gv_cur_region->dyn.addr->file_cntl;
	dbc_init_db(psa);
	/* If REPORT_ONLY was *NOT* specified, then we require two things:
	 * 1) The reserved bytes value must be at least 8 (UNIX) or 9 (VMS).
	 * 2) The maximum record size must be < blk_size - 16 to allow for new V5 block header.
	 */
	max_max_rec_size = psa->dbc_cs_data->blk_size - SIZEOF(blk_hdr);
	if (VMS_ONLY(9) UNIX_ONLY(8) > psa->dbc_cs_data->reserved_bytes)
	{
		gtm_putmsg(VARLSTCNT(4) ERR_DBMINRESBYTES, 2, VMS_ONLY(9) UNIX_ONLY(8), psa->dbc_cs_data->reserved_bytes);
		if (!psa->report_only)
			exit(SS_NORMAL - 1);	/* Gives -1 on UNIX (failure) and 0 on VMS (failure) */
	}
	if (psa->dbc_cs_data->max_rec_size > max_max_rec_size)
	{
		gtm_putmsg(VARLSTCNT(5) ERR_DBMAXREC2BIG, 3, psa->dbc_cs_data->max_rec_size, psa->dbc_cs_data->blk_size,
			  max_max_rec_size);
		if (!psa->report_only)
			exit(SS_NORMAL - 1);
	}
	/* If not REPORT_ONLY, open the phase-1 output file and write header info. Note this will be
	 * re-written at the completion of the process.
	 */
	if (!psa->report_only)
	{	/* Recreate the file entirely if it exists */
		psa->outfd = OPEN3((char_ptr_t)psa->outfn, O_WRONLY + O_CREAT + O_TRUNC, S_IRUSR + S_IWUSR RMS_OPEN_BIN);
		if (FD_INVALID == psa->outfd)
		{	/* The following STRERROR() extraction necessary for VMS portability */
			save_errno = errno;
			errmsg = STRERROR(save_errno);
			rts_error(VARLSTCNT(8) ERR_DEVOPENFAIL, 2, RTS_ERROR_STRING((char_ptr_t)psa->outfn),
				  ERR_TEXT, 2, RTS_ERROR_STRING(errmsg));
		}
#	ifdef __MVS__
		if (-1 == gtm_zos_set_tag(psa->outfd, TAG_BINARY, TAG_NOTTEXT, TAG_FORCE, &realfiletag))
			TAG_POLICY_GTM_PUTMSG((char_ptr_t)psa->outfn, errno, realfiletag, TAG_BINARY);
#	endif
		memset((void *)&psa->ofhdr, 0, SIZEOF(p1hdr));
		memcpy(psa->ofhdr.p1hdr_tag, P1HDR_TAG, SIZEOF(psa->ofhdr.p1hdr_tag));
		dbc_write_p1out(psa, &psa->ofhdr, SIZEOF(p1hdr));	/* Initial hdr is all zeroes */
	}
	/* Initialize */
	psa->block_buff = malloc(psa->dbc_cs_data->blk_size);		/* Current data/index block we are looking at */
	psa->curr_lbmap_buff= malloc(psa->dbc_cs_data->blk_size);	/* Current local bit map cache */
	psa->local_bit_map_cnt = (psa->dbc_cs_data->trans_hist.total_blks + psa->dbc_cs_data->bplmap - 1)
		/ psa->dbc_cs_data->bplmap;
	dbptr = (psa->dbc_cs_data->start_vbn - 1) * DISK_BLOCK_SIZE;
	blk_num = 0;
	/* Loop to process every local bit map in the database. Since the flag tells us only
	 * (0) it is full or (1) it is not full, we have to completely process each local bit map.
	 */
	psa->fc->op_len = psa->dbc_cs_data->blk_size;
	for (mm_offset = 0;
	     (mm_offset < psa->local_bit_map_cnt) && (blk_num < psa->dbc_cs_data->trans_hist.total_blks);
	     ++mm_offset)
	{	/* Once through for each master map bit in our database */
		psa->fc->op_buff = psa->curr_lbmap_buff;
		psa->fc->op_pos = (dbptr / DISK_BLOCK_SIZE) + 1;
		dbcertify_dbfilop(psa);		/* Read local bitmap block (no return if error) */
		/* Verification we haven't gotten lost */
		assert(0 == (blk_num % psa->dbc_cs_data->bplmap));
		/* Loop through each local bit map processing (checking) allocated blocks */
		for (lm_offset = 0;
		     (lm_offset < (psa->dbc_cs_data->bplmap * BML_BITS_PER_BLK))
			     && (blk_num < psa->dbc_cs_data->trans_hist.total_blks);
		     lm_offset += BML_BITS_PER_BLK, dbptr += psa->dbc_cs_data->blk_size, blk_num++)
		{
			if (gvcst_blk_is_allocated(psa->curr_lbmap_buff + SIZEOF(v15_blk_hdr), lm_offset))
				/* This block is in use -- process it */
				dbc_process_block(psa, blk_num, dbptr);
			else
			{
				DBC_DEBUG(("DBC_DEBUG: Block 0x%x is NOT allocated -- bypassing\n", blk_num));
				psa->blks_bypassed++;
			}
		}
		if ((BLKS_PER_LMAP * BML_BITS_PER_BLK) > lm_offset)
			DBC_DEBUG(("DBC_DEBUG: Partial lmap processed - blk 0x%x - lm_offset 0x%x\n", \
				   (mm_offset * BLKS_PER_LMAP), lm_offset));
	}
	/* If there were any blocks we had trouble processing the first time through, perform a second buffer flush and
	 * retry them. If they are still broken, it is an error this time. Note all integ errors are fatal but record-too-long
	 * errors only cause the run to turn into a "report_only" type run and thus do not create the output file but
	 * scanning continues.
	 */
	if (NULL != psa->iebl)
	{
		DBC_DEBUG(("DBC_DEBUG: Entering block re-processing loop\n"));
		psa->final = TRUE;
		last_bitmap_blk_num = -1;
		for (iebl = psa->iebl; iebl; iebl = iebl->next)
			for (blk_index = 0; blk_index < iebl->blk_cnt; blk_index++)
			{	/* For each previously broken block. First see if they are still allocated */
				blk_num = iebl->blk_list[blk_index];
				assert(blk_num);
				bitmap_blk_num = ROUND_DOWN2(blk_num, psa->dbc_cs_data->bplmap);
				/* Read bitmap in if it isn't already in our buffer */
				if (bitmap_blk_num != last_bitmap_blk_num)
				{
					psa->fc->op_buff = psa->curr_lbmap_buff;
					dbptr = ((psa->dbc_cs_data->start_vbn - 1) * DISK_BLOCK_SIZE)
						+ psa->dbc_cs_data->free_space
						+ ((gtm_off_t)psa->dbc_cs_data->blk_size * bitmap_blk_num);
					psa->fc->op_pos = (dbptr / DISK_BLOCK_SIZE) + 1;
					dbcertify_dbfilop(psa);		/* Read local bitmap block (no return if error) */
					last_bitmap_blk_num = bitmap_blk_num;
				}
				lm_offset = (blk_num - bitmap_blk_num) * 2;
				dbptr = ((psa->dbc_cs_data->start_vbn - 1) * DISK_BLOCK_SIZE) + psa->dbc_cs_data->free_space
					+ ((gtm_off_t)psa->dbc_cs_data->blk_size * blk_num);
				if (gvcst_blk_is_allocated(psa->curr_lbmap_buff + SIZEOF(v15_blk_hdr), lm_offset))
					/* This block is in use -- process it */
					dbc_process_block(psa, blk_num, dbptr);
				else
				{
					DBC_DEBUG(("DBC_DEBUG: Bypassing block 0x%x because no longer allocated\n", blk_num));
					psa->blks_bypassed++;
				}
			}
		DBC_DEBUG(("DBC_DEBUG: Block reprocessing complete\n"));
	}
	/* Now, update the fields in the output file's fileheader if we are writing it */
	if (!psa->report_only)
	{
		rc = lseek(psa->outfd, (ssize_t)0, SEEK_SET);
		if (-1 == rc)
		{
			save_errno = errno;
			errmsg = STRERROR(save_errno);
			rts_error(VARLSTCNT(11) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("lseek()"), CALLFROM,
				  ERR_TEXT, 2, RTS_ERROR_STRING(errmsg));
		}
		psa->ofhdr.tn = psa->dbc_cs_data->trans_hist.curr_tn;
		psa->ofhdr.blk_count = psa->blks_too_big;
		psa->ofhdr.tot_blocks = psa->blks_processed;
		psa->ofhdr.dt_leaf_cnt = psa->dtlvl0;
		psa->ofhdr.dt_index_cnt = psa->dtlvln0;
		psa->ofhdr.gvt_leaf_cnt = psa->gvtlvl0;
		psa->ofhdr.gvt_index_cnt = psa->gvtlvln0;
		psa->ofhdr.uid_len = SIZEOF(unique_file_id);	/* Size used by v5cbsu since this field len varies by platform */
		memcpy(&psa->ofhdr.unique_id, &FILE_INFO(psa->dbc_gv_cur_region)->UNIX_ONLY(fileid)VMS_ONLY(file_id),
		       SIZEOF(unique_file_id));
		assert(SIZEOF(psa->ofhdr.regname) > strlen((char_ptr_t)psa->regname));
		strcpy((char_ptr_t)psa->ofhdr.regname, (char_ptr_t)psa->regname);
		assert(SIZEOF(psa->ofhdr.dbfn) > strlen((char_ptr_t)psa->dbc_gv_cur_region->dyn.addr->fname));
		strcpy((char_ptr_t)psa->ofhdr.dbfn, (char_ptr_t)psa->dbc_gv_cur_region->dyn.addr->fname);
		dbc_write_p1out(psa, &psa->ofhdr, SIZEOF(p1hdr));	/* Rewrite populated output file header */
		CLOSEFILE_RESET(psa->outfd, rc);		/* Close output file; Resets "psa->outfd" to FD_INVALID */
		if (-1 == rc)
		{
			save_errno = errno;
			errmsg = STRERROR(save_errno);
			rts_error(VARLSTCNT(11) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("close()"), CALLFROM,
				  ERR_TEXT, 2, RTS_ERROR_STRING(errmsg));
		}
	}
	psa->fc->op = FC_CLOSE;
	dbcertify_dbfilop(psa);		/* Close database */
	PRINTF("\n\n");
	PRINTF("Total blocks in database  -------   %12d [0x%08x]\n", psa->dbc_cs_data->trans_hist.total_blks,
	       psa->dbc_cs_data->trans_hist.total_blks);
	PRINTF("Total local bitmap blocks -------   %12d [0x%08x]\n", psa->local_bit_map_cnt, psa->local_bit_map_cnt);
	PRINTF("Blocks bypassed -----------------   %12d [0x%08x]\n", psa->blks_bypassed, psa->blks_bypassed);
	PRINTF("Blocks processed ----------------   %12d [0x%08x]\n", psa->blks_processed, psa->blks_processed);
	PRINTF("Blocks needing to be split ------   %12d [0x%08x]\n", psa->blks_too_big, psa->blks_too_big);
	PRINTF("- DT leaf (data) blocks ---------   %12d [0x%08x]\n", psa->dtlvl0, psa->dtlvl0);
	PRINTF("- DT index blocks ---------------   %12d [0x%08x]\n", psa->dtlvln0, psa->dtlvln0);
	PRINTF("- GVT leaf (data) blocks --------   %12d [0x%08x]\n", psa->gvtlvl0, psa->gvtlvl0);
	PRINTF("- GVT index blocks --------------   %12d [0x%08x]\n", psa->gvtlvln0, psa->gvtlvln0);
	/* Release resources */
	free(psa->dbc_cs_data);
#	ifdef VMS
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
#	endif
	free(psa->dbc_gv_cur_region->dyn.addr->file_cntl->file_info);
	free(psa->dbc_gv_cur_region->dyn.addr->file_cntl);
	free(psa->dbc_gv_cur_region->dyn.addr);
	free(psa->dbc_gv_cur_region);
	free(psa->curr_lbmap_buff);
	free(psa->block_buff);
	for (; psa->iebl; )
	{
		iebl = psa->iebl->next;
		free(psa->iebl);
		psa->iebl = iebl;
	}
	free(psa);
}

/* Write the given output record but keep the global variable "chksum" updated with the tally.
 * Assumes that all writes are from aligned buffers (regardless of how they end up on disk).
 */
void dbc_write_p1out(phase_static_area *psa, void *obuf, int olen)
{
	int		save_errno;
	ssize_t		rc;
	char_ptr_t	errmsg;

	DBC_DEBUG(("DBC_DEBUG: Output %d bytes to dbcertscan output file\n", olen));
	rc = write(psa->outfd, obuf, olen);
	if (-1 == rc)
	{
		save_errno = errno;
		errmsg = STRERROR(save_errno);
		rts_error(VARLSTCNT(11) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("close()"), CALLFROM,
			  ERR_TEXT, 2, RTS_ERROR_STRING(errmsg));
	}
}

/* Routine to process a database block */
void dbc_process_block(phase_static_area *psa, int blk_num, gtm_off_t dbptr)
{
	int		rec_len, rec1_cmpc, rec2_cmpc, key_len, blk_levl, rec1_len, rec2_len, rec2_rlen;
	int		tmp_cmpc;
	int		free_bytes, blk_len;
	int		save_errno, mm_offset;
	ssize_t		rc;
	size_t		len, rec1_gvn_len;
	boolean_t	have_dt_blk;
	unsigned short	us_rec_len;
	char_ptr_t	errmsg, key_pfx;
	uchar_ptr_t	rec_ptr, rec1_ptr, rec2_ptr, key_ptr;
	enum gdsblk_type blk_type;

	DBC_DEBUG(("DBC_DEBUG: Block 0x%x is allocated -- processing\n", blk_num));
	psa->fc->op_buff = psa->block_buff;
	psa->fc->op_pos = (dbptr / DISK_BLOCK_SIZE) + 1;
	dbcertify_dbfilop(psa);		/* Read data/index block (no return if error) */
	/* Check free space in block */
	blk_len = ((v15_blk_hdr_ptr_t)psa->block_buff)->bsiz;
	free_bytes = psa->dbc_cs_data->blk_size - blk_len;
	if (UNIX_ONLY(8) VMS_ONLY(9) > free_bytes)
	{
		blk_levl = ((v15_blk_hdr_ptr_t)psa->block_buff)->levl;
		if (MAX_BT_DEPTH <= blk_levl)
			dbc_integ_error(psa, blk_num, "Bad block level");
		/* Isolate first record for length check */
		rec1_ptr = psa->block_buff + SIZEOF(v15_blk_hdr);
		rec1_cmpc = EVAL_CMPC((rec_hdr_ptr_t)rec1_ptr);
		if (0 != rec1_cmpc)
			dbc_integ_error(psa, blk_num, "Bad compression count");
		GET_USHORT(us_rec_len, &((rec_hdr_ptr_t)rec1_ptr)->rsiz);
		rec1_len = us_rec_len;
		if ((rec1_len + SIZEOF(v15_blk_hdr)) < blk_len)
		{	/* There is a 2nd record. It must also be checked as it is possible for a
			 * too-long record to exist as the 2nd record if it is a near clone of the
			 * first record (differing only in the last byte of the key) and the first
			 * record has no value (null value).
			 */
			rec2_ptr = rec1_ptr + rec1_len;
			rec2_cmpc = EVAL_CMPC((rec_hdr_ptr_t)rec2_ptr);
			if (rec2_cmpc > rec1_len)
				dbc_integ_error(psa, blk_num, "Compression count too large");
			GET_USHORT(us_rec_len, &((rec_hdr_ptr_t)rec2_ptr)->rsiz);
			rec2_len = us_rec_len;
			rec2_rlen = rec2_len + rec2_cmpc;
		} else
		{
			rec2_len = rec2_rlen = 0;	/* There is no second record */
			assert(0 == blk_levl);		/* And thus this is supposed to be lvl0 data block */
		}
		if (rec1_len > psa->dbc_cs_data->max_rec_size)
		{	/* First record exceeds maximum size */
			rec_len = rec1_len;
			rec_ptr = rec1_ptr;
		} else if (rec2_rlen > psa->dbc_cs_data->max_rec_size)
		{	/* Second record exceeds maximum size */
			rec_len = rec2_rlen;
			rec_ptr = rec2_ptr;
		} else
			/* All is well .. set flag record length is ok */
			rec_len = 0;
		if (rec_len)
		{	/* One of these records exceeds the max size - might be a transitory integ on
			 * 1st pass or permanent on 2nd pass.
			 */
			assert(rec_ptr);
			if (psa->final)
			{	/* Should be a data block with a too-long record in it */
				assert(0 == ((v15_blk_hdr_ptr_t)psa->block_buff)->levl);
				psa->gvtlvl0++;
				key_ptr = dbc_format_key(psa, rec_ptr);	/* Text representation of the key */
				gtm_putmsg(VARLSTCNT(9) ERR_DBCREC2BIG, 7,
					   RTS_ERROR_STRING((char_ptr_t)key_ptr), rec_len,
					   blk_num, psa->dbc_cs_data->max_rec_size,
					   psa->dbc_gv_cur_region->dyn.addr->fname_len,
					   psa->dbc_gv_cur_region->dyn.addr->fname);
				if (!psa->report_only)
				{
					CLOSEFILE_RESET(psa->outfd, rc); /* Close output file; Resets "psa->outfd" to FD_INVALID */
					if (-1 == rc)
					{
						save_errno = errno;
						errmsg = STRERROR(save_errno);
						rts_error(VARLSTCNT(11) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("close()"), CALLFROM,
							  ERR_TEXT, 2, RTS_ERROR_STRING(errmsg));
					}
					if (-1 == remove((char_ptr_t)psa->outfn))
					{	/* Delete bogus output file */
						save_errno = errno;
						errmsg = STRERROR(save_errno);
						rts_error(VARLSTCNT(11) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("remove()"), CALLFROM,
							  ERR_TEXT, 2, RTS_ERROR_STRING(errmsg));
					}
					psa->report_only = TRUE; /* No more writing to output file */
				}
			} else	/* Not our final trip through, cause the block to be requeued for later processing */
				dbc_requeue_block(psa, blk_num);
			return;
		}
		/* Determine type of block (DT lvl 0, DT lvl !0, GVT lvl 0, GVT lvl !0)
		 * Rules for checking:
		 * 1) If compression count of 2nd record is zero, it *must* be a directory tree block. This is a fast path
		 *    check to avoid doing the strlen in the second check.
		 * 2) If compression count of second record is less than or equal to the length of the global variable name,
		 *    then this must be a directory tree block. The reason this check works is a GVT index or data block
		 *    would have same GVN in the 2nd record as the first so the compression count would be a minimum of
		 *    (length(GVN) + 1). The "+ 1" is for the terminating null of the GVN.
		 * 3) If there is no second record, this must be a lvl0 data tree block as no other block could exceed
		 *    the maximum block size with a single record.
		 */
		have_dt_blk = FALSE;
		if (0 != rec2_len)
		{	/* There is a second record .. */
			if (0 == rec2_cmpc)
				have_dt_blk = TRUE;	/* Only DT records have non-zero cmpc in 2nd record */
			else
			{
				rec1_gvn_len = strlen((char_ptr_t)rec1_ptr + SIZEOF(rec_hdr));
				if (rec2_cmpc <= rec1_gvn_len)
					have_dt_blk = TRUE;
			}
			if (have_dt_blk)
			{
				if (0 == blk_levl)
				{
					psa->dtlvl0++;
					blk_type = gdsblk_dtleaf;
				} else
				{
					psa->dtlvln0++;
					if (1 != blk_num)	/* Quick root block check */
						blk_type = gdsblk_dtindex;
					else
						blk_type = gdsblk_dtroot;
				}
			} else
			{
				if (0 == blk_levl)
				{
					psa->gvtlvl0++;
					blk_type = gdsblk_gvtleaf;
				} else
				{
					psa->gvtlvln0++;
					/* Note from the master map, we cannot tell if this is a gvtroot
					   block or not so just mark it as an index block for now. It will
					   be noted as a root block when read in by stage 2.
					*/
					blk_type = gdsblk_gvtindex;
				}
			}
		} else
		{	/* There was no second record. This record must be a lvl 0 data block */
			psa->gvtlvl0++;
			blk_type = gdsblk_gvtleaf;
		}
		if (psa->bsu_keys && gdsblk_gvtleaf == blk_type)
		{	/* Get text representation of the key. Because we are not using the DT in the cache, there is
			 * a possibility that we cannot find this global however that chance should be slight. Not finding
			 * it only means that this record cannot be processed by v5cbsu.m and will instead have to be
			 * processed by phase-2. This is an acceptable alternative.
			 *
			 * The key we format is the second record in the block if it is available. This is because of the
			 * way an update in place takes place. If the block is too big, the block is split at the point
			 * of the update. If the (somewhat long) keys of the first and second records differ by only 1
			 * byte and the first record has no value, we could get into a situation where the second record
			 * is highly compressed and a split at that point will give back insufficient space for the
			 * new block to meet the block size requirements. If instead we split the record after the 2nd
			 * record in this situation, the resulting blocks will be sized sufficiently to meet the upgrade
			 * requirements.
			 */
			if (rec2_len)
			{	/* There is a 2nd record */
				rec_ptr = rec2_ptr;
				key_pfx = "[2] ";
			} else
			{	/* No 2nd record, format 1st record instead */
				rec_ptr = rec1_ptr;
				key_pfx = "[1] ";
			}
			key_ptr = dbc_format_key(psa, (rec2_len ? rec2_ptr : rec1_ptr));
		} else
		{
			key_ptr = (uchar_ptr_t)"";
			key_pfx = "";
		}
		if (psa->detail)
		{
			if (0 == psa->blks_too_big)
			{	/* Print header */
				PRINTF("\nProblem blocks (max block size = %d):\n\n", psa->dbc_cs_data->blk_size);
				PRINTF("       Blknum           Offset  Blktype  BlkLvl   Blksize   Free"
				       "   Key\n");
			}
#			ifndef __osf__
			GTM64_ONLY(PRINTF("   0x%08x %16lx %s %5d   %9d %6d   %s%s\n", blk_num, dbptr,
			       (have_dt_blk ? "   DT   " : "   GVT  "),
			       ((v15_blk_hdr_ptr_t)psa->block_buff)->levl,
			       ((v15_blk_hdr_ptr_t)psa->block_buff)->bsiz, free_bytes, key_pfx, key_ptr));
			NON_GTM64_ONLY(PRINTF("   0x%08x %16llx %s %5d   %9d %6d   %s%s\n", blk_num, dbptr,
			       (have_dt_blk ? "   DT   " : "   GVT  "),
			       ((v15_blk_hdr_ptr_t)psa->block_buff)->levl,
			       ((v15_blk_hdr_ptr_t)psa->block_buff)->bsiz, free_bytes, key_pfx, key_ptr));
			VMS_ONLY(PRINTF("   0x%08x %16llx %s %5d   %9d %6d   %s%s\n", blk_num, dbptr,
			       (have_dt_blk ? "   DT   " : "   GVT  "),
			       ((v15_blk_hdr_ptr_t)psa->block_buff)->levl,
			       ((v15_blk_hdr_ptr_t)psa->block_buff)->bsiz, free_bytes, key_pfx, key_ptr));
#			else
			PRINTF("   0x%08x %16lx %s %5d   %9d %6d\n   %s%s", blk_num, dbptr,
			       (have_dt_blk ? "   DT   " : "   GVT  "),
			       ((v15_blk_hdr_ptr_t)psa->block_buff)->levl,
			       ((v15_blk_hdr_ptr_t)psa->block_buff)->bsiz, free_bytes, key_pfx, key_ptr);
#			endif
		}
		psa->blks_too_big++;
		/* Prepare the record to put to the output file */
		if (!psa->report_only)
		{
			GET_LONG(psa->rhdr.tn, &((v15_blk_hdr_ptr_t)psa->block_buff)->tn);
			psa->rhdr.blk_num = blk_num;
			psa->rhdr.blk_type = blk_type;
			psa->rhdr.blk_levl = blk_levl;
			if (psa->bsu_keys && gdsblk_gvtleaf == blk_type)
				psa->rhdr.akey_len = key_len = STRLEN((char_ptr_t)key_ptr);
			else
				psa->rhdr.akey_len = 0;
			dbc_write_p1out(psa, &psa->rhdr, SIZEOF(p1rec));
			if (psa->bsu_keys && gdsblk_gvtleaf == blk_type)
				dbc_write_p1out(psa, key_ptr, key_len);
		}
	}
	psa->blks_processed++;
}

/* Requeue a block for later processing after a flush to verify we have a problem or not */
void dbc_requeue_block(phase_static_area *psa, block_id blk_num)
{
	integ_error_blk_list	*iebl;

	if (NULL == psa->iebl || MAX_IEB_CNT <= (psa->iebl->blk_cnt + 1))
	{	/* Need a new/another block to hold error blocks */
		DBC_DEBUG(("DBC_DEBUG: Allocating new iebl struct\n"));
		iebl = malloc(SIZEOF(integ_error_blk_list));
		iebl->next = psa->iebl;
		iebl->blk_cnt = 0;
		psa->iebl = iebl;
	}
	DBC_DEBUG(("DBC_DEBUG: Requeuing block 0x%x for later processing\n", blk_num))
	psa->iebl->blk_list[psa->iebl->blk_cnt++] = blk_num;
}

/* Routine to handle integrity errors. If first pass (not final), requeue the block. If on
 * final pass, give integrity rts_error.
 */
void dbc_integ_error(phase_static_area *psa, block_id blk_num, char_ptr_t emsg)
{
	char_ptr_t	errmsg;
	unsigned char	intgerrmsg[256];
	int		save_errno;
	ssize_t		rc;
	size_t		len;

	if (!psa->final)
		dbc_requeue_block(psa, blk_num);
	else
	{	/* Give integrity error message */
		if (!psa->report_only)
		{
			CLOSEFILE_RESET(psa->outfd, rc);	/* Close output file; Resets "psa->outfd" to FD_INVALID */
			if (-1 == rc)
			{
				save_errno = errno;
				errmsg = STRERROR(save_errno);
				rts_error(VARLSTCNT(11) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("close()"), CALLFROM,
					  ERR_TEXT, 2, RTS_ERROR_STRING(errmsg));
			}
			if (-1 == remove((char_ptr_t)psa->outfn))
			{	/* Delete bogus output file */
				save_errno = errno;
				errmsg = STRERROR(save_errno);
				rts_error(VARLSTCNT(11) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("remove()"), CALLFROM,
					  ERR_TEXT, 2, RTS_ERROR_STRING(errmsg));
			}
			psa->report_only = TRUE; /* No more writing to output file */
		}
		intgerrmsg[0] = 0;
		strcpy((char_ptr_t)intgerrmsg, emsg);
		strcat((char_ptr_t)intgerrmsg, " in block 0x");
		len = strlen((char_ptr_t)intgerrmsg);
		i2hex(blk_num, &intgerrmsg[len], 8);
		intgerrmsg[len + 8] = 0;
		rts_error(VARLSTCNT(8) ERR_DBCINTEGERR, 2, RTS_ERROR_STRING((char_ptr_t)psa->ofhdr.dbfn),
			  ERR_TEXT, 2, RTS_ERROR_STRING((char_ptr_t)intgerrmsg));
	}
}

/* Generate an ascii representation of the given key in the current block buffer.
 * This is accomplished (mainly) by:
 * 1) Locating the key within the record.
 * 2) Calling the dbc_find_dtblk() routine to locate the directory entry for us.
 * 3) Setting up gv_target and friends to point to the entry.
 * 4) Checking the located directory entry for collation information.
 * 5) Calling format_targ_key() to do the formatting into our buffer.
 * Note: usage of "first_rec_key" is somewhat overloaded in this routine. Under most
 *       circumstances, it is most likely the second key that is being formatted but
 *	 this is a defined area that is available for use in this (scan phase) routine
 *	 so we use it.
 */
uchar_ptr_t dbc_format_key(phase_static_area *psa, uchar_ptr_t trec_p)
{
	int		dtblk_index, hdr_len, rec_value_len, rec_len, rec_cmpc;
	int		tmp_cmpc;
	size_t		len;
	uchar_ptr_t	blk_p, rec_value_p, subrec_p, key_end_p, rec_p;
	block_info	*blk_set_p;
	unsigned short	us_rec_len;

	dbc_init_key(psa, &psa->first_rec_key);
	/* We have to parse the block down to the supplied key to make sure the compressed portions
	 * of the key are available.
	 */
	rec_p = psa->block_buff + SIZEOF(v15_blk_hdr);
	while (rec_p < trec_p)
	{
		dbc_find_key(psa, psa->first_rec_key, rec_p, 0);
		GET_USHORT(us_rec_len, &((rec_hdr_ptr_t)rec_p)->rsiz);
		rec_p += us_rec_len;
	}
	assert(rec_p == trec_p);
	dbc_find_key(psa, psa->first_rec_key, trec_p, 0);
	psa->first_rec_key->gvn_len = USTRLEN((char_ptr_t)psa->first_rec_key->base);	/* The GVN we need to lookup in the DT */
	assert(0 < psa->first_rec_key->gvn_len);
	psa->block_depth = -1;	/* Reset to beginning each pass */
	dtblk_index = dbc_find_dtblk(psa, psa->first_rec_key, 0);
	if (0 > dtblk_index)
	{	/* Couldn't find the GVN in the DT. Tiz possible but rare (concurrency issues) and of no major consequence. */
		assert(FALSE);
		return NULL;
	}
	blk_set_p = &psa->blk_set[dtblk_index];
	blk_p = blk_set_p->old_buff;
	assert(0 == ((v15_blk_hdr_ptr_t)blk_p)->levl);
	rec_cmpc = EVAL_CMPC((rec_hdr *)blk_set_p->curr_rec);
	rec_value_p = (blk_set_p->curr_rec + SIZEOF(rec_hdr) + blk_set_p->curr_blk_key->end + 1 - rec_cmpc);
	/* Verify that the dt record we found is the exact one we were looking for */
	if ((psa->first_rec_key->gvn_len + 1) != blk_set_p->curr_blk_key->end)
		/* Some concurrency issues no doubt.. */
		return NULL;
	if (0 != memcmp(psa->first_rec_key->base, blk_set_p->curr_blk_key->base, blk_set_p->curr_blk_key->end))
		return NULL;
	/* Create gv_target if necessary */
	if (NULL == gv_target)
	{
		gv_target = malloc(SIZEOF(gv_namehead) + psa->dbc_cs_data->max_key_size);
		gv_target->clue.prev = 0;
		gv_target->clue.top = psa->first_rec_key->top;
	}
	/* Copy our key to gv_target->clue since dbc_gv_key is somewhat different */
	gv_target->clue.end = psa->first_rec_key->end;
	memcpy(gv_target->clue.base, psa->first_rec_key->base, psa->first_rec_key->end + 1);
	/* Figure out collation for this global */
	GET_USHORT(us_rec_len, &((rec_hdr *)blk_set_p->curr_rec)->rsiz);
	rec_len = us_rec_len;
	rec_value_len = (int)(rec_len - (rec_value_p - blk_set_p->curr_rec));
	if (SIZEOF(block_id) < rec_value_len)
	{	/* This global potentially has collation data in its record (taken from gvcst_root_search()) */
		subrec_p = get_spec(rec_value_p + SIZEOF(block_id), (int)(rec_value_len - SIZEOF(block_id)), COLL_SPEC);
		if (subrec_p)
		{
			gv_target->nct = *(subrec_p + COLL_NCT_OFFSET);
			gv_target->act = *(subrec_p + COLL_ACT_OFFSET);
			gv_target->ver = *(subrec_p + COLL_VER_OFFSET);
		} else
		{
			gv_target->nct = 0;
			gv_target->act = 0;
			gv_target->ver = 0;
		}
	} else
	{
		gv_target->nct = 0;
		gv_target->act = psa->dbc_cs_data->def_coll;
		gv_target->ver = psa->dbc_cs_data->def_coll_ver;
	}
	/* If there was any collation data involved, make sure the routines are available */
	if (gv_target->act)
	{	/* Need to setup gv_altkey in case of errors (contains gvn) */
		if (NULL == gv_altkey)
		{
			gv_altkey = malloc(SIZEOF(gv_key) + psa->dbc_cs_data->max_key_size);
			gv_altkey->prev = 0;
			gv_altkey->top = psa->first_rec_key->top;
		}
		gv_altkey->end = psa->first_rec_key->gvn_len + 1;
		memcpy(gv_altkey->base, psa->first_rec_key->base, psa->first_rec_key->gvn_len + 1);
		act_in_gvt();
	}
	assert(gv_target->act || NULL == gv_target->collseq);
	/* Format the resulting key into the result buffer which is sized appropriately for this task */
	key_end_p = format_targ_key(psa->rslt_buff, SIZEOF(psa->rslt_buff), &gv_target->clue, TRUE);
	*key_end_p = 0;
	return psa->rslt_buff;
}

/* Simple cleanup routine for this scan phaase */
void dbc_scan_phase_cleanup(void)
{
	/* Cleanup our temporary files */
	if (psa_gbl->tmp_file_names_gend)
	{	/* only close/delete if we know what they are */
		if (psa_gbl->tcfp)
			dbc_close_command_file(psa_gbl);
		if (!psa_gbl->keep_temp_files)
			dbc_remove_command_file(psa_gbl);
		if (psa_gbl->trfp)
			dbc_close_result_file(psa_gbl);
		if (!psa_gbl->keep_temp_files)
			dbc_remove_result_file(psa_gbl);
	}
}
